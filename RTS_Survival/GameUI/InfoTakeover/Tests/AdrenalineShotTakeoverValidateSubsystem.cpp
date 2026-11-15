// Copyright (C) Bas Blokzijl - All rights reserved.

#include "AdrenalineShotTakeoverValidateSubsystem.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "RTS_Survival/Game/UserSettings/RTSGameUserSettings.h"
#include "RTS_Survival/GameUI/GameDifficultyPicker/W_GameDifficultyPicker.h"
#include "RTS_Survival/GameUI/InfoTakeover/AsyncShowInfoTakeover.h"
#include "RTS_Survival/GameUI/InfoTakeover/InfoTakeoverSubsystem.h"
#include "RTS_Survival/GameUI/InfoTakeover/W_AdrenalineShotTakeover.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/Subsystems/SettingsMenuSubsystem/RTSSettingsMenuSubsystem.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "UnrealClient.h"

namespace AdrenalineShotTakeoverValidateConstants
{
	const TCHAR* const CommandLineSwitch = TEXT("AdrenalineShotTakeoverValidate");
	constexpr double HudBuildSeconds = 5.0;
	constexpr double BeforeStartWaitSeconds = 1.5;
	constexpr double StartGameSeconds = 3.0;
	constexpr double OpenTimeoutSeconds = 10.0;
	// Real seconds after the takeover opened: one shot while the title types, one once everything typed.
	constexpr double TypingShotSeconds = 0.4;
	constexpr double DoneShotSeconds = 9.0;
	// The finished shot waits this long at most for the typewriter.
	constexpr double MaxTypingWaitSeconds = 120.0;
	constexpr double PressUnderstoodAfterDoneShotSeconds = 1.0;
	constexpr double SettleAfterTypingSeconds = 0.5;
	constexpr double SettleAfterCloseSeconds = 0.5;
	constexpr double NextCheckAfterCloseSeconds = 1.0;
	// Real seconds after opening for the two clip shots: the throw, then the blast.
	constexpr double ClipShotSeconds[] = {2.0, 3.6};
	// About two seconds of a 12 fps clip; the run must see at least this many frame changes while paused.
	constexpr int32 MinClipFrameChanges = 20;
	const FName DifficultyButtonName(TEXT("M_NormalDifficultyButton"));
}

bool UAdrenalineShotTakeoverValidateSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	const UWorld* const OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), AdrenalineShotTakeoverValidateConstants::CommandLineSwitch);
#endif
}

void UAdrenalineShotTakeoverValidateSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	RTSFunctionLibrary::PushModalDialogSuppression();
	const URTSGameUserSettings* const GameUserSettings = URTSGameUserSettings::Get();
	bM_OriginalSettingValue = GameUserSettings == nullptr || GameUserSettings->GetShowInfoTakeovers();
	EnterPhase(EAdrenalineShotTakeoverValidatePhase::WaitingForHud);
	UE_LOG(LogTemp, Display, TEXT("ADRENALINE_SHOT_TAKEOVER validate started (setting was %s)"),
	       bM_OriginalSettingValue ? TEXT("on") : TEXT("off"));
}

void UAdrenalineShotTakeoverValidateSubsystem::Tick(float DeltaTime)
{
	switch (M_Phase)
	{
	case EAdrenalineShotTakeoverValidatePhase::WaitingForHud:
		TickWaitingForHud();
		break;
	case EAdrenalineShotTakeoverValidatePhase::WaitingBeforeStart:
		TickWaitingBeforeStart();
		break;
	case EAdrenalineShotTakeoverValidatePhase::StartingGame:
		TickStartingGame();
		break;
	case EAdrenalineShotTakeoverValidatePhase::WatchingTakeover:
		TickWatchingTakeover();
		break;
	case EAdrenalineShotTakeoverValidatePhase::CheckingClosed:
		TickCheckingClosed();
		break;
	default:
		break;
	}
}

TStatId UAdrenalineShotTakeoverValidateSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAdrenalineShotTakeoverValidateSubsystem, STATGROUP_Tickables);
}

void UAdrenalineShotTakeoverValidateSubsystem::EnterPhase(const EAdrenalineShotTakeoverValidatePhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartSeconds = FPlatformTime::Seconds();
}

double UAdrenalineShotTakeoverValidateSubsystem::GetSecondsInPhase() const
{
	return FPlatformTime::Seconds() - M_PhaseStartSeconds;
}

