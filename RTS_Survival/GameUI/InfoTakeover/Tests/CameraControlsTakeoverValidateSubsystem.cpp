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
	// Glass hover check (real seconds after it starts).
	constexpr float HoverMidSweepProgress = 0.45f;
	constexpr double HoverMidSweepShotSeconds = 0.15;
	constexpr double HoverReleaseFreezeSeconds = 0.3;
	constexpr double HoverCheckOnSeconds = 0.8;
	constexpr double HoverCheckOffSeconds = 1.6;
	constexpr float HoverOnMinimum = 0.9f;
	constexpr float HoverOffMaximum = 0.05f;
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
	// Real seconds each field manual of the kept-queue check stays open.
	constexpr double KeptQueueOpenSeconds = 1.5;
	constexpr int32 KeptQueueRequestCount = 3;
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
	case ECameraControlsValidatePhase::WatchingKeptQueue:
		TickWatchingKeptQueue();
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
	if (TickPageShots(*Takeover) && TickHoverCheck(*Takeover))
	{
		PressContinueToNextPage(*Takeover);
	}
}

void UCameraControlsTakeoverValidateSubsystem::CheckPageControls(const UW_CameraControlsTakeover& Takeover,
                                                                 const TCHAR* Context)
{
	const int32 PageIndex = Takeover.GetCurrentPageIndex();
	const int32 LampCount = Takeover.GetPageLampCount();
	const int32 LitLamp = Takeover.GetLitPageLampIndex();
	const bool bPreviousVisible = Takeover.GetIsPreviousButtonVisible();
	UE_LOG(LogTemp, Display, TEXT("CAMERA_TAKEOVER page controls (%s): page %d/%d, lamps %d, lit lamp %d, "
		       "PREVIOUS visible=%d"), Context, PageIndex + 1, Takeover.GetPageCount(), LampCount, LitLamp + 1,
	       bPreviousVisible ? 1 : 0);
	if (LampCount != Takeover.GetPageCount() || LitLamp != PageIndex)
	{
		Fail(FString::Printf(TEXT("the page lamps do not show page %d of %d (%s)"), PageIndex + 1,
		                     Takeover.GetPageCount(), Context));
	}
	if (bPreviousVisible != (PageIndex > 0))
	{
		Fail(FString::Printf(TEXT("PREVIOUS must show only from page two on (%s)"), Context));
	}
}

void UCameraControlsTakeoverValidateSubsystem::PressPreviousOnLastPage(UW_CameraControlsTakeover& Takeover)
{
	bM_HasPressedPrevious = true;
	const int32 ExpectedPageIndex = Takeover.GetCurrentPageIndex() - 1;
	Takeover.PressPreviousButton();
	const bool bStillOpen = GetActiveCameraTakeover() == &Takeover && Takeover.IsInViewport();
	const bool bStillPaused = UGameplayStatics::IsGamePaused(this);
	UE_LOG(LogTemp, Display, TEXT("CAMERA_TAKEOVER previous: page %d (expected %d), still open=%d, paused=%d, "
		       "On Closed count=%d"), Takeover.GetCurrentPageIndex() + 1, ExpectedPageIndex + 1, bStillOpen ? 1 : 0,
	       bStillPaused ? 1 : 0, M_MainRequest.ClosedCount);
	if (Takeover.GetCurrentPageIndex() != ExpectedPageIndex || not bStillOpen || not bStillPaused)
	{
		Fail(TEXT("PREVIOUS did not show the page before on the still paused takeover"));
	}
	if (M_MainRequest.ClosedCount != 0)
	{
		Fail(TEXT("On Closed fired after PREVIOUS"));
	}
	CheckPageControls(Takeover, TEXT("previous"));
	Capture(TEXT("CameraControls_previous_back_to_page2"));
	// Watch the page again (shots, then CONTINUE back to the last page).
	StartWatchingPage(Takeover);
}

int32 UCameraControlsTakeoverValidateSubsystem::FindGlassElement(const UW_CameraControlsTakeover& Takeover,
                                                                 const FString& NamePrefix)
{
	for (int32 ElementIndex = 0; ElementIndex < Takeover.GetGlassElementCount(); ++ElementIndex)
	{
		if (Takeover.GetGlassElementName(ElementIndex).ToString().StartsWith(NamePrefix))
		{
			return ElementIndex;
		}
	}
	return INDEX_NONE;
}

