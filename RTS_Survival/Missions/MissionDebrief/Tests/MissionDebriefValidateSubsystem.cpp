// Copyright (C) Bas Blokzijl - All rights reserved.

#include "MissionDebriefValidateSubsystem.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/Button.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "RTS_Survival/GameUI/GameDifficultyPicker/W_GameDifficultyPicker.h"
#include "RTS_Survival/Missions/MissionDebrief/MissionDebriefStage.h"
#include "RTS_Survival/Missions/MissionDebrief/W_MissionEnd.h"
#include "RTS_Survival/Missions/MissionManager/MissionManager.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "UnrealClient.h"

namespace MissionDebriefValidateConstants
{
	const TCHAR* const CommandLineSwitch = TEXT("MissionDebriefValidate");
	const TCHAR* const DefeatSwitch = TEXT("MissionDebriefDefeat");
	constexpr double HudBuildSeconds = 5.0;
	constexpr double StartGameSeconds = 3.0;
	// Real seconds after the mission ended.
	constexpr double VictoryShots[] = {0.3, 1.2, 1.95, 2.6, 3.5, 4.2, 5.2, 6.4};
	constexpr double DefeatShots[] = {1.95, 3.5, 4.2, 5.2, 6.0, 7.0, 8.4, 10.0};
	constexpr double StageCheckSeconds = 1.0;
	constexpr double ButtonsSlackSeconds = 1.2;
	constexpr float VictoryButtonsDue = 4.35f;
	constexpr float DefeatButtonsDue = 7.35f;
	constexpr double FinishAfterLastShotSeconds = 1.0;
	const FName DifficultyButtonName(TEXT("M_NormalDifficultyButton"));
}

bool UMissionDebriefValidateSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	const UWorld* const OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), MissionDebriefValidateConstants::CommandLineSwitch);
#endif
}

void UMissionDebriefValidateSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	RTSFunctionLibrary::PushModalDialogSuppression();
	M_Outcome = FParse::Param(FCommandLine::Get(), MissionDebriefValidateConstants::DefeatSwitch)
		            ? EMissionDebriefOutcome::Defeat
		            : EMissionDebriefOutcome::Victory;
	EnterPhase(EMissionDebriefValidatePhase::WaitingForHud);
	UE_LOG(LogTemp, Display, TEXT("MISSION_DEBRIEF validate started (%s)"),
	       M_Outcome == EMissionDebriefOutcome::Victory ? TEXT("victory") : TEXT("defeat"));
}

void UMissionDebriefValidateSubsystem::Tick(float DeltaTime)
{
	switch (M_Phase)
	{
	case EMissionDebriefValidatePhase::WaitingForHud:
		TickWaitingForHud();
		break;
	case EMissionDebriefValidatePhase::StartingGame:
		TickStartingGame();
		break;
	case EMissionDebriefValidatePhase::Watching:
		TickWatching();
		break;
	default:
		break;
	}
}

TStatId UMissionDebriefValidateSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMissionDebriefValidateSubsystem, STATGROUP_Tickables);
}

void UMissionDebriefValidateSubsystem::EnterPhase(const EMissionDebriefValidatePhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartSeconds = FPlatformTime::Seconds();
}

double UMissionDebriefValidateSubsystem::GetSecondsInPhase() const
{
	return FPlatformTime::Seconds() - M_PhaseStartSeconds;
}

void UMissionDebriefValidateSubsystem::TickWaitingForHud()
{
	if (GetSecondsInPhase() < MissionDebriefValidateConstants::HudBuildSeconds)
	{
		return;
	}
	ACPPController* const Controller = Cast<ACPPController>(GetWorld()->GetFirstPlayerController());
	if (not IsValid(Controller))
	{
		return;
	}
	UPlayerStartGameControl* const StartGameControl = Controller->FindComponentByClass<UPlayerStartGameControl>();
	if (IsValid(StartGameControl) && StartGameControl->GetIsWaitingForPlayerToStart())
	{
		StartGameControl->PlayerStartedGame();
	}
	EnterPhase(EMissionDebriefValidatePhase::StartingGame);
}

void UMissionDebriefValidateSubsystem::TickStartingGame()
{
	DismissDifficultyPicker();
	if (GetSecondsInPhase() < MissionDebriefValidateConstants::StartGameSeconds)
	{
		return;
	}
	TriggerMissionEnd();
}

void UMissionDebriefValidateSubsystem::TriggerMissionEnd()
{
	for (TActorIterator<AMissionManager> It(GetWorld()); It; ++It)
	{
		M_MissionManager = *It;
		break;
	}
	if (not M_MissionManager.IsValid())
	{
		Fail(TEXT("no mission manager on this map"));
		Finish();
		return;
	}
	if (M_Outcome == EMissionDebriefOutcome::Victory)
	{
		M_MissionManager->TriggerVictory(true);
	}
	else
	{
		M_MissionManager->TriggerDefeat(ERTSDefeatType::LostHQ);
	}
	UE_LOG(LogTemp, Display, TEXT("MISSION_DEBRIEF mission ended; paused=%d"), GetWorld()->IsPaused() ? 1 : 0);
	EnterPhase(EMissionDebriefValidatePhase::Watching);
}

