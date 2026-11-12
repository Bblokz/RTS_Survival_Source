// Copyright (C) Bas Blokzijl - All rights reserved.

#include "CameraControlsTakeoverValidateSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
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
#include "RTS_Survival/GameUI/InfoTakeover/W_CameraControlsTakeover.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/Subsystems/SettingsMenuSubsystem/RTSSettingsMenuSubsystem.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "UnrealClient.h"

namespace CameraControlsValidateConstants
{
	const TCHAR* const CommandLineSwitch = TEXT("CameraControlsTakeoverValidate");
	constexpr double HudBuildSeconds = 5.0;
	constexpr double StartGameSeconds = 3.0;
	constexpr double OpenTimeoutSeconds = 10.0;
	// Real seconds after the takeover opened.
	constexpr double TitleShotSeconds = 0.4;
	// Page one loops in 4 and 6 s, the panning page in 8 s, the marquee page in 5 s; the shots spread over most of a
	// loop once the typing is done.
	constexpr double FirstFrameShotSeconds = 4.0;
	constexpr double FrameShotIntervalSeconds = 0.75;
	constexpr double SidePageFirstFrameShotSeconds = 3.5;
	constexpr double SidePageFrameShotIntervalSeconds = 0.65;
	constexpr int32 CameraPageIllustrationCount = 3;
	constexpr int32 SidePageIllustrationCount = 1;
	constexpr int32 FrameShotCount = 8;
	constexpr double CloseAfterLastShotSeconds = 0.75;
	// Real seconds the node waits behind the start-game screen before the run presses start.
	constexpr double BeforeStartWaitSeconds = 1.5;
	constexpr double SettleAfterCloseSeconds = 0.5;
	constexpr double NextCheckAfterCloseSeconds = 1.0;
	constexpr double NeverShowAgainTickSeconds = 1.5;
	constexpr float MinSignatureChange = 0.01f;
	const FName DifficultyButtonName(TEXT("M_NormalDifficultyButton"));
	const FName ContinueFunctionName(TEXT("OnClickedContinue"));
}

bool UCameraControlsTakeoverValidateSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	const UWorld* const OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), CameraControlsValidateConstants::CommandLineSwitch);
#endif
}

void UCameraControlsTakeoverValidateSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	RTSFunctionLibrary::PushModalDialogSuppression();
	const URTSGameUserSettings* const GameUserSettings = URTSGameUserSettings::Get();
	bM_OriginalSettingValue = GameUserSettings == nullptr || GameUserSettings->GetShowInfoTakeovers();
	EnterPhase(ECameraControlsValidatePhase::WaitingForHud);
	UE_LOG(LogTemp, Display, TEXT("CAMERA_TAKEOVER validate started (setting was %s)"),
	       bM_OriginalSettingValue ? TEXT("on") : TEXT("off"));
}

void UCameraControlsTakeoverValidateSubsystem::Tick(float DeltaTime)
{
	switch (M_Phase)
	{
	case ECameraControlsValidatePhase::WaitingForHud:
		TickWaitingForHud();
		break;
	case ECameraControlsValidatePhase::WaitingBeforeStart:
		TickWaitingBeforeStart();
		break;
	case ECameraControlsValidatePhase::StartingGame:
		TickStartingGame();
		break;
	case ECameraControlsValidatePhase::WatchingTakeover:
		TickWatchingTakeover();
		break;
	case ECameraControlsValidatePhase::WatchingLaterPage:
		TickWatchingLaterPage();
		break;
	case ECameraControlsValidatePhase::CheckingClosed:
		TickCheckingClosed();
		break;
	case ECameraControlsValidatePhase::WatchingDroppedQueue:
		TickWatchingDroppedQueue();
		break;
	default:
		break;
	}
}

TStatId UCameraControlsTakeoverValidateSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCameraControlsTakeoverValidateSubsystem, STATGROUP_Tickables);
}

void UCameraControlsTakeoverValidateSubsystem::EnterPhase(const ECameraControlsValidatePhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartSeconds = FPlatformTime::Seconds();
}

double UCameraControlsTakeoverValidateSubsystem::GetSecondsInPhase() const
{
	return FPlatformTime::Seconds() - M_PhaseStartSeconds;
}

