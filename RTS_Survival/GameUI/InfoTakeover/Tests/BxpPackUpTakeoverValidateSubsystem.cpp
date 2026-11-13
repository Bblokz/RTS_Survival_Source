// Copyright (C) Bas Blokzijl - All rights reserved.

#include "BxpPackUpTakeoverValidateSubsystem.h"

#include "Animation/WidgetAnimation.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
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
#include "RTS_Survival/GameUI/InfoTakeover/W_BxpPackUpTakeover.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/Subsystems/SettingsMenuSubsystem/RTSSettingsMenuSubsystem.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "UnrealClient.h"

namespace BxpPackUpTakeoverValidateConstants
{
	const TCHAR* const CommandLineSwitch = TEXT("BxpPackUpTakeoverValidate");
	constexpr double HudBuildSeconds = 5.0;
	constexpr double BeforeStartWaitSeconds = 1.5;
	constexpr double StartGameSeconds = 3.0;
	constexpr double OpenTimeoutSeconds = 10.0;
	// Real seconds after the takeover opened.
	constexpr double TypingShotSeconds = 0.4;
	// The illustration loops in 12 s (PackLoop); eight frames 1.5 s apart cover the pack-up, the drive, the
	// automatic rebuild and the manual placing.
	constexpr double FirstFrameShotSeconds = 1.0;
	constexpr double FrameShotIntervalSeconds = 1.5;
	constexpr int32 FrameShotCount = 8;
	// Then the loop is jumped to each beat (pack-up, truck, deploy, automatic rebuild, manual placing) for a still,
	// because a slow DebugGame run plays it slower than the clock and the frames above may not reach the later beats.
	const TArray<float> BeatShotAnimationSeconds = {0.4f, 2.0f, 3.0f, 4.6f, 5.6f, 7.8f, 9.3f, 10.6f};
	constexpr double BeatShotIntervalSeconds = 0.4;
	constexpr int64 FramesBetweenBeatJumpAndShot = 3;
	constexpr int32 ExpectedSlotLegendPictures = 3;
	// The finished shot waits this long at most for the typewriter.
	constexpr double MaxTypingWaitSeconds = 120.0;
	constexpr double SettleAfterTypingSeconds = 0.5;
	constexpr double PressUnderstoodAfterDoneShotSeconds = 1.0;
	constexpr double SettleAfterCloseSeconds = 0.5;
	constexpr double NextCheckAfterCloseSeconds = 1.0;
	constexpr float MinSignatureChange = 0.01f;
	const FName DifficultyButtonName(TEXT("M_NormalDifficultyButton"));
}

bool UBxpPackUpTakeoverValidateSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	const UWorld* const OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), BxpPackUpTakeoverValidateConstants::CommandLineSwitch);
#endif
}

void UBxpPackUpTakeoverValidateSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	RTSFunctionLibrary::PushModalDialogSuppression();
	const URTSGameUserSettings* const GameUserSettings = URTSGameUserSettings::Get();
	bM_OriginalSettingValue = GameUserSettings == nullptr || GameUserSettings->GetShowInfoTakeovers();
	EnterPhase(EBxpPackUpTakeoverValidatePhase::WaitingForHud);
	UE_LOG(LogTemp, Display, TEXT("BXP_PACKUP_TAKEOVER validate started (setting was %s)"),
	       bM_OriginalSettingValue ? TEXT("on") : TEXT("off"));
}

void UBxpPackUpTakeoverValidateSubsystem::Tick(float DeltaTime)
{
	switch (M_Phase)
	{
	case EBxpPackUpTakeoverValidatePhase::WaitingForHud:
		TickWaitingForHud();
		break;
	case EBxpPackUpTakeoverValidatePhase::WaitingBeforeStart:
		TickWaitingBeforeStart();
		break;
	case EBxpPackUpTakeoverValidatePhase::StartingGame:
		TickStartingGame();
		break;
	case EBxpPackUpTakeoverValidatePhase::WatchingTakeover:
		TickWatchingTakeover();
		break;
	case EBxpPackUpTakeoverValidatePhase::CheckingClosed:
		TickCheckingClosed();
		break;
	default:
		break;
	}
}

TStatId UBxpPackUpTakeoverValidateSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBxpPackUpTakeoverValidateSubsystem, STATGROUP_Tickables);
}

void UBxpPackUpTakeoverValidateSubsystem::EnterPhase(const EBxpPackUpTakeoverValidatePhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartSeconds = FPlatformTime::Seconds();
}

double UBxpPackUpTakeoverValidateSubsystem::GetSecondsInPhase() const
{
	return FPlatformTime::Seconds() - M_PhaseStartSeconds;
}

void UBxpPackUpTakeoverValidateSubsystem::TickWaitingForHud()
{
	if (GetSecondsInPhase() < BxpPackUpTakeoverValidateConstants::HudBuildSeconds)
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
		EnterPhase(EBxpPackUpTakeoverValidatePhase::WaitingBeforeStart);
		return;
	}
	UE_LOG(LogTemp, Display, TEXT("BXP_PACKUP_TAKEOVER no start-game screen on this map; the before-start check is skipped"));
	EnterPhase(EBxpPackUpTakeoverValidatePhase::StartingGame);
}

void UBxpPackUpTakeoverValidateSubsystem::TickWaitingBeforeStart()
{
	if (GetSecondsInPhase() < BxpPackUpTakeoverValidateConstants::BeforeStartWaitSeconds)
	{
		return;
	}
	const bool bOpenedBeforeStart = GetActiveBxpPackUpTakeover() != nullptr;
	UE_LOG(LogTemp, Display, TEXT("BXP_PACKUP_TAKEOVER before start: opened=%d (must be 0)"), bOpenedBeforeStart ? 1 : 0);
	if (bOpenedBeforeStart)
	{
		Fail(TEXT("the takeover opened over the start-game screen"));
	}
	if (M_StartGameControl.IsValid())
	{
		M_StartGameControl->PlayerStartedGame();
	}
	EnterPhase(EBxpPackUpTakeoverValidatePhase::StartingGame);
}

void UBxpPackUpTakeoverValidateSubsystem::TickStartingGame()
{
	DismissDifficultyPicker();
	if (GetSecondsInPhase() < BxpPackUpTakeoverValidateConstants::StartGameSeconds)
	{
		return;
	}
	if (M_MainNode == nullptr)
	{
		CallMainNode();
	}
	EnterPhase(EBxpPackUpTakeoverValidatePhase::WatchingTakeover);
}

void UBxpPackUpTakeoverValidateSubsystem::CallMainNode()
{
	M_MainNode = UAsyncShowBxpPackUpTakeover::ShowBxpPackUpTakeover(M_PlayerController.Get());
	M_MainNode->OnClosed.AddDynamic(this, &UBxpPackUpTakeoverValidateSubsystem::HandleMainClosed);
	M_MainNode->Activate();
	UE_LOG(LogTemp, Display, TEXT("BXP_PACKUP_TAKEOVER node called; closed count right after: %d"), M_MainClosedCount);
}

