// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTSStandaloneTurretOptimizer.h"

#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/Turret/StandaloneTurret/StandaloneTurret.h"

URTSStandaloneTurretOptimizer::URTSStandaloneTurretOptimizer()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void URTSStandaloneTurretOptimizer::DetermineOwnerOptimization(AActor* ValidOwner)
{
	Super::DetermineOwnerOptimization(ValidOwner);

	M_StandaloneTurret = Cast<AStandaloneTurret>(ValidOwner);
	if (not GetIsValidStandaloneTurret())
	{
		return;
	}

	M_BaseTurretTickInterval = M_StandaloneTurret->GetActorTickInterval();
}

void URTSStandaloneTurretOptimizer::InFOVUpdateComponents()
{
	Super::InFOVUpdateComponents();
	ApplyTurretOptimization(M_BaseTurretTickInterval, ERTSOptimizationDistance::InFOV);
}

void URTSStandaloneTurretOptimizer::OutFovCloseUpdateComponents()
{
	Super::OutFovCloseUpdateComponents();
	ApplyTurretOptimization(
		DeveloperSettings::Optimization::Turrets::OutOfFOVClose_TurretTickInterval,
		ERTSOptimizationDistance::OutFOVClose);
}

void URTSStandaloneTurretOptimizer::OutFovFarUpdateComponents()
{
	Super::OutFovFarUpdateComponents();
	ApplyTurretOptimization(
		DeveloperSettings::Optimization::Turrets::OutOfFOVFar_TurretTickInterval,
		ERTSOptimizationDistance::OutFOVFar);
}

void URTSStandaloneTurretOptimizer::ApplyTurretOptimization(
	const float TickInterval,
	const ERTSOptimizationDistance OptimizationDistance) const
{
	if (not GetIsValidStandaloneTurret())
	{
		return;
	}

	AStandaloneTurret* StandaloneTurret = M_StandaloneTurret.Get();
	StandaloneTurret->SetActorTickInterval(TickInterval);
	StandaloneTurret->ApplyOptimizationDistance(OptimizationDistance);
}

bool URTSStandaloneTurretOptimizer::GetIsValidStandaloneTurret() const
{
	if (M_StandaloneTurret.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_StandaloneTurret"),
		TEXT("GetIsValidStandaloneTurret"),
		this);
	return false;
}
