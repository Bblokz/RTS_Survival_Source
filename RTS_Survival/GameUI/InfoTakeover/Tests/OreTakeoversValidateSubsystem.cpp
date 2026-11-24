// Copyright (C) Bas Blokzijl - All rights reserved.

#include "OreTakeoversValidateSubsystem.h"

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
#include "RTS_Survival/GameUI/InfoTakeover/W_CapturingTrucksTakeover.h"
#include "RTS_Survival/GameUI/InfoTakeover/W_DocumentTakeover.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/Subsystems/SettingsMenuSubsystem/RTSSettingsMenuSubsystem.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "UnrealClient.h"

namespace OreTakeoversValidateConstants
{
	const TCHAR* const CommandLineSwitch = TEXT("OreTakeoversValidate");
	constexpr double HudBuildSeconds = 5.0;
	constexpr double BeforeStartWaitSeconds = 1.5;
	constexpr double StartGameSeconds = 3.0;
	constexpr double OpenTimeoutSeconds = 10.0;
	constexpr double TypingShotSeconds = 0.4;
	// The diagram loop is 9.5 s; two extra shots catch the countdown and the surrendered tag.
	constexpr double DiagramCountdownShotSeconds = 3.0;
	constexpr double DiagramSurrenderedShotSeconds = 8.0;
	constexpr double DoneShotSeconds = 10.0;
	constexpr double MaxTypingWaitSeconds = 120.0;
	constexpr double PressUnderstoodAfterDoneShotSeconds = 1.0;
	constexpr double SettleAfterTypingSeconds = 0.5;
	constexpr double SettleAfterCloseSeconds = 0.5;
	constexpr double NextCheckAfterCloseSeconds = 1.0;
	// 12 s countdown over a 7 s walk shows 12..6; seeing at least this many values proves it ran while paused.
	constexpr int32 MinCountdownValuesSeen = 4;
	constexpr int32 ExpectedManifestLines = 6;
	const FName DifficultyButtonName(TEXT("M_NormalDifficultyButton"));
}

bool UOreTakeoversValidateSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	const UWorld* const OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), OreTakeoversValidateConstants::CommandLineSwitch);
#endif
}

void UOreTakeoversValidateSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	RTSFunctionLibrary::PushModalDialogSuppression();
	const URTSGameUserSettings* const GameUserSettings = URTSGameUserSettings::Get();
	bM_OriginalSettingValue = GameUserSettings == nullptr || GameUserSettings->GetShowInfoTakeovers();
	M_CapturingTrucksWatch.Name = TEXT("CapturingTrucks");
	M_ManifestWatch.Name = TEXT("OreManifest");
	EnterPhase(EOreTakeoversValidatePhase::WaitingForHud);
	UE_LOG(LogTemp, Display, TEXT("ORE_TAKEOVERS validate started (setting was %s)"),
	       bM_OriginalSettingValue ? TEXT("on") : TEXT("off"));
}

void UOreTakeoversValidateSubsystem::Tick(float DeltaTime)
{
	switch (M_Phase)
	{
	case EOreTakeoversValidatePhase::WaitingForHud:
		TickWaitingForHud();
		break;
	case EOreTakeoversValidatePhase::WaitingBeforeStart:
		TickWaitingBeforeStart();
		break;
	case EOreTakeoversValidatePhase::StartingGame:
		TickStartingGame();
		break;
	case EOreTakeoversValidatePhase::WatchingCapturingTrucks:
		TickWatchingCapturingTrucks();
		break;
	case EOreTakeoversValidatePhase::CheckingCapturingTrucksClosed:
		if (TickClosedStep(M_CapturingTrucksWatch))
		{
			// Documents are story content: they must open while unit intel is turned off.
			SetTakeoversEnabled(false);
			CallManifestNode();
			EnterPhase(EOreTakeoversValidatePhase::WatchingManifest);
		}
		break;
	case EOreTakeoversValidatePhase::WatchingManifest:
		TickWatchingManifest();
		break;
	case EOreTakeoversValidatePhase::CheckingManifestClosed:
		if (TickClosedStep(M_ManifestWatch))
		{
			Finish();
		}
		break;
	default:
		break;
	}
}

TStatId UOreTakeoversValidateSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UOreTakeoversValidateSubsystem, STATGROUP_Tickables);
}

void UOreTakeoversValidateSubsystem::EnterPhase(const EOreTakeoversValidatePhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartSeconds = FPlatformTime::Seconds();
}

