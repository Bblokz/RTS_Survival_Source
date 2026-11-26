// Copyright (C) Bas Blokzijl - All rights reserved.

#include "WallBreachTakeoverValidateSubsystem.h"

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
#include "RTS_Survival/GameUI/InfoTakeover/W_WallBreachTakeover.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/Subsystems/SettingsMenuSubsystem/RTSSettingsMenuSubsystem.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "UnrealClient.h"

namespace WallBreachTakeoverValidateConstants
{
	const TCHAR* const CommandLineSwitch = TEXT("WallBreachTakeoverValidate");
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
	// The run-over page: a clip shot this long after CONTINUE, and the finished shot no earlier than this.
	constexpr double RunOverClipShotSeconds = 3.0;
	constexpr double RunOverDoneShotSeconds = 6.0;
	constexpr int32 ExpectedPageCount = 2;
	const FName DifficultyButtonName(TEXT("M_NormalDifficultyButton"));
}

bool UWallBreachTakeoverValidateSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	const UWorld* const OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), WallBreachTakeoverValidateConstants::CommandLineSwitch);
#endif
}

void UWallBreachTakeoverValidateSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	RTSFunctionLibrary::PushModalDialogSuppression();
	const URTSGameUserSettings* const GameUserSettings = URTSGameUserSettings::Get();
	bM_OriginalSettingValue = GameUserSettings == nullptr || GameUserSettings->GetShowInfoTakeovers();
	EnterPhase(EWallBreachTakeoverValidatePhase::WaitingForHud);
	UE_LOG(LogTemp, Display, TEXT("WALL_BREACH_TAKEOVER validate started (setting was %s)"),
	       bM_OriginalSettingValue ? TEXT("on") : TEXT("off"));
}

void UWallBreachTakeoverValidateSubsystem::Tick(float DeltaTime)
{
	switch (M_Phase)
	{
	case EWallBreachTakeoverValidatePhase::WaitingForHud:
		TickWaitingForHud();
		break;
	case EWallBreachTakeoverValidatePhase::WaitingBeforeStart:
		TickWaitingBeforeStart();
		break;
	case EWallBreachTakeoverValidatePhase::StartingGame:
		TickStartingGame();
		break;
	case EWallBreachTakeoverValidatePhase::WatchingTakeover:
		TickWatchingTakeover();
		break;
	case EWallBreachTakeoverValidatePhase::CheckingClosed:
		TickCheckingClosed();
		break;
	default:
		break;
	}
}

TStatId UWallBreachTakeoverValidateSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWallBreachTakeoverValidateSubsystem, STATGROUP_Tickables);
}

void UWallBreachTakeoverValidateSubsystem::EnterPhase(const EWallBreachTakeoverValidatePhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartSeconds = FPlatformTime::Seconds();
}

double UWallBreachTakeoverValidateSubsystem::GetSecondsInPhase() const
{
	return FPlatformTime::Seconds() - M_PhaseStartSeconds;
}

void UWallBreachTakeoverValidateSubsystem::TickWaitingForHud()
{
	if (GetSecondsInPhase() < WallBreachTakeoverValidateConstants::HudBuildSeconds)
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
		EnterPhase(EWallBreachTakeoverValidatePhase::WaitingBeforeStart);
		return;
	}
	UE_LOG(LogTemp, Display, TEXT("WALL_BREACH_TAKEOVER no start-game screen on this map; the before-start check is skipped"));
	EnterPhase(EWallBreachTakeoverValidatePhase::StartingGame);
}

void UWallBreachTakeoverValidateSubsystem::TickWaitingBeforeStart()
{
	if (GetSecondsInPhase() < WallBreachTakeoverValidateConstants::BeforeStartWaitSeconds)
	{
		return;
	}
	const bool bOpenedBeforeStart = GetActiveWallBreachTakeover() != nullptr;
	UE_LOG(LogTemp, Display, TEXT("WALL_BREACH_TAKEOVER before start: opened=%d (must be 0)"), bOpenedBeforeStart ? 1 : 0);
	if (bOpenedBeforeStart)
	{
		Fail(TEXT("the takeover opened over the start-game screen"));
	}
	if (M_StartGameControl.IsValid())
	{
		M_StartGameControl->PlayerStartedGame();
	}
	EnterPhase(EWallBreachTakeoverValidatePhase::StartingGame);
}

