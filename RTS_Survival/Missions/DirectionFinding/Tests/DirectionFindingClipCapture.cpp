// Copyright (C) Bas Blokzijl - All rights reserved.

// -DirectionFindingClip on Ger_OderWarthe: films the tactical map for the Direction Finding field manual. It presses
// Start, skips the arrival tour (holds its skip prompt like the player), opens the tactical map and then saves one
// screenshot with the UI per frame to Saved/DirectionFindingClip/Frames while it takes DF 1, sends an attack order,
// takes DF 2 and sends another. Run it with a fixed frame time (-benchmark -fps=24) and a known command post
// (-DirectionFindingCommandPost=1, Werk 2), see Source/Tools/InfoTakeover/DirectionFinding/capture_clip.ps1. Logs
// DF_CLIP lines (the frame of every event) and quits. Behind RTS_WITH_MAP_TESTS.

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/DelayedAutoRegister.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UObject/UObjectIterator.h"
#include "UnrealClient.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "RTS_Survival/GameUI/HoldToConfirm/W_HoldToSkip.h"
#include "RTS_Survival/GameUI/MainGameUI.h"
#include "RTS_Survival/Missions/DirectionFinding/DirectionFindingManager.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/GrenadeComponent/Tests/GrenadeTestHelpers.h"
#include "RTS_Survival/SupplyPoints/SupplyPoint.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSDirectionFindingClip, Display, All);

namespace DirectionFindingClipPrivate
{
	constexpr float SettleSeconds = 2.f;
	constexpr float MapOpenSeconds = 1.5f;
	constexpr float GiveUpSeconds = 120.f;
	// Frames of the recording (24 per second with -fps=24) at which things happen; the last one ends it.
	constexpr int32 FirstStationFrame = 24;
	constexpr int32 FirstBurstFrame = 72;
	constexpr int32 SecondStationFrame = 132;
	constexpr int32 SecondBurstFrame = 180;
	constexpr int32 EndFrame = 240;

	UWorld* FindGameWorld()
	{
		if (GEngine == nullptr)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* const World = Context.World();
			if (IsValid(World) && World->IsGameWorld() && World->HasBegunPlay())
			{
				return World;
			}
		}
		return nullptr;
	}

	enum class EPhase : uint8
	{
		WaitingForHunt,
		OpeningMap,
		Recording,
		Done
	};
}

/** @brief Drives the clip from a core ticker, so nothing in the game has to know about it. */
class FDirectionFindingClipCapture
{
public:
	bool Tick()
	{
		using namespace DirectionFindingClipPrivate;
		UWorld* const World = FindGameWorld();
		if (not IsValid(World))
		{
			return true;
		}
		ACPPController* const PlayerController = Cast<ACPPController>(World->GetFirstPlayerController());
		FGrenadeTestHelpers::KeepGameRunning(*World, PlayerController);
		PressStartIfWaiting(PlayerController);
		if (World->GetRealTimeSeconds() > GiveUpSeconds && M_Phase != EPhase::Recording)
		{
			Finish(TEXT("gave up waiting"));
			return false;
		}
		switch (M_Phase)
		{
		case EPhase::WaitingForHunt:
			TickWaitingForHunt(*World, PlayerController);
			break;
		case EPhase::OpeningMap:
			TickOpeningMap(*World);
			break;
		case EPhase::Recording:
			TickRecording();
			break;
		case EPhase::Done:
			return false;
		}
		return M_Phase != EPhase::Done;
	}

private:
	DirectionFindingClipPrivate::EPhase M_Phase = DirectionFindingClipPrivate::EPhase::WaitingForHunt;
	TWeakObjectPtr<ADirectionFindingManager> M_Manager;
	double M_PhaseStartSeconds = -1.0;
	int32 M_Frame = 0;

	static void PressStartIfWaiting(ACPPController* PlayerController)
	{
		UPlayerStartGameControl* const StartGameControl = IsValid(PlayerController)
			                                                  ? PlayerController->FindComponentByClass<
				                                                  UPlayerStartGameControl>()
			                                                  : nullptr;
		if (IsValid(StartGameControl) && StartGameControl->GetIsWaitingForPlayerToStart())
		{
			StartGameControl->PlayerStartedGame();
		}
	}

	double GetPhaseSeconds(const UWorld& World)
	{
		if (M_PhaseStartSeconds < 0.0)
		{
			M_PhaseStartSeconds = World.GetRealTimeSeconds();
		}
		return World.GetRealTimeSeconds() - M_PhaseStartSeconds;
	}

