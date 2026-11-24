// Copyright (C) Bas Blokzijl - All rights reserved.

#include "NomadicConversionTakeoverValidateSubsystem.h"

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
#include "RTS_Survival/GameUI/InfoTakeover/W_NomadicConversionTakeover.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/Subsystems/SettingsMenuSubsystem/RTSSettingsMenuSubsystem.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "UnrealClient.h"

namespace NomadicConversionTakeoverValidateConstants
{
	const TCHAR* const CommandLineSwitch = TEXT("NomadicConversionTakeoverValidate");
	constexpr double HudBuildSeconds = 5.0;
	constexpr double BeforeStartWaitSeconds = 1.5;
	constexpr double StartGameSeconds = 3.0;
	constexpr double OpenTimeoutSeconds = 10.0;
	// Real seconds after the takeover opened.
	constexpr double TypingShotSeconds = 0.4;
	// The illustration loops in 10 s (DeployLoop); eight frames 1.2 s apart cover most of one loop.
	constexpr double FirstFrameShotSeconds = 1.0;
	constexpr double FrameShotIntervalSeconds = 1.2;
	constexpr int32 FrameShotCount = 8;
	// The finished shot waits this long at most for the typewriter.
	constexpr double MaxTypingWaitSeconds = 120.0;
	constexpr double SettleAfterTypingSeconds = 0.5;
	constexpr double PressUnderstoodAfterDoneShotSeconds = 1.0;
	constexpr double SettleAfterCloseSeconds = 0.5;
	constexpr double NextCheckAfterCloseSeconds = 1.0;
	constexpr float MinSignatureChange = 0.01f;
	const FName DifficultyButtonName(TEXT("M_NormalDifficultyButton"));
}

bool UNomadicConversionTakeoverValidateSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	const UWorld* const OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), NomadicConversionTakeoverValidateConstants::CommandLineSwitch);
#endif
}

void UNomadicConversionTakeoverValidateSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	RTSFunctionLibrary::PushModalDialogSuppression();
	const URTSGameUserSettings* const GameUserSettings = URTSGameUserSettings::Get();
	bM_OriginalSettingValue = GameUserSettings == nullptr || GameUserSettings->GetShowInfoTakeovers();
	EnterPhase(ENomadicConversionTakeoverValidatePhase::WaitingForHud);
	UE_LOG(LogTemp, Display, TEXT("NOMADIC_TAKEOVER validate started (setting was %s)"),
	       bM_OriginalSettingValue ? TEXT("on") : TEXT("off"));
}

void UNomadicConversionTakeoverValidateSubsystem::Tick(float DeltaTime)
{
	switch (M_Phase)
	{
	case ENomadicConversionTakeoverValidatePhase::WaitingForHud:
		TickWaitingForHud();
		break;
	case ENomadicConversionTakeoverValidatePhase::WaitingBeforeStart:
		TickWaitingBeforeStart();
		break;
	case ENomadicConversionTakeoverValidatePhase::StartingGame:
		TickStartingGame();
		break;
	case ENomadicConversionTakeoverValidatePhase::WatchingTakeover:
		TickWatchingTakeover();
		break;
	case ENomadicConversionTakeoverValidatePhase::CheckingClosed:
		TickCheckingClosed();
		break;
	default:
		break;
	}
}

TStatId UNomadicConversionTakeoverValidateSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UNomadicConversionTakeoverValidateSubsystem, STATGROUP_Tickables);
}

void UNomadicConversionTakeoverValidateSubsystem::EnterPhase(const ENomadicConversionTakeoverValidatePhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartSeconds = FPlatformTime::Seconds();
}

double UNomadicConversionTakeoverValidateSubsystem::GetSecondsInPhase() const
{
	return FPlatformTime::Seconds() - M_PhaseStartSeconds;
}

void UNomadicConversionTakeoverValidateSubsystem::TickWaitingForHud()
{
	if (GetSecondsInPhase() < NomadicConversionTakeoverValidateConstants::HudBuildSeconds)
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
		EnterPhase(ENomadicConversionTakeoverValidatePhase::WaitingBeforeStart);
		return;
	}
	UE_LOG(LogTemp, Display, TEXT("NOMADIC_TAKEOVER no start-game screen on this map; the before-start check is skipped"));
	EnterPhase(ENomadicConversionTakeoverValidatePhase::StartingGame);
}