void UMissionDebriefValidateSubsystem::TickWatching()
{
	using namespace MissionDebriefValidateConstants;
	const bool bVictory = M_Outcome == EMissionDebriefOutcome::Victory;
	const double Seconds = GetSecondsInPhase();
	if (not bM_CheckedStage && Seconds >= StageCheckSeconds)
	{
		CheckStage();
	}
	const float ButtonsDue = bVictory ? VictoryButtonsDue : DefeatButtonsDue;
	if (not bM_CheckedButtons && Seconds >= ButtonsDue + ButtonsSlackSeconds)
	{
		CheckButtons();
	}
	const double* const Shots = bVictory ? VictoryShots : DefeatShots;
	constexpr int32 ShotCount = UE_ARRAY_COUNT(VictoryShots);
	if (M_NextShotIndex < ShotCount && Seconds >= Shots[M_NextShotIndex])
	{
		Capture(FString::Printf(TEXT("%02d_t%04.1f"), M_NextShotIndex, Shots[M_NextShotIndex]));
		++M_NextShotIndex;
		return;
	}
	if (M_NextShotIndex >= ShotCount && Seconds >= Shots[ShotCount - 1] + FinishAfterLastShotSeconds)
	{
		Finish();
	}
}

void UMissionDebriefValidateSubsystem::CheckStage()
{
	bM_CheckedStage = true;
	AMissionDebriefStage* const Stage = M_MissionManager.IsValid() ? M_MissionManager->GetDebriefStage() : nullptr;
	const APlayerController* const Controller = GetWorld()->GetFirstPlayerController();
	const AActor* const ViewTarget = IsValid(Controller) ? Controller->GetViewTarget() : nullptr;
	const bool bHasStage = IsValid(Stage) && Stage->GetIsRunning();
	const bool bViewsStage = bHasStage && ViewTarget == Stage;
	UE_LOG(LogTemp, Display, TEXT("MISSION_DEBRIEF stage running=%d view_target_is_stage=%d elapsed=%.2f paused=%d"),
	       bHasStage ? 1 : 0, bViewsStage ? 1 : 0, bHasStage ? Stage->GetElapsedSeconds() : -1.f,
	       GetWorld()->IsPaused() ? 1 : 0);
	if (not bHasStage)
	{
		Fail(TEXT("the debrief stage did not start"));
	}
	if (bHasStage && not bViewsStage)
	{
		Fail(TEXT("the view did not move to the debrief stage"));
	}
	if (bHasStage && Stage->GetElapsedSeconds() < 0.5f)
	{
		Fail(TEXT("the debrief timeline is not advancing while paused"));
	}
}

void UMissionDebriefValidateSubsystem::CheckButtons()
{
	bM_CheckedButtons = true;
	const UW_MissionEnd* const EndWidget = M_MissionManager.IsValid() ? M_MissionManager->GetMissionEndWidget() : nullptr;
	const bool bRevealed = IsValid(EndWidget) && EndWidget->GetAreButtonsRevealed() && EndWidget->IsInViewport();
	const bool bHasPrimary = IsValid(EndWidget) && IsValid(EndWidget->GetPrimaryButton());
	UE_LOG(LogTemp, Display, TEXT("MISSION_DEBRIEF buttons revealed=%d primary=%d"), bRevealed ? 1 : 0, bHasPrimary ? 1 : 0);
	if (not bRevealed || not bHasPrimary)
	{
		Fail(TEXT("the end screen buttons did not show"));
	}
}

void UMissionDebriefValidateSubsystem::DismissDifficultyPicker() const
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
			Picker->WidgetTree->FindWidget(MissionDebriefValidateConstants::DifficultyButtonName));
		if (IsValid(NormalButton))
		{
			NormalButton->OnClicked.Broadcast();
		}
	}
}

void UMissionDebriefValidateSubsystem::Capture(const FString& Label) const
{
	const FString Outcome = M_Outcome == EMissionDebriefOutcome::Victory ? TEXT("Victory") : TEXT("Defeat");
	const FString ScreenshotPath = FPaths::ProjectSavedDir() / TEXT("MissionDebrief") / Outcome / (Label + TEXT(".png"));
	FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
	UE_LOG(LogTemp, Display, TEXT("MISSION_DEBRIEF capture %s"), *ScreenshotPath);
}

void UMissionDebriefValidateSubsystem::Fail(const FString& Reason)
{
	++M_Failures;
	UE_LOG(LogTemp, Error, TEXT("MISSION_DEBRIEF FAIL %s"), *Reason);
}

void UMissionDebriefValidateSubsystem::Finish()
{
	EnterPhase(EMissionDebriefValidatePhase::Done);
	UE_LOG(LogTemp, Display, TEXT("MISSION_DEBRIEF done: %d failures -> %s"), M_Failures,
	       M_Failures == 0 ? TEXT("PASS") : TEXT("FAIL"));
	RTSFunctionLibrary::PopModalDialogSuppression();
	FPlatformMisc::RequestExit(false, TEXT("MissionDebriefValidate"));
}