void UCameraControlsTakeoverValidateSubsystem::TickWaitingForHud()
{
	if (GetSecondsInPhase() < CameraControlsValidateConstants::HudBuildSeconds)
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
		CallMainRequest();
		EnterPhase(ECameraControlsValidatePhase::WaitingBeforeStart);
		return;
	}
	UE_LOG(LogTemp, Display, TEXT("CAMERA_TAKEOVER no start-game screen on this map; the before-start check is skipped"));
	EnterPhase(ECameraControlsValidatePhase::StartingGame);
}

void UCameraControlsTakeoverValidateSubsystem::TickWaitingBeforeStart()
{
	if (GetSecondsInPhase() < CameraControlsValidateConstants::BeforeStartWaitSeconds)
	{
		return;
	}
	const bool bOpenedBeforeStart = GetActiveCameraTakeover() != nullptr;
	UE_LOG(LogTemp, Display, TEXT("CAMERA_TAKEOVER before start: opened=%d (must be 0)"), bOpenedBeforeStart ? 1 : 0);
	if (bOpenedBeforeStart)
	{
		Fail(TEXT("the takeover opened over the start-game screen"));
	}
	if (M_StartGameControl.IsValid())
	{
		M_StartGameControl->PlayerStartedGame();
	}
	EnterPhase(ECameraControlsValidatePhase::StartingGame);
}

void UCameraControlsTakeoverValidateSubsystem::TickStartingGame()
{
	DismissDifficultyPicker();
	if (GetSecondsInPhase() < CameraControlsValidateConstants::StartGameSeconds)
	{
		return;
	}
	if (M_MainRequest.Action == nullptr)
	{
		CallMainRequest();
	}
	EnterPhase(ECameraControlsValidatePhase::WatchingTakeover);
}

void UCameraControlsTakeoverValidateSubsystem::CallMainRequest()
{
	StartRequest(M_MainRequest)->OnClosed.AddDynamic(this, &UCameraControlsTakeoverValidateSubsystem::HandleMainClosed);
	M_MainRequest.Action->Activate();
	UE_LOG(LogTemp, Display, TEXT("CAMERA_TAKEOVER node called; closed count right after: %d"),
	       M_MainRequest.ClosedCount);
}

UW_CameraControlsTakeover* UCameraControlsTakeoverValidateSubsystem::WaitForTakeoverToOpen(
	const TCHAR* WhatIsAwaited)
{
	UW_CameraControlsTakeover* const Takeover = GetActiveCameraTakeover();
	if (Takeover != nullptr || GetSecondsInPhase() <= CameraControlsValidateConstants::OpenTimeoutSeconds)
	{
		return Takeover;
	}
	Fail(FString::Printf(TEXT("%s never opened"), WhatIsAwaited));
	Finish();
	return nullptr;
}

void UCameraControlsTakeoverValidateSubsystem::TickWatchingTakeover()
{
	if (not M_WatchedTakeover.IsValid())
	{
		UW_CameraControlsTakeover* const OpenedTakeover = WaitForTakeoverToOpen(TEXT("the camera controls takeover"));
		if (OpenedTakeover == nullptr)
		{
			return;
		}
		StartWatchingMainTakeover(*OpenedTakeover);
	}
	UW_CameraControlsTakeover* const Takeover = GetActiveCameraTakeover();
	if (Takeover == nullptr || Takeover != M_WatchedTakeover.Get())
	{
		return;
	}
	if (TickPageShots(*Takeover))
	{
		PressContinueToNextPage(*Takeover);
	}
}