double UOreTakeoversValidateSubsystem::GetSecondsInPhase() const
{
	return FPlatformTime::Seconds() - M_PhaseStartSeconds;
}

void UOreTakeoversValidateSubsystem::TickWaitingForHud()
{
	if (GetSecondsInPhase() < OreTakeoversValidateConstants::HudBuildSeconds)
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
		CallCapturingTrucksNode();
		EnterPhase(EOreTakeoversValidatePhase::WaitingBeforeStart);
		return;
	}
	UE_LOG(LogTemp, Display, TEXT("ORE_TAKEOVERS no start-game screen on this map; the before-start check is skipped"));
	EnterPhase(EOreTakeoversValidatePhase::StartingGame);
}

void UOreTakeoversValidateSubsystem::TickWaitingBeforeStart()
{
	if (GetSecondsInPhase() < OreTakeoversValidateConstants::BeforeStartWaitSeconds)
	{
		return;
	}
	const bool bOpenedBeforeStart = GetActiveTakeover() != nullptr;
	UE_LOG(LogTemp, Display, TEXT("ORE_TAKEOVERS before start: opened=%d (must be 0)"), bOpenedBeforeStart ? 1 : 0);
	if (bOpenedBeforeStart)
	{
		Fail(TEXT("the capturing trucks takeover opened over the start-game screen"));
	}
	if (M_StartGameControl.IsValid())
	{
		M_StartGameControl->PlayerStartedGame();
	}
	EnterPhase(EOreTakeoversValidatePhase::StartingGame);
}

void UOreTakeoversValidateSubsystem::TickStartingGame()
{
	DismissDifficultyPicker();
	if (GetSecondsInPhase() < OreTakeoversValidateConstants::StartGameSeconds)
	{
		return;
	}
	if (M_CapturingTrucksNode == nullptr)
	{
		CallCapturingTrucksNode();
	}
	EnterPhase(EOreTakeoversValidatePhase::WatchingCapturingTrucks);
}

void UOreTakeoversValidateSubsystem::CallCapturingTrucksNode()
{
	M_CapturingTrucksNode = UAsyncShowCapturingTrucksTakeover::ShowCapturingTrucksTakeover(M_PlayerController.Get());
	M_CapturingTrucksNode->OnClosed.AddDynamic(this, &UOreTakeoversValidateSubsystem::HandleCapturingTrucksClosed);
	M_CapturingTrucksNode->Activate();
	UE_LOG(LogTemp, Display, TEXT("ORE_TAKEOVERS capturing trucks node called"));
}

void UOreTakeoversValidateSubsystem::CallManifestNode()
{
	M_ManifestNode = UAsyncShowOreManifestTakeover::ShowOreManifestTakeover(M_PlayerController.Get());
	M_ManifestNode->OnClosed.AddDynamic(this, &UOreTakeoversValidateSubsystem::HandleManifestClosed);
	M_ManifestNode->Activate();
	UE_LOG(LogTemp, Display, TEXT("ORE_TAKEOVERS manifest node called with unit intel off; closed right after: %d"),
	       M_ManifestWatch.ClosedCount);
}

void UOreTakeoversValidateSubsystem::TickWatchingCapturingTrucks()
{
	using namespace OreTakeoversValidateConstants;
	UW_InfoTakeover* const Active = GetActiveTakeover();
	if (not M_CapturingTrucksWatch.bHasOpened && Active != nullptr)
	{
		CheckCapturingTrucksOpened(*Active);
	}
	if (M_CapturingTrucksWatch.bHasOpened)
	{
		SampleDiagram();
		const double SecondsOpen = FPlatformTime::Seconds() - M_CapturingTrucksWatch.OpenSeconds;
		// Two diagram stills between the typing shot and the finished shot (ShotsTaken 1 -> 2 is the done shot).
		static const TArray<TPair<double, FString>> DiagramShots = {
			{DiagramCountdownShotSeconds, TEXT("CapturingTrucks_diagram_countdown")},
			{DiagramSurrenderedShotSeconds, TEXT("CapturingTrucks_diagram_surrendered")}
		};
		for (int32 ShotIndex = 0; ShotIndex < DiagramShots.Num(); ++ShotIndex)
		{
			const int32 ShotBit = 1 << ShotIndex;
			if ((M_DiagramShotsTaken & ShotBit) == 0 && SecondsOpen >= DiagramShots[ShotIndex].Key)
			{
				M_DiagramShotsTaken |= ShotBit;
				Capture(DiagramShots[ShotIndex].Value);
			}
		}
	}
	else if (GetSecondsInPhase() > OpenTimeoutSeconds)
	{
		Fail(TEXT("the capturing trucks takeover never opened"));
		Finish();
		return;
	}
	if (TickWatchStep(M_CapturingTrucksWatch, Active))
	{
		UE_LOG(LogTemp, Display, TEXT("ORE_TAKEOVERS diagram: %d countdown values seen while paused, surrendered tag "
			       "seen=%d"), M_SeenCountdownSeconds.Num(), bM_SawSurrenderedTag ? 1 : 0);
		if (M_SeenCountdownSeconds.Num() < MinCountdownValuesSeen || not bM_SawSurrenderedTag)
		{
			Fail(TEXT("the capture diagram did not play while the game was paused"));
		}
		EnterPhase(EOreTakeoversValidatePhase::CheckingCapturingTrucksClosed);
	}
}

