// Copyright (C) Bas Blokzijl - All rights reserved.

#include "CoverTakeoverValidateSubsystem.h"

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
#include "RTS_Survival/GameUI/InfoTakeover/W_CoverTakeover.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/Subsystems/SettingsMenuSubsystem/RTSSettingsMenuSubsystem.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "UnrealClient.h"

namespace CoverTakeoverValidateConstants
{
	const TCHAR* const CommandLineSwitch = TEXT("CoverTakeoverValidate");
	constexpr double HudBuildSeconds = 5.0;
	constexpr double BeforeStartWaitSeconds = 1.5;
	constexpr double StartGameSeconds = 3.0;
	constexpr double OpenTimeoutSeconds = 10.0;
	// Real seconds after the takeover opened: one shot while the title types, one once everything typed.
	constexpr double TypingShotSeconds = 0.4;
	constexpr double DoneShotSeconds = 9.0;
	constexpr double PressUnderstoodSeconds = 10.0;
	constexpr double SettleAfterCloseSeconds = 0.5;
	constexpr double NextCheckAfterCloseSeconds = 1.0;
	constexpr int32 ExpectedScreenshotCount = 3;
	const FName DifficultyButtonName(TEXT("M_NormalDifficultyButton"));
}

bool UCoverTakeoverValidateSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	const UWorld* const OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), CoverTakeoverValidateConstants::CommandLineSwitch);
#endif
}

void UCoverTakeoverValidateSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	RTSFunctionLibrary::PushModalDialogSuppression();
	const URTSGameUserSettings* const GameUserSettings = URTSGameUserSettings::Get();
	bM_OriginalSettingValue = GameUserSettings == nullptr || GameUserSettings->GetShowInfoTakeovers();
	EnterPhase(ECoverTakeoverValidatePhase::WaitingForHud);
	UE_LOG(LogTemp, Display, TEXT("COVER_TAKEOVER validate started (setting was %s)"),
	       bM_OriginalSettingValue ? TEXT("on") : TEXT("off"));
}

void UCoverTakeoverValidateSubsystem::Tick(float DeltaTime)
{
	switch (M_Phase)
	{
	case ECoverTakeoverValidatePhase::WaitingForHud:
		TickWaitingForHud();
		break;
	case ECoverTakeoverValidatePhase::WaitingBeforeStart:
		TickWaitingBeforeStart();
		break;
	case ECoverTakeoverValidatePhase::StartingGame:
		TickStartingGame();
		break;
	case ECoverTakeoverValidatePhase::WatchingTakeover:
		TickWatchingTakeover();
		break;
	case ECoverTakeoverValidatePhase::CheckingClosed:
		TickCheckingClosed();
		break;
	default:
		break;
	}
}

TStatId UCoverTakeoverValidateSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCoverTakeoverValidateSubsystem, STATGROUP_Tickables);
}

void UCoverTakeoverValidateSubsystem::EnterPhase(const ECoverTakeoverValidatePhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartSeconds = FPlatformTime::Seconds();
}

double UCoverTakeoverValidateSubsystem::GetSecondsInPhase() const
{
	return FPlatformTime::Seconds() - M_PhaseStartSeconds;
}

void UCoverTakeoverValidateSubsystem::TickWaitingForHud()
{
	if (GetSecondsInPhase() < CoverTakeoverValidateConstants::HudBuildSeconds)
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
		EnterPhase(ECoverTakeoverValidatePhase::WaitingBeforeStart);
		return;
	}
	UE_LOG(LogTemp, Display, TEXT("COVER_TAKEOVER no start-game screen on this map; the before-start check is skipped"));
	EnterPhase(ECoverTakeoverValidatePhase::StartingGame);
}

void UCoverTakeoverValidateSubsystem::TickWaitingBeforeStart()
{
	if (GetSecondsInPhase() < CoverTakeoverValidateConstants::BeforeStartWaitSeconds)
	{
		return;
	}
	const bool bOpenedBeforeStart = GetActiveCoverTakeover() != nullptr;
	UE_LOG(LogTemp, Display, TEXT("COVER_TAKEOVER before start: opened=%d (must be 0)"), bOpenedBeforeStart ? 1 : 0);
	if (bOpenedBeforeStart)
	{
		Fail(TEXT("the takeover opened over the start-game screen"));
	}
	if (M_StartGameControl.IsValid())
	{
		M_StartGameControl->PlayerStartedGame();
	}
	EnterPhase(ECoverTakeoverValidatePhase::StartingGame);
}