void UWallBreachTakeoverValidateSubsystem::TickStartingGame()
{
	DismissDifficultyPicker();
	if (GetSecondsInPhase() < WallBreachTakeoverValidateConstants::StartGameSeconds)
	{
		return;
	}
	if (M_MainNode == nullptr)
	{
		CallMainNode();
	}
	EnterPhase(EWallBreachTakeoverValidatePhase::WatchingTakeover);
}

void UWallBreachTakeoverValidateSubsystem::CallMainNode()
{
	M_MainNode = UAsyncShowWallBreachTakeover::ShowWallBreachTakeover(M_PlayerController.Get());
	M_MainNode->OnClosed.AddDynamic(this, &UWallBreachTakeoverValidateSubsystem::HandleMainClosed);
	M_MainNode->Activate();
	UE_LOG(LogTemp, Display, TEXT("WALL_BREACH_TAKEOVER node called; closed count right after: %d"), M_MainClosedCount);
}

void UWallBreachTakeoverValidateSubsystem::TickWatchingTakeover()
{
	using namespace WallBreachTakeoverValidateConstants;
	UW_WallBreachTakeover* const Takeover = GetActiveWallBreachTakeover();
	if (not bM_HasOpened)
	{
		if (Takeover != nullptr)
		{
			StartWatching(*Takeover);
			return;
		}
		if (GetSecondsInPhase() > OpenTimeoutSeconds)
		{
			Fail(TEXT("the Wall Breach takeover never opened"));
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
		Capture(TEXT("WallBreach_0_typing"));
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
		Capture(TEXT("WallBreach_1_done"));
		return;
	}
	if (M_ShotsTaken >= 3)
	{
		TickRunOverPage(*Takeover, SecondsOpen);
		return;
	}
	if (M_ShotsTaken < 2 || SecondsOpen < M_DoneShotSeconds + PressUnderstoodAfterDoneShotSeconds)
	{
		return;
	}
	if (M_MainClosedCount != 0)
	{
		Fail(TEXT("On Closed fired on the first page"));
	}
	CheckClipPlayedWhilePaused(TEXT("walls"), SecondsOpen);
	// The footer button (CONTINUE) as the player clicks it: the run-over page shows, the takeover stays open.
	Takeover->PressConfirmButton();
	CheckPage(*Takeover, 1, TEXT("CONTINUE"));
	M_RunOverPageSeconds = SecondsOpen;
	M_TypingDoneSeconds = -1.0;
	M_LastClipFrame = INDEX_NONE;
	M_ClipFrameChanges = 0;
	M_ShotsTaken = 3;
}

void UWallBreachTakeoverValidateSubsystem::TickRunOverPage(UW_WallBreachTakeover& Takeover, const double SecondsOpen)
{
	using namespace WallBreachTakeoverValidateConstants;
	const double SecondsOnPage = SecondsOpen - M_RunOverPageSeconds;
	if (not bM_HasRunOverClipShot && SecondsOnPage >= RunOverClipShotSeconds)
	{
		bM_HasRunOverClipShot = true;
		Capture(FString::Printf(TEXT("WallBreach_runover_clip_frame%02d"), Takeover.GetShownClipFrame()));
		return;
	}
	if (M_ShotsTaken == 3)
	{
		const bool bTypingDone = Takeover.GetTypingPhase() == EInfoTakeoverTypingPhase::Done;
		if (bTypingDone && M_TypingDoneSeconds < 0.0)
		{
			M_TypingDoneSeconds = SecondsOnPage;
		}
		const bool bLastLetterDrawn = bTypingDone && SecondsOnPage >= M_TypingDoneSeconds + SettleAfterTypingSeconds;
		if (SecondsOnPage < RunOverDoneShotSeconds || (not bLastLetterDrawn && SecondsOnPage < MaxTypingWaitSeconds))
		{
			return;
		}
		if (not bTypingDone)
		{
			Fail(TEXT("the run-over page's title and description did not finish typing"));
		}
		M_ShotsTaken = 4;
		M_DoneShotSeconds = SecondsOpen;
		Capture(TEXT("WallBreach_3_runover_done"));
		return;
	}
	if (SecondsOpen < M_DoneShotSeconds + PressUnderstoodAfterDoneShotSeconds)
	{
		return;
	}
	CheckClipPlayedWhilePaused(TEXT("run-over"), SecondsOnPage);
	// PREVIOUS returns to the walls, CONTINUE comes back; neither closes the takeover.
	Takeover.PressPreviousButton();
	CheckPage(Takeover, 0, TEXT("PREVIOUS"));
	Takeover.PressConfirmButton();
	CheckPage(Takeover, 1, TEXT("CONTINUE again"));
	if (M_MainClosedCount != 0)
	{
		Fail(TEXT("On Closed fired before UNDERSTOOD on the last page"));
	}
	// UNDERSTOOD on the last page closes it.
	Takeover.PressConfirmButton();
	EnterPhase(EWallBreachTakeoverValidatePhase::CheckingClosed);
}

void UWallBreachTakeoverValidateSubsystem::CheckClipPlayedWhilePaused(const TCHAR* PageName, const double Seconds)
{
	using namespace WallBreachTakeoverValidateConstants;
	const bool bPaused = UGameplayStatics::IsGamePaused(this);
	UE_LOG(LogTemp, Display, TEXT("WALL_BREACH_TAKEOVER %s clip changed frame %d times in %.1f s, paused=%d"),
	       PageName, M_ClipFrameChanges, Seconds, bPaused ? 1 : 0);
	if (M_ClipFrameChanges < MinClipFrameChanges || not bPaused)
	{
		Fail(FString::Printf(TEXT("the %s clip did not keep playing while the game was paused"), PageName));
	}
}

void UWallBreachTakeoverValidateSubsystem::CheckPage(const UW_WallBreachTakeover& Takeover, const int32 ExpectedPage,
                                                     const TCHAR* AfterWhat, const bool bExpectFirstClipFrame)
{
	using namespace WallBreachTakeoverValidateConstants;
	const int32 Page = Takeover.GetCurrentPageIndex();
	const int32 LitLamp = Takeover.GetLitPageLampIndex();
	const bool bPreviousVisible = Takeover.GetIsPreviousButtonVisible();
	const bool bClipLoaded = Takeover.GetHasLoadedClip();
	const int32 ClipFrame = Takeover.GetShownClipFrame();
	const bool bStillOpen = GetActiveWallBreachTakeover() == &Takeover;
	UE_LOG(LogTemp, Display, TEXT("WALL_BREACH_TAKEOVER after %s: page=%d/%d lamps=%d lit=%d previous=%d clip loaded=%d "
		       "clip frame=%d open=%d"), AfterWhat, Page, Takeover.GetPageCount(), Takeover.GetPageLampCount(), LitLamp,
	       bPreviousVisible ? 1 : 0, bClipLoaded ? 1 : 0, ClipFrame, bStillOpen ? 1 : 0);
	if (Takeover.GetPageCount() != ExpectedPageCount || Takeover.GetPageLampCount() != ExpectedPageCount)
	{
		Fail(TEXT("the takeover does not have two pages with a lamp each"));
	}
	if (Page != ExpectedPage || LitLamp != ExpectedPage || bPreviousVisible != (ExpectedPage > 0) || not bStillOpen)
	{
		Fail(FString::Printf(TEXT("after %s the takeover is not on page %d with that lamp lit and PREVIOUS %s"),
		                     AfterWhat, ExpectedPage + 1, ExpectedPage > 0 ? TEXT("shown") : TEXT("hidden")));
	}
	if (not bClipLoaded || (bExpectFirstClipFrame && ClipFrame != 0))
	{
		Fail(FString::Printf(TEXT("after %s the page's clip is not loaded or did not start at its first frame"),
		                     AfterWhat));
	}
}

void UWallBreachTakeoverValidateSubsystem::StartWatching(UW_WallBreachTakeover& Takeover)
{
	bM_HasOpened = true;
	M_WatchedTakeover = &Takeover;
	M_OpenSeconds = FPlatformTime::Seconds();
	const bool bHasRender = Takeover.GetHasLoadedClip();
	const bool bPaused = UGameplayStatics::IsGamePaused(this);
	UE_LOG(LogTemp, Display, TEXT("WALL_BREACH_TAKEOVER opened %s, clip atlas loaded=%d, paused=%d"),
	       *Takeover.GetClass()->GetName(), bHasRender ? 1 : 0, bPaused ? 1 : 0);
	if (Takeover.GetClass() == UW_WallBreachTakeover::StaticClass())
	{
		Fail(TEXT("WBP_WallBreachTakeover did not load; the unstyled C++ widget opened"));
	}
	if (not bHasRender)
	{
		Fail(TEXT("the clip atlas did not load"));
	}
	if (not bPaused)
	{
		Fail(TEXT("the game is not paused while the takeover is open"));
	}
	// The clip may already have stepped a frame before the first watch tick.
	CheckPage(Takeover, 0, TEXT("opening"), false);
}

void UWallBreachTakeoverValidateSubsystem::TickCheckingClosed()
{
	using namespace WallBreachTakeoverValidateConstants;
	if (GetSecondsInPhase() < SettleAfterCloseSeconds)
	{
		return;
	}
	if (not bM_HasCapturedAfterClose)
	{
		bM_HasCapturedAfterClose = true;
		UE_LOG(LogTemp, Display, TEXT("WALL_BREACH_TAKEOVER main node: closed %d times, shown=%d, paused when closed=%d, "
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
		Capture(TEXT("WallBreach_2_after_close"));
		return;
	}
	if (GetSecondsInPhase() < NextCheckAfterCloseSeconds)
	{
		return;
	}
	CheckShowsWhileTurnedOff();
	Finish();
}

void UWallBreachTakeoverValidateSubsystem::CheckShowsWhileTurnedOff()
{
	// "Never show again" only hides unit intel: this field manual must still open, without the box, and close as shown.
	SetTakeoversEnabled(false);
	M_TurnedOffNode = UAsyncShowWallBreachTakeover::ShowWallBreachTakeover(M_PlayerController.Get());
	M_TurnedOffNode->OnClosed.AddDynamic(this, &UWallBreachTakeoverValidateSubsystem::HandleTurnedOffClosed);
	M_TurnedOffNode->Activate();
	UW_WallBreachTakeover* const Takeover = GetActiveWallBreachTakeover();
	const bool bOpened = Takeover != nullptr;
	const bool bOffersNeverShowAgain = bOpened && Takeover->GetOffersNeverShowAgain();
	UE_LOG(LogTemp, Display, TEXT("WALL_BREACH_TAKEOVER turned off: opened=%d, closed %d times at once, never-show box=%d"),
	       bOpened ? 1 : 0, M_TurnedOffClosedCount, bOffersNeverShowAgain ? 1 : 0);
	if (not bOpened || M_TurnedOffClosedCount != 0 || bOffersNeverShowAgain)
	{
		Fail(TEXT("with unit intel off, the field manual must still open, without the never-show box"));
		return;
	}
	Takeover->OnClickedContinue();
	UE_LOG(LogTemp, Display, TEXT("WALL_BREACH_TAKEOVER turned off, after close: closed %d times, shown=%d"),
	       M_TurnedOffClosedCount, bM_TurnedOffWasShown ? 1 : 0);
	if (M_TurnedOffClosedCount != 1 || not bM_TurnedOffWasShown)
	{
		Fail(TEXT("with unit intel off, the field manual did not close once as shown"));
	}
}

void UWallBreachTakeoverValidateSubsystem::TrackClip(const UW_WallBreachTakeover& Takeover, const double SecondsOpen)
{
	using namespace WallBreachTakeoverValidateConstants;
	const int32 ClipFrame = Takeover.GetShownClipFrame();
	if (ClipFrame != M_LastClipFrame)
	{
		M_ClipFrameChanges += M_LastClipFrame == INDEX_NONE ? 0 : 1;
		M_LastClipFrame = ClipFrame;
	}
	if (M_ClipShotsTaken < static_cast<int32>(UE_ARRAY_COUNT(ClipShotSeconds)) && SecondsOpen >= ClipShotSeconds[M_ClipShotsTaken])
	{
		Capture(FString::Printf(TEXT("WallBreach_clip_%d_frame%02d"), M_ClipShotsTaken, ClipFrame));
		++M_ClipShotsTaken;
	}
}

void UWallBreachTakeoverValidateSubsystem::HandleMainClosed(bool bWasShown)
{
	++M_MainClosedCount;
	bM_MainWasShown = bWasShown;
	bM_MainWasPausedWhenClosed = UGameplayStatics::IsGamePaused(this);
	UE_LOG(LogTemp, Display, TEXT("WALL_BREACH_TAKEOVER On Closed fired: shown=%d paused=%d"), bWasShown ? 1 : 0,
	       bM_MainWasPausedWhenClosed ? 1 : 0);
}

void UWallBreachTakeoverValidateSubsystem::HandleTurnedOffClosed(bool bWasShown)
{
	++M_TurnedOffClosedCount;
	bM_TurnedOffWasShown = bWasShown;
}

void UWallBreachTakeoverValidateSubsystem::SetTakeoversEnabled(const bool bEnabled) const
{
	URTSSettingsMenuSubsystem* const SettingsSubsystem = GetWorld()->GetGameInstance()->GetSubsystem<
		URTSSettingsMenuSubsystem>();
	if (SettingsSubsystem != nullptr)
	{
		SettingsSubsystem->SetShowInfoTakeoversImmediately(bEnabled);
	}
}

UW_WallBreachTakeover* UWallBreachTakeoverValidateSubsystem::GetActiveWallBreachTakeover() const
{
	const UInfoTakeoverSubsystem* const InfoTakeovers = GetWorld()->GetSubsystem<UInfoTakeoverSubsystem>();
	return InfoTakeovers != nullptr ? Cast<UW_WallBreachTakeover>(InfoTakeovers->GetActiveTakeover()) : nullptr;
}

void UWallBreachTakeoverValidateSubsystem::DismissDifficultyPicker() const
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
			Picker->WidgetTree->FindWidget(WallBreachTakeoverValidateConstants::DifficultyButtonName));
		if (IsValid(NormalButton))
		{
			NormalButton->OnClicked.Broadcast();
		}
	}
}