void UCameraControlsTakeoverValidateSubsystem::StartWatchingMainTakeover(UW_CameraControlsTakeover& Takeover)
{
	using namespace CameraControlsValidateConstants;
	M_WatchedTakeover = &Takeover;
	const int32 CameraCount = Takeover.GetPageIllustrations(ECameraControlsTakeoverPage::CameraControls).Num();
	const int32 PanningCount = Takeover.GetPageIllustrations(ECameraControlsTakeoverPage::CameraPanning).Num();
	const int32 MarqueeCount = Takeover.GetPageIllustrations(ECameraControlsTakeoverPage::MarqueeSelection).Num();
	UE_LOG(LogTemp, Display, TEXT("CAMERA_TAKEOVER opened %s with %d + %d + %d illustrations, page=%d, paused=%d"),
	       *Takeover.GetClass()->GetName(), CameraCount, PanningCount, MarqueeCount,
	       static_cast<int32>(Takeover.GetActivePage()), UGameplayStatics::IsGamePaused(this) ? 1 : 0);
	if (CameraCount != CameraPageIllustrationCount || PanningCount != SidePageIllustrationCount
		|| MarqueeCount != SidePageIllustrationCount)
	{
		Fail(TEXT("expected three illustrations on the camera page and one on the panning and marquee pages"));
	}
	if (Takeover.GetActivePage() != ECameraControlsTakeoverPage::CameraControls)
	{
		Fail(TEXT("the takeover did not open on page one"));
	}
	StartWatchingPage(Takeover);
}

void UCameraControlsTakeoverValidateSubsystem::StartWatchingPage(const UW_CameraControlsTakeover& Takeover)
{
	M_WatchedOpenSeconds = FPlatformTime::Seconds();
	M_NextFrameShot = 0;
	bM_HasTitleShot = false;
	M_IllustrationSignatures.Reset();
	M_IllustrationSignatures.SetNum(Takeover.GetPageIllustrations(Takeover.GetActivePage()).Num());
}

bool UCameraControlsTakeoverValidateSubsystem::TickPageShots(UW_CameraControlsTakeover& Takeover)
{
	using namespace CameraControlsValidateConstants;
	const bool bIsSidePage = Takeover.GetActivePage() != ECameraControlsTakeoverPage::CameraControls;
	const double FirstShotSeconds = bIsSidePage ? SidePageFirstFrameShotSeconds : FirstFrameShotSeconds;
	const double ShotInterval = bIsSidePage ? SidePageFrameShotIntervalSeconds : FrameShotIntervalSeconds;
	const double SecondsOpen = FPlatformTime::Seconds() - M_WatchedOpenSeconds;
	if (not bM_HasTitleShot && SecondsOpen >= TitleShotSeconds)
	{
		bM_HasTitleShot = true;
		Capture(GetShotPrefix(Takeover) + TEXT("0_typing"));
	}
	if (M_NextFrameShot < FrameShotCount && SecondsOpen >= FirstShotSeconds + ShotInterval * M_NextFrameShot)
	{
		CaptureAnimationFrame(Takeover);
		return false;
	}
	const double DoneSeconds = FirstShotSeconds + ShotInterval * FrameShotCount + CloseAfterLastShotSeconds;
	return M_NextFrameShot >= FrameShotCount && SecondsOpen >= DoneSeconds;
}

void UCameraControlsTakeoverValidateSubsystem::PressContinueToNextPage(UW_CameraControlsTakeover& Takeover)
{
	CheckIllustrationsAnimated();
	const int32 ExpectedPageIndex = static_cast<int32>(Takeover.GetActivePage()) + 1;
	// The footer button as the player clicks it: CONTINUE on every page but the last.
	Takeover.PressConfirmButton();
	const int32 ShownPageIndex = static_cast<int32>(Takeover.GetActivePage());
	const bool bOnNextPage = ShownPageIndex == ExpectedPageIndex;
	const bool bStillOpen = GetActiveCameraTakeover() == &Takeover && Takeover.IsInViewport();
	const bool bStillPaused = UGameplayStatics::IsGamePaused(this);
	UE_LOG(LogTemp, Display, TEXT("CAMERA_TAKEOVER continue: page %d (expected %d), still open=%d, paused=%d, "
		       "On Closed count=%d"), ShownPageIndex + 1, ExpectedPageIndex + 1, bStillOpen ? 1 : 0,
	       bStillPaused ? 1 : 0, M_MainRequest.ClosedCount);
	if (not bOnNextPage || not bStillOpen || not bStillPaused)
	{
		Fail(TEXT("CONTINUE did not show the next page on the still paused takeover"));
	}
	if (M_MainRequest.ClosedCount != 0)
	{
		Fail(TEXT("On Closed fired before the last page"));
	}
	StartWatchingPage(Takeover);
	EnterPhase(ECameraControlsValidatePhase::WatchingLaterPage);
}

