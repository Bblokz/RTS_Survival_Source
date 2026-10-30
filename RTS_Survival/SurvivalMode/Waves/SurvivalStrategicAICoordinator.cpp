// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalStrategicAICoordinator.h"

#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/StrategicAI/Component/EnemyStrategicAIComponent.h"
#include "RTS_Survival/SurvivalMode/Director/SurvivalModeDirector.h"
#include "RTS_Survival/SurvivalMode/Waves/SurvivalWaveSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace SurvivalStrategicAICoordinatorConstants
{
	constexpr float SecondsPerMinute = 60.f;
}

void USurvivalStrategicAICoordinator::InitCoordinator(ASurvivalModeDirector* Director,
                                                      USurvivalWaveSettings* WaveSettings)
{
	M_Director = Director;
	M_WaveSettings = WaveSettings;
}

void USurvivalStrategicAICoordinator::OnGracePeriodStarted()
{
	if (not GetShouldConfigure())
	{
		return;
	}
	ConfigureStrategicAIOnce();
	if (M_WaveSettings->StrategicAI.StartMoment == ESurvivalStrategicAIStartMoment::GracePeriodStart)
	{
		ApplyMissionSettings(M_WaveSettings->StrategicAI.bAllowDirectControlDuringGrace);
		StartStrategicThinking();
	}
}

void USurvivalStrategicAICoordinator::OnWavesStarted()
{
	if (not GetShouldConfigure())
	{
		return;
	}
	bM_WavesRunning = true;
	ConfigureStrategicAIOnce();
	ApplyMissionSettings(true);
	StartStrategicThinking();
	StartProfileUpdates();
}

void USurvivalStrategicAICoordinator::OnWavesStopped()
{
	bM_WavesRunning = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_ProfileUpdateTimerHandle);
	}
	if (not GetShouldConfigure() or not M_WaveSettings->StrategicAI.bStopStrategicAIOnDefeat)
	{
		return;
	}
	UEnemyStrategicAIComponent* StrategicAI = GetStrategicAIComponent();
	if (not StrategicAI)
	{
		return;
	}
	FEnemyAIMissionSettings& MissionSettings = StrategicAI->GetEditableStrategicAIBlackboard().StrategicAIMissionSettings;
	MissionSettings.bAllowDirectControlStochasticDecisionTree = false;
	MissionSettings.bAllowUnitTraining = false;
}

void USurvivalStrategicAICoordinator::Shutdown()
{
	bM_WavesRunning = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_ProfileUpdateTimerHandle);
	}
}

UWorld* USurvivalStrategicAICoordinator::GetWorld() const
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}
	if (const ASurvivalModeDirector* Director = M_Director.Get())
	{
		return Director->GetWorld();
	}
	const UObject* Outer = GetOuter();
	return Outer ? Outer->GetWorld() : nullptr;
}