void UWallBreachTakeoverValidateSubsystem::Capture(const FString& Label) const
{
	const FIntPoint ViewportSize = GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport
		                               ? GEngine->GameViewport->Viewport->GetSizeXY()
		                               : FIntPoint::ZeroValue;
	const FString Folder = FString::Printf(TEXT("%dp"), ViewportSize.Y);
	const FString ScreenshotPath = FPaths::ProjectSavedDir() / TEXT("InfoTakeover") / TEXT("WallBreach") / Folder
		/ (Label + TEXT(".png"));
	FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
	UE_LOG(LogTemp, Display, TEXT("WALL_BREACH_TAKEOVER capture %s"), *ScreenshotPath);
}

void UWallBreachTakeoverValidateSubsystem::Fail(const FString& Reason)
{
	++M_Failures;
	UE_LOG(LogTemp, Error, TEXT("WALL_BREACH_TAKEOVER FAIL %s"), *Reason);
}

void UWallBreachTakeoverValidateSubsystem::Finish()
{
	SetTakeoversEnabled(bM_OriginalSettingValue);
	EnterPhase(EWallBreachTakeoverValidatePhase::Done);
	UE_LOG(LogTemp, Display, TEXT("WALL_BREACH_TAKEOVER done: %d failures -> %s"), M_Failures,
	       M_Failures == 0 ? TEXT("PASS") : TEXT("FAIL"));
	FPlatformMisc::RequestExit(false, TEXT("WallBreachTakeoverValidate"));
}