bool UCameraControlsTakeoverValidateSubsystem::GetIsLastPage(const UW_CameraControlsTakeover& Takeover)
{
	return static_cast<int32>(Takeover.GetActivePage()) + 1 >= static_cast<int32>(ECameraControlsTakeoverPage::Count);
}

void UCameraControlsTakeoverValidateSubsystem::TickWatchingLaterPage()
{
	UW_CameraControlsTakeover* const Takeover = GetActiveCameraTakeover();
	if (Takeover == nullptr || Takeover != M_WatchedTakeover.Get())
	{
		Fail(TEXT("the takeover closed before UNDERSTOOD on the last page"));
		Finish();
		return;
	}
	if (not TickPageShots(*Takeover))
	{
		return;
	}
	if (not GetIsLastPage(*Takeover))
	{
		PressContinueToNextPage(*Takeover);
		return;
	}
	CheckIllustrationsAnimated();
	if (M_MainRequest.ClosedCount != 0)
	{
		Fail(TEXT("On Closed fired before UNDERSTOOD on the last page"));
	}
	// UNDERSTOOD on the last page closes the takeover.
	Takeover->PressConfirmButton();
	EnterPhase(ECameraControlsValidatePhase::CheckingClosed);
}

void UCameraControlsTakeoverValidateSubsystem::CaptureAnimationFrame(const UW_CameraControlsTakeover& Takeover)
{
	if (not UGameplayStatics::IsGamePaused(this))
	{
		Fail(TEXT("the game is not paused while the takeover is open"));
	}
	RecordIllustrationSignatures(Takeover);
	Capture(FString::Printf(TEXT("%sframe_%d"), *GetShotPrefix(Takeover), M_NextFrameShot + 1));
	++M_NextFrameShot;
}

FString UCameraControlsTakeoverValidateSubsystem::GetShotPrefix(const UW_CameraControlsTakeover& Takeover) const
{
	const int32 PageIndex = static_cast<int32>(Takeover.GetActivePage());
	return PageIndex == 0
		       ? FString(TEXT("CameraControls_"))
		       : FString::Printf(TEXT("CameraControls_page%d_"), PageIndex + 1);
}

void UCameraControlsTakeoverValidateSubsystem::RecordIllustrationSignatures(const UW_CameraControlsTakeover& Takeover)
{
	const TArray<UUserWidget*> Illustrations = Takeover.GetPageIllustrations(Takeover.GetActivePage());
	FString SignatureLine;
	for (int32 IllustrationIndex = 0; IllustrationIndex < Illustrations.Num(); ++IllustrationIndex)
	{
		const UUserWidget* const Illustration = Illustrations[IllustrationIndex];
		const float Signature = IsValid(Illustration) ? GetIllustrationSignature(*Illustration) : 0.f;
		if (M_IllustrationSignatures.IsValidIndex(IllustrationIndex))
		{
			M_IllustrationSignatures[IllustrationIndex].Add(Signature);
		}
		SignatureLine += FString::Printf(TEXT(" %s=%.3f"), IsValid(Illustration)
			                                                  ? *Illustration->GetClass()->GetName()
			                                                  : TEXT("none"), Signature);
	}
	UE_LOG(LogTemp, Display, TEXT("CAMERA_TAKEOVER %sframe %d paused=%d:%s"), *GetShotPrefix(Takeover),
	       M_NextFrameShot + 1, UGameplayStatics::IsGamePaused(this) ? 1 : 0, *SignatureLine);
}