void UAdrenalineShotTakeoverValidateSubsystem::TickWaitingForHud()
{
	if (GetSecondsInPhase() < AdrenalineShotTakeoverValidateConstants::HudBuildSeconds)
	{
		return;
	}
	M_PlayerController = Cast<ACPPController>(GetWorld()->GetFirstPlayerController());
	if (not M_PlayerController.IsValid())
	{
		return;
	}
	SetTakeoversEnabled(true);
	M_StartGameControl = M_PlayerController->FindComponentByClass<UPlayerStartGameControl>();
	if (M_StartGameControl.IsValid() && M_StartGameControl->GetIsWaitingForPlayerToStart())
	{
		// As a mission that calls the node on its first frame: the takeover must wait for the start button.
		CallMainNode();
		EnterPhase(EAdrenalineShotTakeoverValidatePhase::WaitingBeforeStart);
		return;
	}
	UE_LOG(LogTemp, Display, TEXT("ADRENALINE_SHOT_TAKEOVER no start-game screen on this map; the before-start check is skipped"));
	EnterPhase(EAdrenalineShotTakeoverValidatePhase::StartingGame);
}

void UAdrenalineShotTakeoverValidateSubsystem::TickWaitingBeforeStart()
{
	if (GetSecondsInPhase() < AdrenalineShotTakeoverValidateConstants::BeforeStartWaitSeconds)
	{
		return;
	}
	const bool bOpenedBeforeStart = GetActiveAdrenalineShotTakeover() != nullptr;
	UE_LOG(LogTemp, Display, TEXT("ADRENALINE_SHOT_TAKEOVER before start: opened=%d (must be 0)"), bOpenedBeforeStart ? 1 : 0);
	if (bOpenedBeforeStart)
	{
		Fail(TEXT("the takeover opened over the start-game screen"));
	}
	if (M_StartGameControl.IsValid())
	{
		M_StartGameControl->PlayerStartedGame();
	}
	EnterPhase(EAdrenalineShotTakeoverValidatePhase::StartingGame);
}

void UAdrenalineShotTakeoverValidateSubsystem::TickStartingGame()
{
	DismissDifficultyPicker();
	if (GetSecondsInPhase() < AdrenalineShotTakeoverValidateConstants::StartGameSeconds)
	{
		return;
	}
	if (M_MainNode == nullptr)
	{
		CallMainNode();
	}
	EnterPhase(EAdrenalineShotTakeoverValidatePhase::WatchingTakeover);
}

void UAdrenalineShotTakeoverValidateSubsystem::CallMainNode()
{
	M_MainNode = UAsyncShowAdrenalineShotTakeover::ShowAdrenalineShotTakeover(M_PlayerController.Get());
	M_MainNode->OnClosed.AddDynamic(this, &UAdrenalineShotTakeoverValidateSubsystem::HandleMainClosed);
	M_MainNode->Activate();
	UE_LOG(LogTemp, Display, TEXT("ADRENALINE_SHOT_TAKEOVER node called; closed count right after: %d"), M_MainClosedCount);
}

void UAdrenalineShotTakeoverValidateSubsystem::TickWatchingTakeover()
{
	using namespace AdrenalineShotTakeoverValidateConstants;
	UW_AdrenalineShotTakeover* const Takeover = GetActiveAdrenalineShotTakeover();
	if (not bM_HasOpened)
	{
		if (Takeover != nullptr)
		{
			StartWatching(*Takeover);
			return;
		}
		if (GetSecondsInPhase() > OpenTimeoutSeconds)
		{
			Fail(TEXT("the Adrenaline Shot takeover never opened"));
			Finish();
		}
		return;
	}
	if (Takeover == nullptr || Takeover != M_WatchedTakeover.Get())
	{
		Fail(TEXT("the takeover closed before UNDERSTOOD was pressed"));
		Finish();
		return;
	}
	const double SecondsOpen = FPlatformTime::Seconds() - M_OpenSeconds;
	TrackClip(*Takeover, SecondsOpen);
	if (M_ShotsTaken == 0 && SecondsOpen >= TypingShotSeconds)
	{
		++M_ShotsTaken;
		Capture(TEXT("AdrenalineShot_0_typing"));
		return;
	}
	if (M_ShotsTaken == 1)
	{
		// A slow (DebugGame, busy GPU) run types slower than the clock; the finished panel waits for the typewriter.
		const bool bTypingDone = Takeover->GetTypingPhase() == EInfoTakeoverTypingPhase::Done;
		if (bTypingDone && M_TypingDoneSeconds < 0.0)
		{
			M_TypingDoneSeconds = SecondsOpen;
		}
		// A frame after the last letter, so the shot shows it.
		const bool bLastLetterDrawn = bTypingDone && SecondsOpen >= M_TypingDoneSeconds + SettleAfterTypingSeconds;
		if (SecondsOpen < DoneShotSeconds || (not bLastLetterDrawn && SecondsOpen < MaxTypingWaitSeconds))
		{
			return;
		}
		if (not bTypingDone)
		{
			Fail(TEXT("the title and description did not finish typing"));
		}
		++M_ShotsTaken;
		M_DoneShotSeconds = SecondsOpen;
		Capture(TEXT("AdrenalineShot_1_done"));
		return;
	}
	if (M_ShotsTaken < 2 || SecondsOpen < M_DoneShotSeconds + PressUnderstoodAfterDoneShotSeconds)
	{
		return;
	}
	if (M_MainClosedCount != 0)
	{
		Fail(TEXT("On Closed fired before UNDERSTOOD"));
	}
	const bool bPausedBeforeClose = UGameplayStatics::IsGamePaused(this);
	UE_LOG(LogTemp, Display, TEXT("ADRENALINE_SHOT_TAKEOVER clip changed frame %d times in %.1f s, paused=%d"),
	       M_ClipFrameChanges, SecondsOpen, bPausedBeforeClose ? 1 : 0);
	if (M_ClipFrameChanges < MinClipFrameChanges || not bPausedBeforeClose)
	{
		Fail(TEXT("the clip did not keep playing while the game was paused"));
	}
	// The footer button as the player clicks it.
	Takeover->PressConfirmButton();
	EnterPhase(EAdrenalineShotTakeoverValidatePhase::CheckingClosed);
}