void UNomadicConversionTakeoverValidateSubsystem::TickWaitingBeforeStart()
{
	if (GetSecondsInPhase() < NomadicConversionTakeoverValidateConstants::BeforeStartWaitSeconds)
	{
		return;
	}
	const bool bOpenedBeforeStart = GetActiveNomadicConversionTakeover() != nullptr;
	UE_LOG(LogTemp, Display, TEXT("NOMADIC_TAKEOVER before start: opened=%d (must be 0)"), bOpenedBeforeStart ? 1 : 0);
	if (bOpenedBeforeStart)
	{
		Fail(TEXT("the takeover opened over the start-game screen"));
	}
	if (M_StartGameControl.IsValid())
	{
		M_StartGameControl->PlayerStartedGame();
	}
	EnterPhase(ENomadicConversionTakeoverValidatePhase::StartingGame);
}

void UNomadicConversionTakeoverValidateSubsystem::TickStartingGame()
{
	DismissDifficultyPicker();
	if (GetSecondsInPhase() < NomadicConversionTakeoverValidateConstants::StartGameSeconds)
	{
		return;
	}
	if (M_MainNode == nullptr)
	{
		CallMainNode();
	}
	EnterPhase(ENomadicConversionTakeoverValidatePhase::WatchingTakeover);
}

void UNomadicConversionTakeoverValidateSubsystem::CallMainNode()
{
	M_MainNode = UAsyncShowNomadicConversionTakeover::ShowNomadicConversionTakeover(M_PlayerController.Get());
	M_MainNode->OnClosed.AddDynamic(this, &UNomadicConversionTakeoverValidateSubsystem::HandleMainClosed);
	M_MainNode->Activate();
	UE_LOG(LogTemp, Display, TEXT("NOMADIC_TAKEOVER node called; closed count right after: %d"), M_MainClosedCount);
}