float UCameraControlsTakeoverValidateSubsystem::GetIllustrationSignature(const UUserWidget& Illustration)
{
	if (not IsValid(Illustration.WidgetTree))
	{
		return 0.f;
	}
	float Signature = 0.f;
	Illustration.WidgetTree->ForEachWidget([&Signature](UWidget* const Widget)
	{
		// Read from Slate, not the UPROPERTY: a track that only writes the property moves nothing on screen.
		const TSharedPtr<SWidget> SlateWidget = Widget->GetCachedWidget();
		if (not SlateWidget.IsValid())
		{
			return;
		}
		const TOptional<FSlateRenderTransform>& SlateTransform = SlateWidget->GetRenderTransform();
		if (SlateTransform.IsSet())
		{
			const FVector2f MovedPoint = SlateTransform.GetValue().TransformPoint(FVector2f(1.f, 1.f));
			Signature += MovedPoint.X + MovedPoint.Y;
		}
		Signature += SlateWidget->GetRenderOpacity();
		if (const UImage* const Image = Cast<UImage>(Widget))
		{
			const FLinearColor Tint = Image->GetColorAndOpacity();
			Signature += Tint.R + Tint.G + Tint.B + Tint.A;
		}
	});
	return Signature;
}

void UCameraControlsTakeoverValidateSubsystem::CheckIllustrationsAnimated()
{
	using namespace CameraControlsValidateConstants;
	for (int32 IllustrationIndex = 0; IllustrationIndex < M_IllustrationSignatures.Num(); ++IllustrationIndex)
	{
		const TArray<float>& Signatures = M_IllustrationSignatures[IllustrationIndex];
		float MinSignature = TNumericLimits<float>::Max();
		float MaxSignature = TNumericLimits<float>::Lowest();
		for (const float Signature : Signatures)
		{
			MinSignature = FMath::Min(MinSignature, Signature);
			MaxSignature = FMath::Max(MaxSignature, Signature);
		}
		const bool bAnimated = Signatures.Num() > 1 && MaxSignature - MinSignature > MinSignatureChange;
		UE_LOG(LogTemp, Display, TEXT("CAMERA_TAKEOVER illustration %d animated while paused: %s (range %.3f)"),
		       IllustrationIndex, bAnimated ? TEXT("yes") : TEXT("NO"), MaxSignature - MinSignature);
		if (not bAnimated)
		{
			Fail(FString::Printf(TEXT("illustration %d did not animate while paused"), IllustrationIndex));
		}
	}
}

void UCameraControlsTakeoverValidateSubsystem::CloseTakeover(UW_CameraControlsTakeover& Takeover) const
{
	// The same path the map smoke tests use: OnClickedContinue is protected but a UFUNCTION.
	UFunction* const ContinueFunction = Takeover.FindFunction(CameraControlsValidateConstants::ContinueFunctionName);
	if (ContinueFunction == nullptr)
	{
		return;
	}
	Takeover.ProcessEvent(ContinueFunction, nullptr);
}

void UCameraControlsTakeoverValidateSubsystem::TickCheckingClosed()
{
	if (GetSecondsInPhase() < CameraControlsValidateConstants::SettleAfterCloseSeconds)
	{
		return;
	}
	if (bM_HasCapturedAfterClose)
	{
		// A frame later than the capture, so the screenshot shows the HUD and not the next test's takeover.
		if (GetSecondsInPhase() >= CameraControlsValidateConstants::NextCheckAfterCloseSeconds)
		{
			CheckTurnedOffRequest();
			StartDroppedQueueCheck();
		}
		return;
	}
	bM_HasCapturedAfterClose = true;
	UE_LOG(LogTemp, Display, TEXT("CAMERA_TAKEOVER main request: closed %d times, shown=%d, paused when closed=%d, "
		       "paused now=%d"), M_MainRequest.ClosedCount, M_MainRequest.bWasShown ? 1 : 0,
	       M_MainRequest.bWasPausedWhenClosed ? 1 : 0, UGameplayStatics::IsGamePaused(this) ? 1 : 0);
	if (M_MainRequest.ClosedCount != 1 || not M_MainRequest.bWasShown)
	{
		Fail(TEXT("On Closed did not fire exactly once with Was Shown after Understood"));
	}
	if (M_MainRequest.bWasPausedWhenClosed || UGameplayStatics::IsGamePaused(this))
	{
		Fail(TEXT("the game was still paused when On Closed fired"));
	}
	// The unpaused HUD with the archive pop-up of the closed takeover.
	Capture(TEXT("CameraControls_9_after_close"));
}