void UAdrenalineShotTakeoverValidateSubsystem::StartWatching(UW_AdrenalineShotTakeover& Takeover)
{
	bM_HasOpened = true;
	M_WatchedTakeover = &Takeover;
	M_OpenSeconds = FPlatformTime::Seconds();
	const bool bHasRender = Takeover.GetHasLoadedClip();
	const bool bPaused = UGameplayStatics::IsGamePaused(this);
	UE_LOG(LogTemp, Display, TEXT("ADRENALINE_SHOT_TAKEOVER opened %s, clip atlas loaded=%d, paused=%d"),
	       *Takeover.GetClass()->GetName(), bHasRender ? 1 : 0, bPaused ? 1 : 0);
	if (Takeover.GetClass() == UW_AdrenalineShotTakeover::StaticClass())
	{
		Fail(TEXT("WBP_AdrenalineShotTakeover did not load; the unstyled C++ widget opened"));
	}
	if (not bHasRender)
	{
		Fail(TEXT("the clip atlas did not load"));
	}
	if (not bPaused)
	{
		Fail(TEXT("the game is not paused while the takeover is open"));
	}
}

void UAdrenalineShotTakeoverValidateSubsystem::TickCheckingClosed()
{
	using namespace AdrenalineShotTakeoverValidateConstants;
	if (GetSecondsInPhase() < SettleAfterCloseSeconds)
	{
		return;
	}
	if (not bM_HasCapturedAfterClose)
	{
		bM_HasCapturedAfterClose = true;
		UE_LOG(LogTemp, Display, TEXT("ADRENALINE_SHOT_TAKEOVER main node: closed %d times, shown=%d, paused when closed=%d, "
			       "paused now=%d"), M_MainClosedCount, bM_MainWasShown ? 1 : 0, bM_MainWasPausedWhenClosed ? 1 : 0,
		       UGameplayStatics::IsGamePaused(this) ? 1 : 0);
		if (M_MainClosedCount != 1 || not bM_MainWasShown)
		{
			Fail(TEXT("On Closed did not fire exactly once with Was Shown after UNDERSTOOD"));
		}
		if (bM_MainWasPausedWhenClosed || UGameplayStatics::IsGamePaused(this))
		{
			Fail(TEXT("the game was still paused when On Closed fired"));
		}
		Capture(TEXT("AdrenalineShot_2_after_close"));
		return;
	}
	if (GetSecondsInPhase() < NextCheckAfterCloseSeconds)
	{
		return;
	}
	CheckTurnedOffNode();
	Finish();
}

void UAdrenalineShotTakeoverValidateSubsystem::CheckTurnedOffNode()
{
	SetTakeoversEnabled(false);
	M_TurnedOffNode = UAsyncShowAdrenalineShotTakeover::ShowAdrenalineShotTakeover(M_PlayerController.Get());
	M_TurnedOffNode->OnClosed.AddDynamic(this, &UAdrenalineShotTakeoverValidateSubsystem::HandleTurnedOffClosed);
	M_TurnedOffNode->Activate();
	const bool bOpened = GetActiveAdrenalineShotTakeover() != nullptr;
	UE_LOG(LogTemp, Display, TEXT("ADRENALINE_SHOT_TAKEOVER turned off: closed %d times at once, shown=%d, opened=%d"),
	       M_TurnedOffClosedCount, bM_TurnedOffWasShown ? 1 : 0, bOpened ? 1 : 0);
	if (M_TurnedOffClosedCount != 1 || bM_TurnedOffWasShown || bOpened)
	{
		Fail(TEXT("with the takeovers off, On Closed must fire at once with Was Shown false"));
	}
}