void UCameraControlsTakeoverValidateSubsystem::StartHoverCheck(UW_CameraControlsTakeover& Takeover)
{
	using namespace CameraControlsValidateConstants;
	bM_HasStartedHoverCheck = true;
	M_HoverCheckStartSeconds = FPlatformTime::Seconds();
	M_HoverPaneIndex = FindGlassElement(Takeover, TEXT("Pane"));
	M_HoverButtonIndex = FindGlassElement(Takeover, TEXT("Button_Confirm"));
	UE_LOG(LogTemp, Display, TEXT("CAMERA_TAKEOVER glass elements: %d, hovering %s and %s"),
	       Takeover.GetGlassElementCount(), *Takeover.GetGlassElementName(M_HoverPaneIndex).ToString(),
	       *Takeover.GetGlassElementName(M_HoverButtonIndex).ToString());
	if (M_HoverPaneIndex == INDEX_NONE || M_HoverButtonIndex == INDEX_NONE)
	{
		Fail(TEXT("no glass hover on the card panes or the footer button"));
		bM_HasFinishedHoverCheck = true;
		return;
	}
	Takeover.SetGlassElementForcedHover(M_HoverPaneIndex, true, HoverMidSweepProgress);
	Takeover.SetGlassElementForcedHover(M_HoverButtonIndex, true);
}

void UCameraControlsTakeoverValidateSubsystem::CheckHoverValues(const UW_CameraControlsTakeover& Takeover,
                                                                const bool bExpectHovered)
{
	using namespace CameraControlsValidateConstants;
	const float PaneHover = Takeover.GetGlassElementHover(M_HoverPaneIndex);
	const float ButtonHover = Takeover.GetGlassElementHover(M_HoverButtonIndex);
	UE_LOG(LogTemp, Display, TEXT("CAMERA_TAKEOVER glass hover %s: pane %.3f, button %.3f"),
	       bExpectHovered ? TEXT("on") : TEXT("off"), PaneHover, ButtonHover);
	const bool bOk = bExpectHovered
		                 ? PaneHover >= HoverOnMinimum && ButtonHover >= HoverOnMinimum
		                 : PaneHover >= 0.f && PaneHover <= HoverOffMaximum && ButtonHover >= 0.f
		                 && ButtonHover <= HoverOffMaximum;
	if (not bOk)
	{
		Fail(bExpectHovered ? TEXT("the glass Hover did not ramp up") : TEXT("the glass Hover did not ramp back"));
	}
}

bool UCameraControlsTakeoverValidateSubsystem::TickHoverCheck(UW_CameraControlsTakeover& Takeover)
{
	using namespace CameraControlsValidateConstants;
	if (bM_HasFinishedHoverCheck)
	{
		return true;
	}
	if (not bM_HasStartedHoverCheck)
	{
		StartHoverCheck(Takeover);
		return bM_HasFinishedHoverCheck;
	}
	const double Seconds = FPlatformTime::Seconds() - M_HoverCheckStartSeconds;
	if (not bM_HasHoverMidSweepShot && Seconds >= HoverMidSweepShotSeconds)
	{
		bM_HasHoverMidSweepShot = true;
		Capture(TEXT("CameraControls_hover_midsweep"));
	}
	if (not bM_HasReleasedSweepFreeze && Seconds >= HoverReleaseFreezeSeconds)
	{
		bM_HasReleasedSweepFreeze = true;
		Takeover.SetGlassElementForcedHover(M_HoverPaneIndex, true);
	}
	if (not bM_HasCheckedHoverOn && Seconds >= HoverCheckOnSeconds)
	{
		bM_HasCheckedHoverOn = true;
		CheckHoverValues(Takeover, true);
		Capture(TEXT("CameraControls_hover"));
		Takeover.SetGlassElementForcedHover(M_HoverPaneIndex, false);
		Takeover.SetGlassElementForcedHover(M_HoverButtonIndex, false);
	}
	if (Seconds < HoverCheckOffSeconds)
	{
		return false;
	}
	CheckHoverValues(Takeover, false);
	bM_HasFinishedHoverCheck = true;
	return true;
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
	CheckPageControls(Takeover, TEXT("open"));
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
	CheckPageControls(Takeover, TEXT("continue"));
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
	if (not bM_HasPressedPrevious)
	{
		// Back to page two once, then CONTINUE returns here and UNDERSTOOD closes.
		PressPreviousOnLastPage(*Takeover);
		return;
	}
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
			StartKeptQueueCheck();
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
	// "Never show again" only hides unit intel: the camera field manual must still open, and without the box. It stays
	// open as the first takeover of the kept-queue check.
	SetTakeoversEnabled(false);
	StartRequest(M_TurnedOffRequest)->OnClosed.AddDynamic(
		this, &UCameraControlsTakeoverValidateSubsystem::HandleTurnedOffClosed);
	M_TurnedOffRequest.Action->Activate();
	const UW_CameraControlsTakeover* const Takeover = GetActiveCameraTakeover();
	const bool bOpened = Takeover != nullptr;
	const bool bOffersNeverShowAgain = bOpened && Takeover->GetOffersNeverShowAgain();
	UE_LOG(LogTemp, Display, TEXT("CAMERA_TAKEOVER turned off: opened=%d, closed %d times at once, never-show box=%d"),
	       bOpened ? 1 : 0, M_TurnedOffRequest.ClosedCount, bOffersNeverShowAgain ? 1 : 0);
	if (not bOpened || M_TurnedOffRequest.ClosedCount != 0 || bOffersNeverShowAgain)
	{
		Fail(TEXT("with unit intel off, the field manual must still open, without the never-show box"));
	}
}