void UCameraControlsTakeoverValidateSubsystem::CheckTurnedOffRequest()
{
	SetTakeoversEnabled(false);
	StartRequest(M_TurnedOffRequest)->OnClosed.AddDynamic(
		this, &UCameraControlsTakeoverValidateSubsystem::HandleTurnedOffClosed);
	M_TurnedOffRequest.Action->Activate();
	const bool bOpened = GetActiveCameraTakeover() != nullptr;
	UE_LOG(LogTemp, Display, TEXT("CAMERA_TAKEOVER turned off: closed %d times at once, shown=%d, opened=%d"),
	       M_TurnedOffRequest.ClosedCount, M_TurnedOffRequest.bWasShown ? 1 : 0, bOpened ? 1 : 0);
	if (M_TurnedOffRequest.ClosedCount != 1 || M_TurnedOffRequest.bWasShown || bOpened)
	{
		Fail(TEXT("with the takeovers off, On Closed must fire at once with Was Shown false"));
	}
	SetTakeoversEnabled(true);
}

void UCameraControlsTakeoverValidateSubsystem::StartDroppedQueueCheck()
{
	StartRequest(M_FirstQueuedRequest)->OnClosed.AddDynamic(
		this, &UCameraControlsTakeoverValidateSubsystem::HandleFirstQueuedClosed);
	StartRequest(M_DroppedRequest)->OnClosed.AddDynamic(
		this, &UCameraControlsTakeoverValidateSubsystem::HandleDroppedClosed);
	M_FirstQueuedRequest.Action->Activate();
	M_DroppedRequest.Action->Activate();
	M_WatchedTakeover = nullptr;
	EnterPhase(ECameraControlsValidatePhase::WatchingDroppedQueue);
}

void UCameraControlsTakeoverValidateSubsystem::TickWatchingDroppedQueue()
{
	using namespace CameraControlsValidateConstants;
	if (not bM_HasDroppedQueueTakeoverOpened)
	{
		UW_CameraControlsTakeover* const OpenedTakeover = WaitForTakeoverToOpen(TEXT("the first queued takeover"));
		if (OpenedTakeover == nullptr)
		{
			return;
		}
		bM_HasDroppedQueueTakeoverOpened = true;
		M_WatchedTakeover = OpenedTakeover;
		M_WatchedOpenSeconds = FPlatformTime::Seconds();
		return;
	}
	const double SecondsOpen = FPlatformTime::Seconds() - M_WatchedOpenSeconds;
	if (M_FirstQueuedRequest.ClosedCount == 0)
	{
		if (SecondsOpen < NeverShowAgainTickSeconds)
		{
			return;
		}
		UW_CameraControlsTakeover* const FirstTakeover = M_WatchedTakeover.Get();
		if (FirstTakeover == nullptr)
		{
			Fail(TEXT("the first queued takeover vanished before it was closed"));
			Finish();
			return;
		}
		FirstTakeover->SetNeverShowAgainChecked(true);
		CloseTakeover(*FirstTakeover);
		return;
	}
	if (SecondsOpen < NeverShowAgainTickSeconds + SettleAfterCloseSeconds)
	{
		return;
	}
	const bool bAnotherOpened = GetActiveCameraTakeover() != nullptr;
	UE_LOG(LogTemp, Display, TEXT("CAMERA_TAKEOVER queue drop: first closed %d shown=%d; dropped closed %d shown=%d; "
		       "another opened=%d"), M_FirstQueuedRequest.ClosedCount, M_FirstQueuedRequest.bWasShown ? 1 : 0,
	       M_DroppedRequest.ClosedCount, M_DroppedRequest.bWasShown ? 1 : 0, bAnotherOpened ? 1 : 0);
	if (M_FirstQueuedRequest.ClosedCount != 1 || not M_FirstQueuedRequest.bWasShown)
	{
		Fail(TEXT("the first queued request did not close as shown"));
	}
	if (M_DroppedRequest.ClosedCount != 1 || M_DroppedRequest.bWasShown || bAnotherOpened)
	{
		Fail(TEXT("the request dropped by never show again did not finish once as not shown"));
	}
	Finish();
}

UAsyncShowCameraControlsTakeover* UCameraControlsTakeoverValidateSubsystem::StartRequest(
	FCameraControlsValidateRequest& Request)
{
	Request = FCameraControlsValidateRequest();
	// The node as a mission graph calls it; any game-world object serves as the context.
	Request.Action = UAsyncShowCameraControlsTakeover::ShowCameraControlsTakeover(M_PlayerController.Get());
	return Request.Action;
}