void UCoverTakeoverValidateSubsystem::TickStartingGame()
{
	DismissDifficultyPicker();
	if (GetSecondsInPhase() < CoverTakeoverValidateConstants::StartGameSeconds)
	{
		return;
	}
	if (M_MainNode == nullptr)
	{
		CallMainNode();
	}
	EnterPhase(ECoverTakeoverValidatePhase::WatchingTakeover);
}

void UCoverTakeoverValidateSubsystem::CallMainNode()
{
	M_MainNode = UAsyncShowCoverTakeover::ShowCoverTakeover(M_PlayerController.Get());
	M_MainNode->OnClosed.AddDynamic(this, &UCoverTakeoverValidateSubsystem::HandleMainClosed);
	M_MainNode->Activate();
	UE_LOG(LogTemp, Display, TEXT("COVER_TAKEOVER node called; closed count right after: %d"), M_MainClosedCount);
}

void UCoverTakeoverValidateSubsystem::TickWatchingTakeover()
{
	using namespace CoverTakeoverValidateConstants;
	UW_CoverTakeover* const Takeover = GetActiveCoverTakeover();
	if (not bM_HasOpened)
	{
		if (Takeover != nullptr)
		{
			StartWatching(*Takeover);
			return;
		}
		if (GetSecondsInPhase() > OpenTimeoutSeconds)
		{
			Fail(TEXT("the cover takeover never opened"));
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
	if (M_ShotsTaken == 0 && SecondsOpen >= TypingShotSeconds)
	{
		++M_ShotsTaken;
		Capture(TEXT("Cover_0_typing"));
		return;
	}
	if (M_ShotsTaken == 1 && SecondsOpen >= DoneShotSeconds)
	{
		++M_ShotsTaken;
		Capture(TEXT("Cover_1_done"));
		return;
	}
	if (M_ShotsTaken < 2 || SecondsOpen < PressUnderstoodSeconds)
	{
		return;
	}
	if (M_MainClosedCount != 0)
	{
		Fail(TEXT("On Closed fired before UNDERSTOOD"));
	}
	// The footer button as the player clicks it.
	Takeover->PressConfirmButton();
	EnterPhase(ECoverTakeoverValidatePhase::CheckingClosed);
}

void UCoverTakeoverValidateSubsystem::StartWatching(UW_CoverTakeover& Takeover)
{
	bM_HasOpened = true;
	M_WatchedTakeover = &Takeover;
	M_OpenSeconds = FPlatformTime::Seconds();
	const int32 ScreenshotCount = Takeover.GetLoadedScreenshotCount();
	const bool bPaused = UGameplayStatics::IsGamePaused(this);
	UE_LOG(LogTemp, Display, TEXT("COVER_TAKEOVER opened %s with %d preview screenshots, paused=%d"),
	       *Takeover.GetClass()->GetName(), ScreenshotCount, bPaused ? 1 : 0);
	if (ScreenshotCount != CoverTakeoverValidateConstants::ExpectedScreenshotCount)
	{
		Fail(TEXT("expected the green, yellow and red preview screenshots"));
	}
	if (not bPaused)
	{
		Fail(TEXT("the game is not paused while the takeover is open"));
	}
}

void UCoverTakeoverValidateSubsystem::TickCheckingClosed()
{
	using namespace CoverTakeoverValidateConstants;
	if (GetSecondsInPhase() < SettleAfterCloseSeconds)
	{
		return;
	}
	if (not bM_HasCapturedAfterClose)
	{
		bM_HasCapturedAfterClose = true;
		UE_LOG(LogTemp, Display, TEXT("COVER_TAKEOVER main node: closed %d times, shown=%d, paused when closed=%d, "
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
		Capture(TEXT("Cover_2_after_close"));
		return;
	}
	if (GetSecondsInPhase() < NextCheckAfterCloseSeconds)
	{
		return;
	}
	CheckShowsWhileTurnedOff();
	Finish();
}

void UCoverTakeoverValidateSubsystem::CheckShowsWhileTurnedOff()
{
	// "Never show again" only hides unit intel: this field manual must still open, without the box, and close as shown.
	SetTakeoversEnabled(false);
	M_TurnedOffNode = UAsyncShowCoverTakeover::ShowCoverTakeover(M_PlayerController.Get());
	M_TurnedOffNode->OnClosed.AddDynamic(this, &UCoverTakeoverValidateSubsystem::HandleTurnedOffClosed);
	M_TurnedOffNode->Activate();
	UW_CoverTakeover* const Takeover = GetActiveCoverTakeover();
	const bool bOpened = Takeover != nullptr;
	const bool bOffersNeverShowAgain = bOpened && Takeover->GetOffersNeverShowAgain();
	UE_LOG(LogTemp, Display, TEXT("COVER_TAKEOVER turned off: opened=%d, closed %d times at once, never-show box=%d"),
	       bOpened ? 1 : 0, M_TurnedOffClosedCount, bOffersNeverShowAgain ? 1 : 0);
	if (not bOpened || M_TurnedOffClosedCount != 0 || bOffersNeverShowAgain)
	{
		Fail(TEXT("with unit intel off, the field manual must still open, without the never-show box"));
		return;
	}
	Takeover->OnClickedContinue();
	UE_LOG(LogTemp, Display, TEXT("COVER_TAKEOVER turned off, after close: closed %d times, shown=%d"),
	       M_TurnedOffClosedCount, bM_TurnedOffWasShown ? 1 : 0);
	if (M_TurnedOffClosedCount != 1 || not bM_TurnedOffWasShown)
	{
		Fail(TEXT("with unit intel off, the field manual did not close once as shown"));
	}
}

void UCoverTakeoverValidateSubsystem::HandleMainClosed(bool bWasShown)
{
	++M_MainClosedCount;
	bM_MainWasShown = bWasShown;
	bM_MainWasPausedWhenClosed = UGameplayStatics::IsGamePaused(this);
	UE_LOG(LogTemp, Display, TEXT("COVER_TAKEOVER On Closed fired: shown=%d paused=%d"), bWasShown ? 1 : 0,
	       bM_MainWasPausedWhenClosed ? 1 : 0);
}

void UCoverTakeoverValidateSubsystem::HandleTurnedOffClosed(bool bWasShown)
{
	++M_TurnedOffClosedCount;
	bM_TurnedOffWasShown = bWasShown;
}

void UCoverTakeoverValidateSubsystem::SetTakeoversEnabled(const bool bEnabled) const
{
	URTSSettingsMenuSubsystem* const SettingsSubsystem = GetWorld()->GetGameInstance()->GetSubsystem<
		URTSSettingsMenuSubsystem>();
	if (SettingsSubsystem != nullptr)
	{
		SettingsSubsystem->SetShowInfoTakeoversImmediately(bEnabled);
	}
}

UW_CoverTakeover* UCoverTakeoverValidateSubsystem::GetActiveCoverTakeover() const
{
	const UInfoTakeoverSubsystem* const InfoTakeovers = GetWorld()->GetSubsystem<UInfoTakeoverSubsystem>();
	return InfoTakeovers != nullptr ? Cast<UW_CoverTakeover>(InfoTakeovers->GetActiveTakeover()) : nullptr;
}

void UCoverTakeoverValidateSubsystem::DismissDifficultyPicker() const
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
			Picker->WidgetTree->FindWidget(CoverTakeoverValidateConstants::DifficultyButtonName));
		if (IsValid(NormalButton))
		{
			NormalButton->OnClicked.Broadcast();
		}
	}
}

void UCoverTakeoverValidateSubsystem::Capture(const FString& Label) const
{
	const FIntPoint ViewportSize = GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport
		                               ? GEngine->GameViewport->Viewport->GetSizeXY()
		                               : FIntPoint::ZeroValue;
	const FString Folder = FString::Printf(TEXT("%dp"), ViewportSize.Y);
	const FString ScreenshotPath = FPaths::ProjectSavedDir() / TEXT("InfoTakeover") / TEXT("Cover") / Folder
		/ (Label + TEXT(".png"));
	FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
	UE_LOG(LogTemp, Display, TEXT("COVER_TAKEOVER capture %s"), *ScreenshotPath);
}

void UCoverTakeoverValidateSubsystem::Fail(const FString& Reason)
{
	++M_Failures;
	UE_LOG(LogTemp, Error, TEXT("COVER_TAKEOVER FAIL %s"), *Reason);
}

void UCoverTakeoverValidateSubsystem::Finish()
{
	SetTakeoversEnabled(bM_OriginalSettingValue);
	EnterPhase(ECoverTakeoverValidatePhase::Done);
	UE_LOG(LogTemp, Display, TEXT("COVER_TAKEOVER done: %d failures -> %s"), M_Failures,
	       M_Failures == 0 ? TEXT("PASS") : TEXT("FAIL"));
	FPlatformMisc::RequestExit(false, TEXT("CoverTakeoverValidate"));
}