void UCameraControlsTakeoverValidateSubsystem::StartKeptQueueCheck()
{
	// Queued behind the open one while unit intel is off: closing each takeover runs the queue's unit-intel drop, which
	// must keep these field manuals.
	StartRequest(M_FirstQueuedRequest)->OnClosed.AddDynamic(
		this, &UCameraControlsTakeoverValidateSubsystem::HandleFirstQueuedClosed);
	StartRequest(M_KeptRequest)->OnClosed.AddDynamic(
		this, &UCameraControlsTakeoverValidateSubsystem::HandleKeptClosed);
	M_FirstQueuedRequest.Action->Activate();
	M_KeptRequest.Action->Activate();
	M_WatchedTakeover = nullptr;
	M_KeptQueueOpenedCount = 0;
	EnterPhase(ECameraControlsValidatePhase::WatchingKeptQueue);
}

void UCameraControlsTakeoverValidateSubsystem::TickWatchingKeptQueue()
{
	using namespace CameraControlsValidateConstants;
	UW_CameraControlsTakeover* const ActiveTakeover = GetActiveCameraTakeover();
	if (ActiveTakeover == nullptr)
	{
		if (M_KeptQueueOpenedCount >= KeptQueueRequestCount)
		{
			CheckKeptQueueClosed();
			return;
		}
		if (GetSecondsInPhase() > OpenTimeoutSeconds)
		{
			Fail(FString::Printf(TEXT("only %d of %d queued field manuals opened with unit intel off"),
			                     M_KeptQueueOpenedCount, KeptQueueRequestCount));
			Finish();
		}
		return;
	}
	if (ActiveTakeover != M_WatchedTakeover.Get())
	{
		StartWatchingKeptQueueTakeover(*ActiveTakeover);
		return;
	}
	if (FPlatformTime::Seconds() - M_WatchedOpenSeconds < KeptQueueOpenSeconds)
	{
		return;
	}
	CloseTakeover(*ActiveTakeover);
	// Restarts the open timeout for the next field manual.
	EnterPhase(ECameraControlsValidatePhase::WatchingKeptQueue);
}

void UCameraControlsTakeoverValidateSubsystem::StartWatchingKeptQueueTakeover(UW_CameraControlsTakeover& Takeover)
{
	++M_KeptQueueOpenedCount;
	M_WatchedTakeover = &Takeover;
	M_WatchedOpenSeconds = FPlatformTime::Seconds();
	const bool bOffersNeverShowAgain = Takeover.GetOffersNeverShowAgain();
	UE_LOG(LogTemp, Display, TEXT("CAMERA_TAKEOVER kept queue: field manual %d opened, never-show box=%d"),
	       M_KeptQueueOpenedCount, bOffersNeverShowAgain ? 1 : 0);
	if (bOffersNeverShowAgain)
	{
		Fail(TEXT("a field manual offered the never-show box"));
	}
	// The test hook must do nothing here: no box, and closing must not write the setting.
	Takeover.SetNeverShowAgainChecked(true);
}

void UCameraControlsTakeoverValidateSubsystem::CheckKeptQueueClosed()
{
	UE_LOG(LogTemp, Display, TEXT("CAMERA_TAKEOVER kept queue: turned-off closed %d shown=%d; first closed %d shown=%d; "
		       "kept closed %d shown=%d"), M_TurnedOffRequest.ClosedCount, M_TurnedOffRequest.bWasShown ? 1 : 0,
	       M_FirstQueuedRequest.ClosedCount, M_FirstQueuedRequest.bWasShown ? 1 : 0, M_KeptRequest.ClosedCount,
	       M_KeptRequest.bWasShown ? 1 : 0);
	const bool bAllShownOnce = M_TurnedOffRequest.ClosedCount == 1 && M_TurnedOffRequest.bWasShown
		&& M_FirstQueuedRequest.ClosedCount == 1 && M_FirstQueuedRequest.bWasShown
		&& M_KeptRequest.ClosedCount == 1 && M_KeptRequest.bWasShown;
	if (not bAllShownOnce)
	{
		Fail(TEXT("with unit intel off, every queued field manual must open and close once as shown"));
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

void UCameraControlsTakeoverValidateSubsystem::HandleKeptClosed(bool bWasShown)
{
	RecordClosed(M_KeptRequest, bWasShown);
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