	void EnterPhase(const DirectionFindingClipPrivate::EPhase NewPhase)
	{
		M_Phase = NewPhase;
		M_PhaseStartSeconds = -1.0;
	}

	void Finish(const TCHAR* Reason)
	{
		UE_LOG(LogRTSDirectionFindingClip, Display, TEXT("DF_CLIP done frames=%d (%s)"), M_Frame, Reason);
		EnterPhase(DirectionFindingClipPrivate::EPhase::Done);
		FPlatformMisc::RequestExit(false);
	}

	/** Holds Space on the arrival tour's skip prompt, as the player would. @return True while a prompt was found. */
	static bool SkipCinematics(const UWorld& World)
	{
		bool bFoundPrompt = false;
		for (TObjectIterator<UW_HoldToSkip> It; It; ++It)
		{
			if (It->GetWorld() == &World && It->IsInViewport())
			{
				It->StartHold();
				bFoundPrompt = true;
			}
		}
		return bFoundPrompt;
	}

	void TickWaitingForHunt(UWorld& World, const ACPPController* PlayerController)
	{
		const bool bSkipping = SkipCinematics(World);
		ADirectionFindingManager* const Manager = ADirectionFindingManager::FindDirectionFindingManager(&World);
		const bool bScreenFree = IsValid(PlayerController) && PlayerController->GetCanStartScreenTakeover();
		if (bSkipping || not bScreenFree || not IsValid(Manager) || not Manager->GetHasStarted())
		{
			M_PhaseStartSeconds = -1.0;
			return;
		}
		if (GetPhaseSeconds(World) < DirectionFindingClipPrivate::SettleSeconds)
		{
			return;
		}
		M_Manager = Manager;
		UMainGameUI* const MainGameUI = FRTS_Statics::GetMainGameUI(&World);
		if (not IsValid(MainGameUI))
		{
			Finish(TEXT("no main game UI"));
			return;
		}
		MainGameUI->OpenFullMapView();
		UE_LOG(LogRTSDirectionFindingClip, Display, TEXT("DF_CLIP map open, command post werk=%d"),
		       Manager->GetCommandPostWerkIndex());
		EnterPhase(DirectionFindingClipPrivate::EPhase::OpeningMap);
	}

	void TickOpeningMap(const UWorld& World)
	{
		if (GetPhaseSeconds(World) >= DirectionFindingClipPrivate::MapOpenSeconds)
		{
			EnterPhase(DirectionFindingClipPrivate::EPhase::Recording);
		}
	}

	void TickRecording()
	{
		using namespace DirectionFindingClipPrivate;
		RunEventsOfFrame();
		const FString FileName = FPaths::ProjectSavedDir() / FString::Printf(
			TEXT("DirectionFindingClip/Frames/frame_%04d.png"), M_Frame);
		FScreenshotRequest::RequestScreenshot(FileName, true, false);
		++M_Frame;
		if (M_Frame > EndFrame)
		{
			Finish(TEXT("recorded"));
		}
	}

	void RunEventsOfFrame() const
	{
		using namespace DirectionFindingClipPrivate;
		ADirectionFindingManager* const Manager = M_Manager.Get();
		if (not IsValid(Manager))
		{
			return;
		}
		if (M_Frame == FirstStationFrame || M_Frame == SecondStationFrame)
		{
			const int32 StationIndex = M_Frame == FirstStationFrame ? 0 : 1;
			if (Manager->GetStations().IsValidIndex(StationIndex) && IsValid(Manager->GetStations()[StationIndex]))
			{
				Manager->GetStations()[StationIndex]->ForceCaptureByPlayer(RTSPlayers::HumanPlayer);
			}
			UE_LOG(LogRTSDirectionFindingClip, Display, TEXT("DF_CLIP frame=%d station DF %d taken"), M_Frame,
			       StationIndex + 1);
		}
		if (M_Frame == FirstBurstFrame || M_Frame == SecondBurstFrame)
		{
			Manager->ForceBurst(EDirectionFindingBurst::AttackOrder);
			UE_LOG(LogRTSDirectionFindingClip, Display, TEXT("DF_CLIP frame=%d attack order"), M_Frame);
		}
	}
};

static FDelayedAutoRegisterHelper GRegisterDirectionFindingClipCapture(EDelayedRegisterRunPhase::EndOfEngineInit, []()
{
	if (not FParse::Param(FCommandLine::Get(), TEXT("DirectionFindingClip")))
	{
		return;
	}
	const TSharedRef<FDirectionFindingClipCapture> Capture = MakeShared<FDirectionFindingClipCapture>();
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Capture](float)
	{
		return Capture->Tick();
	}));
});

#endif