void UOreTakeoversValidateSubsystem::TickWatchingManifest()
{
	UW_InfoTakeover* const Active = GetActiveTakeover();
	if (not M_ManifestWatch.bHasOpened)
	{
		if (Active != nullptr)
		{
			CheckManifestOpened(*Active);
		}
		else if (GetSecondsInPhase() > OreTakeoversValidateConstants::OpenTimeoutSeconds)
		{
			Fail(TEXT("the ore manifest takeover never opened while unit intel was off"));
			Finish();
			return;
		}
	}
	if (TickWatchStep(M_ManifestWatch, Active))
	{
		EnterPhase(EOreTakeoversValidatePhase::CheckingManifestClosed);
	}
}

bool UOreTakeoversValidateSubsystem::TickWatchStep(FOreTakeoverWatch& Watch, UW_InfoTakeover* Active)
{
	using namespace OreTakeoversValidateConstants;
	if (not Watch.bHasOpened)
	{
		return false;
	}
	if (Active == nullptr || Active != Watch.Takeover.Get())
	{
		Fail(Watch.Name + TEXT(": the takeover closed before UNDERSTOOD was pressed"));
		Finish();
		return false;
	}
	const double SecondsOpen = FPlatformTime::Seconds() - Watch.OpenSeconds;
	if (Watch.ShotsTaken == 0 && SecondsOpen >= TypingShotSeconds)
	{
		++Watch.ShotsTaken;
		Capture(Watch.Name + TEXT("_0_typing"));
		return false;
	}
	if (Watch.ShotsTaken == 1)
	{
		// A slow (DebugGame, busy GPU) run types slower than the clock; the finished shot waits for the typewriter.
		const bool bTypingDone = Active->GetTypingPhase() == EInfoTakeoverTypingPhase::Done;
		if (bTypingDone && Watch.TypingDoneSeconds < 0.0)
		{
			Watch.TypingDoneSeconds = SecondsOpen;
		}
		const bool bLastLetterDrawn = bTypingDone && SecondsOpen >= Watch.TypingDoneSeconds + SettleAfterTypingSeconds;
		if (SecondsOpen < DoneShotSeconds || (not bLastLetterDrawn && SecondsOpen < MaxTypingWaitSeconds))
		{
			return false;
		}
		if (not bTypingDone)
		{
			Fail(Watch.Name + TEXT(": the title and description did not finish typing"));
		}
		++Watch.ShotsTaken;
		Watch.DoneShotSeconds = SecondsOpen;
		Capture(Watch.Name + TEXT("_1_done"));
		return false;
	}
	if (Watch.ShotsTaken < 2 || SecondsOpen < Watch.DoneShotSeconds + PressUnderstoodAfterDoneShotSeconds)
	{
		return false;
	}
	if (Watch.ClosedCount != 0)
	{
		Fail(Watch.Name + TEXT(": On Closed fired before UNDERSTOOD"));
	}
	Active->PressConfirmButton();
	return true;
}