void UNomadicConversionTakeoverValidateSubsystem::TickWatchingTakeover()
{
	using namespace NomadicConversionTakeoverValidateConstants;
	UW_NomadicConversionTakeover* const Takeover = GetActiveNomadicConversionTakeover();
	if (not bM_HasOpened)
	{
		if (Takeover != nullptr)
		{
			StartWatching(*Takeover);
			return;
		}
		if (GetSecondsInPhase() > OpenTimeoutSeconds)
		{
			Fail(TEXT("the nomadic conversion takeover never opened"));
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
	if (not TickFrameShots(*Takeover, SecondsOpen))
	{
		return;
	}
	if (not TickDoneShot(*Takeover, SecondsOpen))
	{
		return;
	}
	if (SecondsOpen < M_DoneShotSeconds + PressUnderstoodAfterDoneShotSeconds)
	{
		return;
	}
	CheckIllustrationAnimated();
	if (M_MainClosedCount != 0)
	{
		Fail(TEXT("On Closed fired before UNDERSTOOD"));
	}
	if (Takeover->GetActivePage() == ENomadicConversionTakeoverPage::Conversion)
	{
		ContinueToBuildRadiusPage(*Takeover);
		return;
	}
	// The footer button as the player clicks it.
	Takeover->PressConfirmButton();
	EnterPhase(ENomadicConversionTakeoverValidatePhase::CheckingClosed);
}

void UNomadicConversionTakeoverValidateSubsystem::ContinueToBuildRadiusPage(UW_NomadicConversionTakeover& Takeover)
{
	Takeover.PressConfirmButton();
	const bool bOnRadiusPage = Takeover.GetActivePage() == ENomadicConversionTakeoverPage::BuildRadius;
	const bool bHasRadiusIllustration = IsValid(Takeover.GetRadiusIllustration());
	UE_LOG(LogTemp, Display, TEXT("NOMADIC_TAKEOVER CONTINUE: page=%d of %d, lamps=%d lit=%d, previous=%d, "
		       "radius illustration=%s"), Takeover.GetCurrentPageIndex(), Takeover.GetPageCount(),
	       Takeover.GetPageLampCount(), Takeover.GetLitPageLampIndex(), Takeover.GetIsPreviousButtonVisible() ? 1 : 0,
	       bHasRadiusIllustration ? *Takeover.GetRadiusIllustration()->GetClass()->GetName() : TEXT("none"));
	if (not bOnRadiusPage)
	{
		Fail(TEXT("CONTINUE did not show the build radius page"));
	}
	if (Takeover.GetPageLampCount() != Takeover.GetPageCount() || Takeover.GetLitPageLampIndex() != 1)
	{
		Fail(TEXT("the page lamps do not show page 2 of 2"));
	}
	if (not Takeover.GetIsPreviousButtonVisible())
	{
		Fail(TEXT("PREVIOUS is hidden on the build radius page"));
	}
	if (not bHasRadiusIllustration)
	{
		Fail(TEXT("the looping illustration WBP_NomCnv_Radius did not load"));
	}
	// The second page is watched like the first: typing, frames, finished panel.
	M_OpenSeconds = FPlatformTime::Seconds();
	M_TypingDoneSeconds = -1.0;
	M_DoneShotSeconds = -1.0;
	M_NextFrameShot = 0;
	bM_HasTypingShot = false;
	M_IllustrationSignatures.Reset();
}

const UUserWidget* UNomadicConversionTakeoverValidateSubsystem::GetActivePageIllustration(
	const UW_NomadicConversionTakeover& Takeover)
{
	return Takeover.GetActivePage() == ENomadicConversionTakeoverPage::BuildRadius
		       ? Takeover.GetRadiusIllustration()
		       : Takeover.GetIllustration();
}

FString UNomadicConversionTakeoverValidateSubsystem::GetPageShotPrefix(const UW_NomadicConversionTakeover& Takeover)
{
	return Takeover.GetActivePage() == ENomadicConversionTakeoverPage::BuildRadius
		       ? TEXT("NomadicConversion_radius")
		       : TEXT("NomadicConversion");
}

void UNomadicConversionTakeoverValidateSubsystem::StartWatching(UW_NomadicConversionTakeover& Takeover)
{
	bM_HasOpened = true;
	M_WatchedTakeover = &Takeover;
	M_OpenSeconds = FPlatformTime::Seconds();
	const bool bHasIllustration = IsValid(Takeover.GetIllustration());
	const bool bPaused = UGameplayStatics::IsGamePaused(this);
	UE_LOG(LogTemp, Display, TEXT("NOMADIC_TAKEOVER opened %s, illustration=%s, paused=%d"),
	       *Takeover.GetClass()->GetName(),
	       bHasIllustration ? *Takeover.GetIllustration()->GetClass()->GetName() : TEXT("none"), bPaused ? 1 : 0);
	if (Takeover.GetClass() == UW_NomadicConversionTakeover::StaticClass())
	{
		Fail(TEXT("WBP_NomadicConversionTakeover did not load; the unstyled C++ widget opened"));
	}
	if (not bHasIllustration)
	{
		Fail(TEXT("the looping illustration WBP_NomCnv_Deploy did not load"));
	}
	if (not bPaused)
	{
		Fail(TEXT("the game is not paused while the takeover is open"));
	}
}

bool UNomadicConversionTakeoverValidateSubsystem::TickFrameShots(const UW_NomadicConversionTakeover& Takeover,
                                                                 const double SecondsOpen)
{
	using namespace NomadicConversionTakeoverValidateConstants;
	if (not bM_HasTypingShot)
	{
		if (SecondsOpen >= TypingShotSeconds)
		{
			bM_HasTypingShot = true;
			Capture(GetPageShotPrefix(Takeover) + TEXT("_0_typing"));
		}
		return false;
	}
	if (M_NextFrameShot >= FrameShotCount)
	{
		return true;
	}
	if (SecondsOpen >= FirstFrameShotSeconds + FrameShotIntervalSeconds * M_NextFrameShot)
	{
		CaptureAnimationFrame(Takeover);
	}
	return false;
}

bool UNomadicConversionTakeoverValidateSubsystem::TickDoneShot(const UW_NomadicConversionTakeover& Takeover,
                                                               const double SecondsOpen)
{
	using namespace NomadicConversionTakeoverValidateConstants;
	if (M_DoneShotSeconds >= 0.0)
	{
		return true;
	}
	// A slow (DebugGame, busy GPU) run types slower than the clock; the finished panel waits for the typewriter.
	const bool bTypingDone = Takeover.GetTypingPhase() == EInfoTakeoverTypingPhase::Done;
	if (bTypingDone && M_TypingDoneSeconds < 0.0)
	{
		M_TypingDoneSeconds = SecondsOpen;
	}
	// A frame after the last letter, so the shot shows it.
	const bool bLastLetterDrawn = bTypingDone && SecondsOpen >= M_TypingDoneSeconds + SettleAfterTypingSeconds;
	if (not bLastLetterDrawn && SecondsOpen < MaxTypingWaitSeconds)
	{
		return false;
	}
	if (not bTypingDone)
	{
		Fail(TEXT("the title and description did not finish typing"));
	}
	M_DoneShotSeconds = SecondsOpen;
	Capture(GetPageShotPrefix(Takeover) + TEXT("_1_done"));
	return false;
}

void UNomadicConversionTakeoverValidateSubsystem::CaptureAnimationFrame(const UW_NomadicConversionTakeover& Takeover)
{
	const bool bPaused = UGameplayStatics::IsGamePaused(this);
	if (not bPaused)
	{
		Fail(TEXT("the game is not paused while the takeover is open"));
	}
	const UUserWidget* const Illustration = GetActivePageIllustration(Takeover);
	const float Signature = IsValid(Illustration) ? GetIllustrationSignature(*Illustration) : 0.f;
	M_IllustrationSignatures.Add(Signature);
	UE_LOG(LogTemp, Display, TEXT("NOMADIC_TAKEOVER page %d frame %d paused=%d signature=%.3f"),
	       Takeover.GetCurrentPageIndex() + 1, M_NextFrameShot + 1, bPaused ? 1 : 0, Signature);
	Capture(FString::Printf(TEXT("%s_frame_%d"), *GetPageShotPrefix(Takeover), M_NextFrameShot + 1));
	++M_NextFrameShot;
}

float UNomadicConversionTakeoverValidateSubsystem::GetIllustrationSignature(const UUserWidget& Illustration)
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

void UNomadicConversionTakeoverValidateSubsystem::CheckIllustrationAnimated()
{
	using namespace NomadicConversionTakeoverValidateConstants;
	float MinSignature = TNumericLimits<float>::Max();
	float MaxSignature = TNumericLimits<float>::Lowest();
	for (const float Signature : M_IllustrationSignatures)
	{
		MinSignature = FMath::Min(MinSignature, Signature);
		MaxSignature = FMath::Max(MaxSignature, Signature);
	}
	const bool bAnimated = M_IllustrationSignatures.Num() > 1 && MaxSignature - MinSignature > MinSignatureChange;
	UE_LOG(LogTemp, Display, TEXT("NOMADIC_TAKEOVER illustration animated while paused: %s (range %.3f over %d frames)"),
	       bAnimated ? TEXT("yes") : TEXT("NO"), bAnimated ? MaxSignature - MinSignature : 0.f,
	       M_IllustrationSignatures.Num());
	if (not bAnimated)
	{
		Fail(TEXT("the illustration did not animate while paused"));
	}
}

void UNomadicConversionTakeoverValidateSubsystem::TickCheckingClosed()
{
	using namespace NomadicConversionTakeoverValidateConstants;
	if (GetSecondsInPhase() < SettleAfterCloseSeconds)
	{
		return;
	}
	if (not bM_HasCapturedAfterClose)
	{
		bM_HasCapturedAfterClose = true;
		UE_LOG(LogTemp, Display, TEXT("NOMADIC_TAKEOVER main node: closed %d times, shown=%d, paused when closed=%d, "
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
		Capture(TEXT("NomadicConversion_2_after_close"));
		return;
	}
	if (GetSecondsInPhase() < NextCheckAfterCloseSeconds)
	{
		return;
	}
	CheckShowsWhileTurnedOff();
	Finish();
}

void UNomadicConversionTakeoverValidateSubsystem::CheckShowsWhileTurnedOff()
{
	// "Never show again" only hides unit intel: this field manual must still open, without the box, and close as shown.
	SetTakeoversEnabled(false);
	M_TurnedOffNode = UAsyncShowNomadicConversionTakeover::ShowNomadicConversionTakeover(M_PlayerController.Get());
	M_TurnedOffNode->OnClosed.AddDynamic(this, &UNomadicConversionTakeoverValidateSubsystem::HandleTurnedOffClosed);
	M_TurnedOffNode->Activate();
	UW_NomadicConversionTakeover* const Takeover = GetActiveNomadicConversionTakeover();
	const bool bOpened = Takeover != nullptr;
	const bool bOffersNeverShowAgain = bOpened && Takeover->GetOffersNeverShowAgain();
	UE_LOG(LogTemp, Display, TEXT("NOMADIC_TAKEOVER turned off: opened=%d, closed %d times at once, never-show box=%d"),
	       bOpened ? 1 : 0, M_TurnedOffClosedCount, bOffersNeverShowAgain ? 1 : 0);
	if (not bOpened || M_TurnedOffClosedCount != 0 || bOffersNeverShowAgain)
	{
		Fail(TEXT("with unit intel off, the field manual must still open, without the never-show box"));
		return;
	}
	Takeover->OnClickedContinue();
	UE_LOG(LogTemp, Display, TEXT("NOMADIC_TAKEOVER turned off, after close: closed %d times, shown=%d"),
	       M_TurnedOffClosedCount, bM_TurnedOffWasShown ? 1 : 0);
	if (M_TurnedOffClosedCount != 1 || not bM_TurnedOffWasShown)
	{
		Fail(TEXT("with unit intel off, the field manual did not close once as shown"));
	}
}

void UNomadicConversionTakeoverValidateSubsystem::HandleMainClosed(bool bWasShown)
{
	++M_MainClosedCount;
	bM_MainWasShown = bWasShown;
	bM_MainWasPausedWhenClosed = UGameplayStatics::IsGamePaused(this);
	UE_LOG(LogTemp, Display, TEXT("NOMADIC_TAKEOVER On Closed fired: shown=%d paused=%d"), bWasShown ? 1 : 0,
	       bM_MainWasPausedWhenClosed ? 1 : 0);
}

void UNomadicConversionTakeoverValidateSubsystem::HandleTurnedOffClosed(bool bWasShown)
{
	++M_TurnedOffClosedCount;
	bM_TurnedOffWasShown = bWasShown;
}

void UNomadicConversionTakeoverValidateSubsystem::SetTakeoversEnabled(const bool bEnabled) const
{
	URTSSettingsMenuSubsystem* const SettingsSubsystem = GetWorld()->GetGameInstance()->GetSubsystem<
		URTSSettingsMenuSubsystem>();
	if (SettingsSubsystem != nullptr)
	{
		SettingsSubsystem->SetShowInfoTakeoversImmediately(bEnabled);
	}
}

UW_NomadicConversionTakeover* UNomadicConversionTakeoverValidateSubsystem::GetActiveNomadicConversionTakeover() const
{
	const UInfoTakeoverSubsystem* const InfoTakeovers = GetWorld()->GetSubsystem<UInfoTakeoverSubsystem>();
	return InfoTakeovers != nullptr
		       ? Cast<UW_NomadicConversionTakeover>(InfoTakeovers->GetActiveTakeover())
		       : nullptr;
}

void UNomadicConversionTakeoverValidateSubsystem::DismissDifficultyPicker() const
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
			Picker->WidgetTree->FindWidget(NomadicConversionTakeoverValidateConstants::DifficultyButtonName));
		if (IsValid(NormalButton))
		{
			NormalButton->OnClicked.Broadcast();
		}
	}
}

void UNomadicConversionTakeoverValidateSubsystem::Capture(const FString& Label) const
{
	const FIntPoint ViewportSize = GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport
		                               ? GEngine->GameViewport->Viewport->GetSizeXY()
		                               : FIntPoint::ZeroValue;
	const FString Folder = FString::Printf(TEXT("%dp"), ViewportSize.Y);
	const FString ScreenshotPath = FPaths::ProjectSavedDir() / TEXT("InfoTakeover") / TEXT("NomadicConversion") / Folder
		/ (Label + TEXT(".png"));
	FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
	UE_LOG(LogTemp, Display, TEXT("NOMADIC_TAKEOVER capture %s"), *ScreenshotPath);
}

void UNomadicConversionTakeoverValidateSubsystem::Fail(const FString& Reason)
{
	++M_Failures;
	UE_LOG(LogTemp, Error, TEXT("NOMADIC_TAKEOVER FAIL %s"), *Reason);
}

void UNomadicConversionTakeoverValidateSubsystem::Finish()
{
	SetTakeoversEnabled(bM_OriginalSettingValue);
	EnterPhase(ENomadicConversionTakeoverValidatePhase::Done);
	UE_LOG(LogTemp, Display, TEXT("NOMADIC_TAKEOVER done: %d failures -> %s"), M_Failures,
	       M_Failures == 0 ? TEXT("PASS") : TEXT("FAIL"));
	FPlatformMisc::RequestExit(false, TEXT("NomadicConversionTakeoverValidate"));
}