bool USurvivalStrategicAICoordinator::GetIsValidDirector() const
{
	if (M_Director.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_Director", "GetIsValidDirector", this);
	return false;
}

bool USurvivalStrategicAICoordinator::GetIsValidWaveSettings() const
{
	if (M_WaveSettings.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_WaveSettings", "GetIsValidWaveSettings",
	                                                             this);
	return false;
}

AEnemyController* USurvivalStrategicAICoordinator::GetEnemyController()
{
	if (M_EnemyController.IsValid())
	{
		return M_EnemyController.Get();
	}
	AEnemyController* EnemyController = FRTS_Statics::GetEnemyController(this);
	if (not IsValid(EnemyController))
	{
		if (bM_ReportedMissingEnemyController)
		{
			return nullptr;
		}
		bM_ReportedMissingEnemyController = true;
		RTSFunctionLibrary::ReportError(
			"Survival strategic AI coordinator found no AEnemyController in the level; Soviet bases and wave "
			"survivors will not be commanded.\nAt USurvivalStrategicAICoordinator::GetEnemyController");
		return nullptr;
	}
	M_EnemyController = EnemyController;
	return EnemyController;
}

UEnemyStrategicAIComponent* USurvivalStrategicAICoordinator::GetStrategicAIComponent()
{
	AEnemyController* EnemyController = GetEnemyController();
	if (not EnemyController)
	{
		return nullptr;
	}
	UEnemyStrategicAIComponent* StrategicAI = EnemyController->GetEnemyStrategicAIComponent();
	if (not IsValid(StrategicAI))
	{
		RTSFunctionLibrary::ReportError(
			"Enemy controller has no strategic AI component.\nAt USurvivalStrategicAICoordinator");
		return nullptr;
	}
	return StrategicAI;
}

bool USurvivalStrategicAICoordinator::GetShouldConfigure() const
{
	if (not GetIsValidWaveSettings())
	{
		return false;
	}
	return M_WaveSettings->StrategicAI.bConfigureStrategicAI;
}

void USurvivalStrategicAICoordinator::ConfigureStrategicAIOnce()
{
	if (bM_HasConfiguredAI)
	{
		return;
	}
	UEnemyStrategicAIComponent* StrategicAI = GetStrategicAIComponent();
	if (not StrategicAI)
	{
		return;
	}
	bM_HasConfiguredAI = true;

	FStrategicAIBlackboard& Blackboard = StrategicAI->GetEditableStrategicAIBlackboard();
	// Survival maps have no mission manager to propagate the difficulty.
	Blackboard.GameDifficulty = GetDifficulty();

	FEnemyStrategicTrainingState& TrainingState = StrategicAI->GetEditableEnemyTrainingState();
	TrainingState.TrainingPoints = FMath::Max(TrainingState.TrainingPoints,
	                                          M_WaveSettings->StrategicAI.TrainingPointsAtStart);

	ApplyDecisionTreeOverride();
	ApplyMissionSettings(false);
	UpdateFromProfile();
}

void USurvivalStrategicAICoordinator::ApplyMissionSettings(const bool bAllowDirectControl)
{
	UEnemyStrategicAIComponent* StrategicAI = GetStrategicAIComponent();
	if (not StrategicAI or not GetIsValidWaveSettings())
	{
		return;
	}
	const FSurvivalStrategicAISettings& AISettings = M_WaveSettings->StrategicAI;
	FEnemyAIMissionSettings& MissionSettings = StrategicAI->GetEditableStrategicAIBlackboard().StrategicAIMissionSettings;
	MissionSettings.StrategicAIBehaviour = AISettings.Behaviour;
	MissionSettings.bAllowUnitTraining = AISettings.bAllowUnitTraining;
	MissionSettings.bAllowDirectControlStochasticDecisionTree = bAllowDirectControl;
	if (const FSurvivalWaveDifficultyProfile* Profile = GetProfile())
	{
		MissionSettings.MinPickedUnitsForAction = Profile->StrategicMinPickedUnitsForAction;
		MissionSettings.MaxPickedUnitsForAction = FMath::Max(Profile->StrategicMinPickedUnitsForAction,
		                                                     Profile->StrategicMaxPickedUnitsForAction);
	}
}

void USurvivalStrategicAICoordinator::ApplyDecisionTreeOverride()
{
	if (not GetIsValidWaveSettings())
	{
		return;
	}
	const FSurvivalStrategicAISettings& AISettings = M_WaveSettings->StrategicAI;
	if (not AISettings.bOverrideDecisionTree)
	{
		return;
	}
	if (not IsValid(AISettings.DecisionTreeTemplate))
	{
		RTSFunctionLibrary::ReportWarning(
			"Survival wave settings want to override the decision tree but DecisionTreeTemplate is empty; the "
			"map-authored tree of the AEnemyController is used.");
		return;
	}
#if RTS_SURVIVAL_WAVES_WITH_TREE_OVERRIDE
	AEnemyController* EnemyController = GetEnemyController();
	if (not EnemyController)
	{
		return;
	}
	EnemyController->OverrideStrategicDecisionTree(AISettings.DecisionTreeTemplate);
#else
	RTSFunctionLibrary::ReportWarning(
		"Survival decision tree override is not compiled in (RTS_SURVIVAL_WAVES_WITH_TREE_OVERRIDE = 0); the "
		"map-authored tree of the AEnemyController is used. See SurvivalMode/Docs/40_WavesAndAI.md.");
#endif
}

void USurvivalStrategicAICoordinator::StartStrategicThinking()
{
	UEnemyStrategicAIComponent* StrategicAI = GetStrategicAIComponent();
	if (not StrategicAI)
	{
		return;
	}
	// Guarded against double timers inside; works whatever start condition the map's controller has.
	StrategicAI->StartStrategicAIThinkingTimer();
}

void USurvivalStrategicAICoordinator::StartProfileUpdates()
{
	UWorld* World = GetWorld();
	if (not IsValid(World) or not GetIsValidWaveSettings())
	{
		return;
	}
	World->GetTimerManager().SetTimer(
		M_ProfileUpdateTimerHandle,
		this,
		&USurvivalStrategicAICoordinator::UpdateFromProfile,
		M_WaveSettings->StrategicAI.ProfileUpdateIntervalSeconds,
		true);
}

void USurvivalStrategicAICoordinator::UpdateFromProfile()
{
	const FSurvivalWaveDifficultyProfile* Profile = GetProfile();
	UEnemyStrategicAIComponent* StrategicAI = GetStrategicAIComponent();
	if (not Profile or not StrategicAI)
	{
		return;
	}
	const float Minute = GetSurvivalMinute();
	const float PointsPerMinute = USurvivalWaveSettings::EvaluateCurveAtMinute(
		Profile->StrategicTrainingPointsPerMinute, Minute, 0.f);
	FEnemyStrategicTrainingState& TrainingState = StrategicAI->GetEditableEnemyTrainingState();
	TrainingState.EnemyLevelTraining.TrainingPointsPerMinute = FMath::Max(0, FMath::RoundToInt(PointsPerMinute));
}

const FSurvivalWaveDifficultyProfile* USurvivalStrategicAICoordinator::GetProfile() const
{
	if (not M_WaveSettings.IsValid())
	{
		return nullptr;
	}
	return &M_WaveSettings->GetProfileForDifficulty(GetDifficulty().DifficultyLevel);
}

FRTSGameDifficulty USurvivalStrategicAICoordinator::GetDifficulty() const
{
	if (not M_Director.IsValid())
	{
		return FRTSGameDifficulty();
	}
	return M_Director->GetDifficulty();
}

float USurvivalStrategicAICoordinator::GetSurvivalMinute() const
{
	if (not M_Director.IsValid())
	{
		return 0.f;
	}
	return M_Director->GetElapsedSurvivalSeconds() / SurvivalStrategicAICoordinatorConstants::SecondsPerMinute;
}