void UBxpPackUpTakeoverValidateSubsystem::TickWatchingTakeover()
{
	using namespace BxpPackUpTakeoverValidateConstants;
	UW_BxpPackUpTakeover* const Takeover = GetActiveBxpPackUpTakeover();
	if (not bM_HasOpened)
	{
		if (Takeover != nullptr)
		{
			StartWatching(*Takeover);
			return;
		}
		if (GetSecondsInPhase() > OpenTimeoutSeconds)
		{
			Fail(TEXT("the building expansion pack-up takeover never opened"));
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
	// The footer button as the player clicks it.
	Takeover->PressConfirmButton();
	EnterPhase(EBxpPackUpTakeoverValidatePhase::CheckingClosed);
}

void UBxpPackUpTakeoverValidateSubsystem::StartWatching(UW_BxpPackUpTakeover& Takeover)
{
	bM_HasOpened = true;
	M_WatchedTakeover = &Takeover;
	M_OpenSeconds = FPlatformTime::Seconds();
	const bool bHasIllustration = IsValid(Takeover.GetIllustration());
	const bool bPaused = UGameplayStatics::IsGamePaused(this);
	UE_LOG(LogTemp, Display, TEXT("BXP_PACKUP_TAKEOVER opened %s, illustration=%s, paused=%d"),
	       *Takeover.GetClass()->GetName(),
	       bHasIllustration ? *Takeover.GetIllustration()->GetClass()->GetName() : TEXT("none"), bPaused ? 1 : 0);
	if (Takeover.GetClass() == UW_BxpPackUpTakeover::StaticClass())
	{
		Fail(TEXT("WBP_BxpPackUpTakeover did not load; the unstyled C++ widget opened"));
	}
	if (not bHasIllustration)
	{
		Fail(TEXT("the looping illustration WBP_BxpPack_Loop did not load"));
	}
	UE_LOG(LogTemp, Display, TEXT("BXP_PACKUP_TAKEOVER slot legend pictures loaded: %d of %d"),
	       Takeover.GetSlotLegendPictureCount(), BxpPackUpTakeoverValidateConstants::ExpectedSlotLegendPictures);
	if (Takeover.GetSlotLegendPictureCount() != BxpPackUpTakeoverValidateConstants::ExpectedSlotLegendPictures)
	{
		Fail(TEXT("the slot legend pictures did not all load"));
	}
	if (not bPaused)
	{
		Fail(TEXT("the game is not paused while the takeover is open"));
	}
}

bool UBxpPackUpTakeoverValidateSubsystem::TickFrameShots(const UW_BxpPackUpTakeover& Takeover,
                                                                 const double SecondsOpen)
{
	using namespace BxpPackUpTakeoverValidateConstants;
	if (not bM_HasTypingShot)
	{
		if (SecondsOpen >= TypingShotSeconds)
		{
			bM_HasTypingShot = true;
			Capture(TEXT("BxpPackUp_0_typing"));
		}
		return false;
	}
	if (M_NextFrameShot < FrameShotCount)
	{
		if (SecondsOpen >= FirstFrameShotSeconds + FrameShotIntervalSeconds * M_NextFrameShot)
		{
			CaptureAnimationFrame(Takeover);
		}
		return false;
	}
	if (M_NextBeatShot >= BeatShotAnimationSeconds.Num())
	{
		return true;
	}
	const float BeatSeconds = BeatShotAnimationSeconds[M_NextBeatShot];
	if (M_BeatJumpFrame == INDEX_NONE)
	{
		if (SecondsOpen >= M_LastShotSeconds + BeatShotIntervalSeconds)
		{
			JumpToBeat(Takeover, BeatSeconds);
			M_BeatJumpFrame = static_cast<int64>(GFrameCounter);
		}
		return false;
	}
	// A few frames after the jump, so the widgets have evaluated the new time before the shot is taken.
	if (static_cast<int64>(GFrameCounter) < M_BeatJumpFrame + FramesBetweenBeatJumpAndShot)
	{
		return false;
	}
	M_BeatJumpFrame = INDEX_NONE;
	M_LastShotSeconds = SecondsOpen;
	Capture(FString::Printf(TEXT("BxpPackUp_beat_%04.1fs"), BeatSeconds));
	++M_NextBeatShot;
	return false;
}

void UBxpPackUpTakeoverValidateSubsystem::JumpToBeat(const UW_BxpPackUpTakeover& Takeover,
                                                     const float AnimationSeconds)
{
	UUserWidget* const Illustration = Takeover.GetIllustration();
	const UWidgetBlueprintGeneratedClass* const GeneratedClass = IsValid(Illustration)
		                                                             ? Cast<UWidgetBlueprintGeneratedClass>(
			                                                             Illustration->GetClass())
		                                                             : nullptr;
	if (GeneratedClass == nullptr)
	{
		return;
	}
	for (UWidgetAnimation* const Animation : GeneratedClass->Animations)
	{
		if (IsValid(Animation))
		{
			// Restarts the endless loop at that time; the illustration keeps looping from there.
			Illustration->PlayAnimation(Animation, AnimationSeconds, 0);
		}
	}
}

bool UBxpPackUpTakeoverValidateSubsystem::TickDoneShot(const UW_BxpPackUpTakeover& Takeover,
                                                               const double SecondsOpen)
{
	using namespace BxpPackUpTakeoverValidateConstants;
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
	Capture(TEXT("BxpPackUp_1_done"));
	return false;
}

void UBxpPackUpTakeoverValidateSubsystem::CaptureAnimationFrame(const UW_BxpPackUpTakeover& Takeover)
{
	const bool bPaused = UGameplayStatics::IsGamePaused(this);
	if (not bPaused)
	{
		Fail(TEXT("the game is not paused while the takeover is open"));
	}
	const UUserWidget* const Illustration = Takeover.GetIllustration();
	const float Signature = IsValid(Illustration) ? GetIllustrationSignature(*Illustration) : 0.f;
	M_IllustrationSignatures.Add(Signature);
	UE_LOG(LogTemp, Display, TEXT("BXP_PACKUP_TAKEOVER frame %d paused=%d signature=%.3f"), M_NextFrameShot + 1,
	       bPaused ? 1 : 0, Signature);
	Capture(FString::Printf(TEXT("BxpPackUp_frame_%d"), M_NextFrameShot + 1));
	++M_NextFrameShot;
	M_LastShotSeconds = FPlatformTime::Seconds() - M_OpenSeconds;
}

float UBxpPackUpTakeoverValidateSubsystem::GetIllustrationSignature(const UUserWidget& Illustration)
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

void UBxpPackUpTakeoverValidateSubsystem::CheckIllustrationAnimated()
{
	using namespace BxpPackUpTakeoverValidateConstants;
	float MinSignature = TNumericLimits<float>::Max();
	float MaxSignature = TNumericLimits<float>::Lowest();
	for (const float Signature : M_IllustrationSignatures)
	{
		MinSignature = FMath::Min(MinSignature, Signature);
		MaxSignature = FMath::Max(MaxSignature, Signature);
	}
	const bool bAnimated = M_IllustrationSignatures.Num() > 1 && MaxSignature - MinSignature > MinSignatureChange;
	UE_LOG(LogTemp, Display, TEXT("BXP_PACKUP_TAKEOVER illustration animated while paused: %s (range %.3f over %d frames)"),
	       bAnimated ? TEXT("yes") : TEXT("NO"), bAnimated ? MaxSignature - MinSignature : 0.f,
	       M_IllustrationSignatures.Num());
	if (not bAnimated)
	{
		Fail(TEXT("the illustration did not animate while paused"));
	}
}

void UBxpPackUpTakeoverValidateSubsystem::TickCheckingClosed()
{
	using namespace BxpPackUpTakeoverValidateConstants;
	if (GetSecondsInPhase() < SettleAfterCloseSeconds)
	{
		return;
	}
	if (not bM_HasCapturedAfterClose)
	{
		bM_HasCapturedAfterClose = true;
		UE_LOG(LogTemp, Display, TEXT("BXP_PACKUP_TAKEOVER main node: closed %d times, shown=%d, paused when closed=%d, "
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
		Capture(TEXT("BxpPackUp_2_after_close"));
		return;
	}
	if (GetSecondsInPhase() < NextCheckAfterCloseSeconds)
	{
		return;
	}
	CheckTurnedOffNode();
	Finish();
}

void UBxpPackUpTakeoverValidateSubsystem::CheckTurnedOffNode()
{
	SetTakeoversEnabled(false);
	M_TurnedOffNode = UAsyncShowBxpPackUpTakeover::ShowBxpPackUpTakeover(M_PlayerController.Get());
	M_TurnedOffNode->OnClosed.AddDynamic(this, &UBxpPackUpTakeoverValidateSubsystem::HandleTurnedOffClosed);
	M_TurnedOffNode->Activate();
	const bool bOpened = GetActiveBxpPackUpTakeover() != nullptr;
	UE_LOG(LogTemp, Display, TEXT("BXP_PACKUP_TAKEOVER turned off: closed %d times at once, shown=%d, opened=%d"),
	       M_TurnedOffClosedCount, bM_TurnedOffWasShown ? 1 : 0, bOpened ? 1 : 0);
	if (M_TurnedOffClosedCount != 1 || bM_TurnedOffWasShown || bOpened)
	{
		Fail(TEXT("with the takeovers off, On Closed must fire at once with Was Shown false"));
	}
}

void UBxpPackUpTakeoverValidateSubsystem::HandleMainClosed(bool bWasShown)
{
	++M_MainClosedCount;
	bM_MainWasShown = bWasShown;
	bM_MainWasPausedWhenClosed = UGameplayStatics::IsGamePaused(this);
	UE_LOG(LogTemp, Display, TEXT("BXP_PACKUP_TAKEOVER On Closed fired: shown=%d paused=%d"), bWasShown ? 1 : 0,
	       bM_MainWasPausedWhenClosed ? 1 : 0);
}

void UBxpPackUpTakeoverValidateSubsystem::HandleTurnedOffClosed(bool bWasShown)
{
	++M_TurnedOffClosedCount;
	bM_TurnedOffWasShown = bWasShown;
}

void UBxpPackUpTakeoverValidateSubsystem::SetTakeoversEnabled(const bool bEnabled) const
{
	URTSSettingsMenuSubsystem* const SettingsSubsystem = GetWorld()->GetGameInstance()->GetSubsystem<
		URTSSettingsMenuSubsystem>();
	if (SettingsSubsystem != nullptr)
	{
		SettingsSubsystem->SetShowInfoTakeoversImmediately(bEnabled);
	}
}

UW_BxpPackUpTakeover* UBxpPackUpTakeoverValidateSubsystem::GetActiveBxpPackUpTakeover() const
{
	const UInfoTakeoverSubsystem* const InfoTakeovers = GetWorld()->GetSubsystem<UInfoTakeoverSubsystem>();
	return InfoTakeovers != nullptr
		       ? Cast<UW_BxpPackUpTakeover>(InfoTakeovers->GetActiveTakeover())
		       : nullptr;
}

void UBxpPackUpTakeoverValidateSubsystem::DismissDifficultyPicker() const
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
			Picker->WidgetTree->FindWidget(BxpPackUpTakeoverValidateConstants::DifficultyButtonName));
		if (IsValid(NormalButton))
		{
			NormalButton->OnClicked.Broadcast();
		}
	}
}

void UBxpPackUpTakeoverValidateSubsystem::Capture(const FString& Label) const
{
	const FIntPoint ViewportSize = GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport
		                               ? GEngine->GameViewport->Viewport->GetSizeXY()
		                               : FIntPoint::ZeroValue;
	const FString Folder = FString::Printf(TEXT("%dp"), ViewportSize.Y);
	const FString ScreenshotPath = FPaths::ProjectSavedDir() / TEXT("InfoTakeover") / TEXT("BxpPackUp") / Folder
		/ (Label + TEXT(".png"));
	FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
	UE_LOG(LogTemp, Display, TEXT("BXP_PACKUP_TAKEOVER capture %s"), *ScreenshotPath);
}

void UBxpPackUpTakeoverValidateSubsystem::Fail(const FString& Reason)
{
	++M_Failures;
	UE_LOG(LogTemp, Error, TEXT("BXP_PACKUP_TAKEOVER FAIL %s"), *Reason);
}

void UBxpPackUpTakeoverValidateSubsystem::Finish()
{
	SetTakeoversEnabled(bM_OriginalSettingValue);
	EnterPhase(EBxpPackUpTakeoverValidatePhase::Done);
	UE_LOG(LogTemp, Display, TEXT("BXP_PACKUP_TAKEOVER done: %d failures -> %s"), M_Failures,
	       M_Failures == 0 ? TEXT("PASS") : TEXT("FAIL"));
	FPlatformMisc::RequestExit(false, TEXT("BxpPackUpTakeoverValidate"));
}