bool UOreTakeoversValidateSubsystem::TickClosedStep(FOreTakeoverWatch& Watch)
{
	using namespace OreTakeoversValidateConstants;
	if (GetSecondsInPhase() < SettleAfterCloseSeconds)
	{
		return false;
	}
	if (not Watch.bHasCapturedAfterClose)
	{
		Watch.bHasCapturedAfterClose = true;
		const bool bPausedNow = UGameplayStatics::IsGamePaused(this);
		UE_LOG(LogTemp, Display, TEXT("ORE_TAKEOVERS %s closed %d times, shown=%d, paused when closed=%d, paused now=%d"),
		       *Watch.Name, Watch.ClosedCount, Watch.bWasShown ? 1 : 0, Watch.bWasPausedWhenClosed ? 1 : 0,
		       bPausedNow ? 1 : 0);
		if (Watch.ClosedCount != 1 || not Watch.bWasShown)
		{
			Fail(Watch.Name + TEXT(": On Closed did not fire exactly once with Was Shown after UNDERSTOOD"));
		}
		if (Watch.bWasPausedWhenClosed || bPausedNow)
		{
			Fail(Watch.Name + TEXT(": the game was still paused when On Closed fired"));
		}
		Capture(Watch.Name + TEXT("_2_after_close"));
		return false;
	}
	return GetSecondsInPhase() >= NextCheckAfterCloseSeconds;
}

void UOreTakeoversValidateSubsystem::CheckCapturingTrucksOpened(UW_InfoTakeover& Takeover)
{
	M_CapturingTrucksWatch.bHasOpened = true;
	M_CapturingTrucksWatch.Takeover = &Takeover;
	M_CapturingTrucksWatch.OpenSeconds = FPlatformTime::Seconds();
	const UW_CapturingTrucksTakeover* const CapturingTrucks = Cast<UW_CapturingTrucksTakeover>(&Takeover);
	const bool bHasDiagram = CapturingTrucks != nullptr && CapturingTrucks->GetHasDiagram();
	const bool bPaused = UGameplayStatics::IsGamePaused(this);
	UE_LOG(LogTemp, Display, TEXT("ORE_TAKEOVERS opened %s, diagram=%d, paused=%d, never-show box=%d"),
	       *Takeover.GetClass()->GetName(), bHasDiagram ? 1 : 0, bPaused ? 1 : 0,
	       Takeover.GetOffersNeverShowAgain() ? 1 : 0);
	if (CapturingTrucks == nullptr)
	{
		Fail(TEXT("the open takeover is not the capturing trucks takeover"));
		return;
	}
	if (Takeover.GetClass() == UW_CapturingTrucksTakeover::StaticClass())
	{
		Fail(TEXT("WBP_CapturingTrucksTakeover did not load; the unstyled C++ widget opened"));
	}
	if (not bHasDiagram)
	{
		Fail(TEXT("the capture diagram (countdown plate, squad icon, surrendered tag) was not built"));
	}
	if (not bPaused || Takeover.GetOffersNeverShowAgain())
	{
		Fail(TEXT("the field manual must pause the game and offer no never-show box"));
	}
}

void UOreTakeoversValidateSubsystem::CheckManifestOpened(UW_InfoTakeover& Takeover)
{
	using namespace OreTakeoversValidateConstants;
	M_ManifestWatch.bHasOpened = true;
	M_ManifestWatch.Takeover = &Takeover;
	M_ManifestWatch.OpenSeconds = FPlatformTime::Seconds();
	const UW_DocumentTakeover* const Document = Cast<UW_DocumentTakeover>(&Takeover);
	const int32 LineCount = Document != nullptr ? Document->GetPaperLineCount() : 0;
	const bool bHasStamp = Document != nullptr && Document->GetHasStamp();
	const bool bPaused = UGameplayStatics::IsGamePaused(this);
	UE_LOG(LogTemp, Display, TEXT("ORE_TAKEOVERS opened %s, lines=%d, stamp=%d, paused=%d, never-show box=%d"),
	       *Takeover.GetClass()->GetName(), LineCount, bHasStamp ? 1 : 0, bPaused ? 1 : 0,
	       Takeover.GetOffersNeverShowAgain() ? 1 : 0);
	if (Document == nullptr)
	{
		Fail(TEXT("the open takeover is not the document takeover"));
		return;
	}
	if (Takeover.GetClass() == UW_DocumentTakeover::StaticClass())
	{
		Fail(TEXT("WBP_DocumentTakeover did not load; the unstyled C++ widget opened"));
	}
	if (LineCount != ExpectedManifestLines || not bHasStamp)
	{
		Fail(TEXT("the manifest sheet does not show its six columns and the stamp"));
	}
	if (not bPaused || Takeover.GetOffersNeverShowAgain())
	{
		Fail(TEXT("the document must pause the game and offer no never-show box"));
	}
}