void UAdrenalineShotTakeoverValidateSubsystem::TrackClip(const UW_AdrenalineShotTakeover& Takeover, const double SecondsOpen)
{
	using namespace AdrenalineShotTakeoverValidateConstants;
	const int32 ClipFrame = Takeover.GetShownClipFrame();
	if (ClipFrame != M_LastClipFrame)
	{
		M_ClipFrameChanges += M_LastClipFrame == INDEX_NONE ? 0 : 1;
		M_LastClipFrame = ClipFrame;
	}
	if (M_ClipShotsTaken < static_cast<int32>(UE_ARRAY_COUNT(ClipShotSeconds)) && SecondsOpen >= ClipShotSeconds[M_ClipShotsTaken])
	{
		Capture(FString::Printf(TEXT("AdrenalineShot_clip_%d_frame%02d"), M_ClipShotsTaken, ClipFrame));
		++M_ClipShotsTaken;
	}
}

void UAdrenalineShotTakeoverValidateSubsystem::HandleMainClosed(bool bWasShown)
{
	++M_MainClosedCount;
	bM_MainWasShown = bWasShown;
	bM_MainWasPausedWhenClosed = UGameplayStatics::IsGamePaused(this);
	UE_LOG(LogTemp, Display, TEXT("ADRENALINE_SHOT_TAKEOVER On Closed fired: shown=%d paused=%d"), bWasShown ? 1 : 0,
	       bM_MainWasPausedWhenClosed ? 1 : 0);
}

void UAdrenalineShotTakeoverValidateSubsystem::HandleTurnedOffClosed(bool bWasShown)
{
	++M_TurnedOffClosedCount;
	bM_TurnedOffWasShown = bWasShown;
}

void UAdrenalineShotTakeoverValidateSubsystem::SetTakeoversEnabled(const bool bEnabled) const
{
	URTSSettingsMenuSubsystem* const SettingsSubsystem = GetWorld()->GetGameInstance()->GetSubsystem<
		URTSSettingsMenuSubsystem>();
	if (SettingsSubsystem != nullptr)
	{
		SettingsSubsystem->SetShowInfoTakeoversImmediately(bEnabled);
	}
}

UW_AdrenalineShotTakeover* UAdrenalineShotTakeoverValidateSubsystem::GetActiveAdrenalineShotTakeover() const
{
	const UInfoTakeoverSubsystem* const InfoTakeovers = GetWorld()->GetSubsystem<UInfoTakeoverSubsystem>();
	return InfoTakeovers != nullptr ? Cast<UW_AdrenalineShotTakeover>(InfoTakeovers->GetActiveTakeover()) : nullptr;
}

void UAdrenalineShotTakeoverValidateSubsystem::DismissDifficultyPicker() const
{
	TArray<UUserWidget*> Pickers;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), Pickers, UW_GameDifficultyPicker::StaticClass(), false);
	for (UUserWidget* const Picker : Pickers)
	{
		if (not IsValid(Picker) || not IsValid(Picker->WidgetTree) || not Picker->IsInViewport())
		{
			continue;
		}
		UButton* const NormalButton = Cast<UButton>(
			Picker->WidgetTree->FindWidget(AdrenalineShotTakeoverValidateConstants::DifficultyButtonName));
		if (IsValid(NormalButton))
		{
			NormalButton->OnClicked.Broadcast();
		}
	}
}

void UAdrenalineShotTakeoverValidateSubsystem::Capture(const FString& Label) const
{
	const FIntPoint ViewportSize = GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport
		                               ? GEngine->GameViewport->Viewport->GetSizeXY()
		                               : FIntPoint::ZeroValue;
	const FString Folder = FString::Printf(TEXT("%dp"), ViewportSize.Y);
	const FString ScreenshotPath = FPaths::ProjectSavedDir() / TEXT("InfoTakeover") / TEXT("AdrenalineShot") / Folder
		/ (Label + TEXT(".png"));
	FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
	UE_LOG(LogTemp, Display, TEXT("ADRENALINE_SHOT_TAKEOVER capture %s"), *ScreenshotPath);
}

void UAdrenalineShotTakeoverValidateSubsystem::Fail(const FString& Reason)
{
	++M_Failures;
	UE_LOG(LogTemp, Error, TEXT("ADRENALINE_SHOT_TAKEOVER FAIL %s"), *Reason);
}

void UAdrenalineShotTakeoverValidateSubsystem::Finish()
{
	SetTakeoversEnabled(bM_OriginalSettingValue);
	EnterPhase(EAdrenalineShotTakeoverValidatePhase::Done);
	UE_LOG(LogTemp, Display, TEXT("ADRENALINE_SHOT_TAKEOVER done: %d failures -> %s"), M_Failures,
	       M_Failures == 0 ? TEXT("PASS") : TEXT("FAIL"));
	FPlatformMisc::RequestExit(false, TEXT("AdrenalineShotTakeoverValidate"));
}
