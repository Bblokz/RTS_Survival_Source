// Copyright (C) Bas Blokzijl - All rights reserved.


#include "RTSSquadUnitOptimizer.h"

#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"


URTSSquadUnitOptimizer::URTSSquadUnitOptimizer()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}


void URTSSquadUnitOptimizer::BeginPlay()
{
	Super::BeginPlay();
}


void URTSSquadUnitOptimizer::InFOVUpdateComponents()
{
	Super::InFOVUpdateComponents();
	if (not M_MvtOptimizationData.CharacterMovementComponent)
	{
		return;
	}
	const float Interval = M_MvtOptimizationData.BaseTickInterval;
	const int32 MaxSimulations = M_MvtOptimizationData.BaseMaxSimulationIterations;
	const float MaxSimulationTimeStep = M_MvtOptimizationData.BaseMaxSimulationTimeStep;
	M_MvtOptimizationData.CharacterMovementComponent->SetComponentTickInterval(Interval);
	M_MvtOptimizationData.CharacterMovementComponent->MaxSimulationIterations = MaxSimulations;
	M_MvtOptimizationData.CharacterMovementComponent->MaxSimulationTimeStep = MaxSimulationTimeStep;
}

void URTSSquadUnitOptimizer::OutFovCloseUpdateComponents()
{
	Super::OutFovCloseUpdateComponents();
	if (not M_MvtOptimizationData.CharacterMovementComponent)
	{
		return;
	}
	const float Interval = M_MvtOptimizationData.CloseOutFovInterval;
	const int32 MaxSimulations = M_MvtOptimizationData.OutFOVClose_MaxSimulationIterations;
	const float MaxSimulationTimeStep = M_MvtOptimizationData.OutFOVClose_MaxSimulationTimeStep;
	M_MvtOptimizationData.CharacterMovementComponent->SetComponentTickInterval(Interval);
	M_MvtOptimizationData.CharacterMovementComponent->MaxSimulationIterations = MaxSimulations;
	M_MvtOptimizationData.CharacterMovementComponent->MaxSimulationTimeStep = MaxSimulationTimeStep;
}

void URTSSquadUnitOptimizer::OutFovFarUpdateComponents()
{
	Super::OutFovFarUpdateComponents();
	if (not M_MvtOptimizationData.CharacterMovementComponent)
	{
		return;
	}
	const float Interval = M_MvtOptimizationData.FarOutFovInterval;
	const int32 MaxSimulations = M_MvtOptimizationData.OutFOVFar_MaxSimulationIterations;
	const float MaxSimulationTimeStep = M_MvtOptimizationData.OutFOVFar_MaxSimulationTimeStep;
	M_MvtOptimizationData.CharacterMovementComponent->SetComponentTickInterval(Interval);
	M_MvtOptimizationData.CharacterMovementComponent->MaxSimulationIterations = MaxSimulations;
	M_MvtOptimizationData.CharacterMovementComponent->MaxSimulationTimeStep = MaxSimulationTimeStep;
}

void URTSSquadUnitOptimizer::DetermineCharMovtOptimization(UCharacterMovementComponent* CharacterMovementComponent)
{
	using DeveloperSettings::Optimization::CharacterMovement::OutOfFOVClose_CharacterMovementSimulationSteps;
	using DeveloperSettings::Optimization::CharacterMovement::OutOfFOVClose_CharacterMovementTickInterval;
	using DeveloperSettings::Optimization::CharacterMovement::OutOfFOVFar_CharacterMovementSimulationSteps;
	using DeveloperSettings::Optimization::CharacterMovement::OutOfFOVFar_CharacterMovementTickInterval;
	if (not CharacterMovementComponent)
	{
		RTSFunctionLibrary::ReportError("failed to cast to char movement component in squad unit optimizer!");
		return;
	}
	// The delta time is sometimes larger than the tick interval set and it is possible in those cases to overshoot
	// MaxSteps * SimulTime which gives a warning. We use epsilon to give some leeway.
	constexpr float Epsilon = 1.2;
	M_MvtOptimizationData.CharacterMovementComponent = CharacterMovementComponent;
	// Close out FOV settings.
	M_MvtOptimizationData.CloseOutFovInterval = OutOfFOVClose_CharacterMovementTickInterval;
	M_MvtOptimizationData.OutFOVClose_MaxSimulationIterations =
		OutOfFOVClose_CharacterMovementSimulationSteps;
	M_MvtOptimizationData.OutFOVClose_MaxSimulationTimeStep = OutOfFOVClose_CharacterMovementTickInterval /
		OutOfFOVClose_CharacterMovementSimulationSteps * Epsilon;

	// Far out FOV settings.
	M_MvtOptimizationData.FarOutFovInterval = OutOfFOVFar_CharacterMovementTickInterval;
	M_MvtOptimizationData.OutFOVFar_MaxSimulationIterations = OutOfFOVFar_CharacterMovementSimulationSteps;
	M_MvtOptimizationData.OutFOVFar_MaxSimulationTimeStep = OutOfFOVFar_CharacterMovementTickInterval /
		OutOfFOVFar_CharacterMovementSimulationSteps * Epsilon;

	// Set base settings.
	M_MvtOptimizationData.BaseMaxSimulationTimeStep =
		CharacterMovementComponent->MaxSimulationTimeStep;
	M_MvtOptimizationData.BaseMaxSimulationIterations =
		CharacterMovementComponent->MaxSimulationIterations;
}