void UOreTakeoversValidateSubsystem::SampleDiagram()
{
	const UW_CapturingTrucksTakeover* const CapturingTrucks = Cast<UW_CapturingTrucksTakeover>(
		M_CapturingTrucksWatch.Takeover.Get());
	if (CapturingTrucks == nullptr || not UGameplayStatics::IsGamePaused(this))
	{
		return;
	}
	const int32 ShownSeconds = CapturingTrucks->GetShownCountdownSeconds();
	if (ShownSeconds == INDEX_NONE)
	{
		bM_SawSurrenderedTag = true;
		return;
	}
	M_SeenCountdownSeconds.Add(ShownSeconds);
}

void UOreTakeoversValidateSubsystem::HandleCapturingTrucksClosed(bool bWasShown)
{
	RecordClosed(M_CapturingTrucksWatch, bWasShown);
}

void UOreTakeoversValidateSubsystem::HandleManifestClosed(bool bWasShown)
{
	RecordClosed(M_ManifestWatch, bWasShown);
}

void UOreTakeoversValidateSubsystem::RecordClosed(FOreTakeoverWatch& Watch, const bool bWasShown)
{
	++Watch.ClosedCount;
	Watch.bWasShown = bWasShown;
	Watch.bWasPausedWhenClosed = UGameplayStatics::IsGamePaused(this);
	UE_LOG(LogTemp, Display, TEXT("ORE_TAKEOVERS %s On Closed fired: shown=%d paused=%d"), *Watch.Name,
	       bWasShown ? 1 : 0, Watch.bWasPausedWhenClosed ? 1 : 0);
}

void UOreTakeoversValidateSubsystem::SetTakeoversEnabled(const bool bEnabled) const
{
	URTSSettingsMenuSubsystem* const SettingsSubsystem = GetWorld()->GetGameInstance()->GetSubsystem<
		URTSSettingsMenuSubsystem>();
	if (SettingsSubsystem != nullptr)
	{
		SettingsSubsystem->SetShowInfoTakeoversImmediately(bEnabled);
	}
}

UW_InfoTakeover* UOreTakeoversValidateSubsystem::GetActiveTakeover() const
{
	const UInfoTakeoverSubsystem* const InfoTakeovers = GetWorld()->GetSubsystem<UInfoTakeoverSubsystem>();
	return InfoTakeovers != nullptr ? InfoTakeovers->GetActiveTakeover() : nullptr;
}

void UOreTakeoversValidateSubsystem::DismissDifficultyPicker() const
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
			Picker->WidgetTree->FindWidget(OreTakeoversValidateConstants::DifficultyButtonName));
		if (IsValid(NormalButton))
		{
			NormalButton->OnClicked.Broadcast();
		}
	}
}

void UOreTakeoversValidateSubsystem::Capture(const FString& Label) const
{
	const FIntPoint ViewportSize = GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport
		                               ? GEngine->GameViewport->Viewport->GetSizeXY()
		                               : FIntPoint::ZeroValue;
	const FString Folder = FString::Printf(TEXT("%dp"), ViewportSize.Y);
	const FString ScreenshotPath = FPaths::ProjectSavedDir() / TEXT("InfoTakeover") / TEXT("OreTakeovers") / Folder
		/ (Label + TEXT(".png"));
	FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
	UE_LOG(LogTemp, Display, TEXT("ORE_TAKEOVERS capture %s"), *ScreenshotPath);
}

void UOreTakeoversValidateSubsystem::Fail(const FString& Reason)
{
	++M_Failures;
	UE_LOG(LogTemp, Error, TEXT("ORE_TAKEOVERS FAIL %s"), *Reason);
}

void UOreTakeoversValidateSubsystem::Finish()
{
	if (M_Phase == EOreTakeoversValidatePhase::Done)
	{
		return;
	}
	SetTakeoversEnabled(bM_OriginalSettingValue);
	EnterPhase(EOreTakeoversValidatePhase::Done);
	UE_LOG(LogTemp, Display, TEXT("ORE_TAKEOVERS done: %d failures -> %s"), M_Failures,
	       M_Failures == 0 ? TEXT("PASS") : TEXT("FAIL"));
	FPlatformMisc::RequestExit(false, TEXT("OreTakeoversValidate"));
}