void UCameraControlsTakeoverValidateSubsystem::HandleMainClosed(bool bWasShown)
{
	RecordClosed(M_MainRequest, bWasShown);
}

void UCameraControlsTakeoverValidateSubsystem::HandleTurnedOffClosed(bool bWasShown)
{
	RecordClosed(M_TurnedOffRequest, bWasShown);
}

void UCameraControlsTakeoverValidateSubsystem::HandleFirstQueuedClosed(bool bWasShown)
{
	RecordClosed(M_FirstQueuedRequest, bWasShown);
}

void UCameraControlsTakeoverValidateSubsystem::HandleDroppedClosed(bool bWasShown)
{
	RecordClosed(M_DroppedRequest, bWasShown);
}

void UCameraControlsTakeoverValidateSubsystem::RecordClosed(FCameraControlsValidateRequest& Request,
                                                            const bool bWasShown) const
{
	++Request.ClosedCount;
	Request.bWasShown = bWasShown;
	Request.bWasPausedWhenClosed = UGameplayStatics::IsGamePaused(this);
	UE_LOG(LogTemp, Display, TEXT("CAMERA_TAKEOVER On Closed fired: shown=%d paused=%d"), bWasShown ? 1 : 0,
	       Request.bWasPausedWhenClosed ? 1 : 0);
}

void UCameraControlsTakeoverValidateSubsystem::SetTakeoversEnabled(const bool bEnabled) const
{
	URTSSettingsMenuSubsystem* const SettingsSubsystem = GetWorld()->GetGameInstance()->GetSubsystem<
		URTSSettingsMenuSubsystem>();
	if (SettingsSubsystem != nullptr)
	{
		SettingsSubsystem->SetShowInfoTakeoversImmediately(bEnabled);
	}
}

UW_CameraControlsTakeover* UCameraControlsTakeoverValidateSubsystem::GetActiveCameraTakeover() const
{
	const UInfoTakeoverSubsystem* const InfoTakeovers = GetWorld()->GetSubsystem<UInfoTakeoverSubsystem>();
	return InfoTakeovers != nullptr ? Cast<UW_CameraControlsTakeover>(InfoTakeovers->GetActiveTakeover()) : nullptr;
}

void UCameraControlsTakeoverValidateSubsystem::DismissDifficultyPicker() const
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
			Picker->WidgetTree->FindWidget(CameraControlsValidateConstants::DifficultyButtonName));
		if (IsValid(NormalButton))
		{
			NormalButton->OnClicked.Broadcast();
		}
	}
}

void UCameraControlsTakeoverValidateSubsystem::Capture(const FString& Label) const
{
	const FIntPoint ViewportSize = GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport
		                               ? GEngine->GameViewport->Viewport->GetSizeXY()
		                               : FIntPoint::ZeroValue;
	const FString Folder = FString::Printf(TEXT("%dp"), ViewportSize.Y);
	const FString ScreenshotPath = FPaths::ProjectSavedDir() / TEXT("InfoTakeover") / TEXT("CameraControls") / Folder
		/ (Label + TEXT(".png"));
	FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
	UE_LOG(LogTemp, Display, TEXT("CAMERA_TAKEOVER capture %s"), *ScreenshotPath);
}

void UCameraControlsTakeoverValidateSubsystem::Fail(const FString& Reason)
{
	++M_Failures;
	UE_LOG(LogTemp, Error, TEXT("CAMERA_TAKEOVER FAIL %s"), *Reason);
}

void UCameraControlsTakeoverValidateSubsystem::Finish()
{
	SetTakeoversEnabled(bM_OriginalSettingValue);
	EnterPhase(ECameraControlsValidatePhase::Done);
	UE_LOG(LogTemp, Display, TEXT("CAMERA_TAKEOVER done: %d failures -> %s"), M_Failures,
	       M_Failures == 0 ? TEXT("PASS") : TEXT("FAIL"));
	FPlatformMisc::RequestExit(false, TEXT("CameraControlsTakeoverValidate"));
}
