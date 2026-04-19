// Copyright (C) Bas Blokzijl - All rights reserved.

#include "TankExperienceBehaviour.h"

#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"
#include "Navigation/PathFollowingComponent.h"
#include "RTS_Survival/Units/Tanks/AITankMaster.h"

void UTankExperienceBehaviour::OnAdded(AActor* BehaviourOwner)
{
	CacheTankDependencies();
	ApplyTankSpecificMultipliers();
	Super::OnAdded(BehaviourOwner);
}

void UTankExperienceBehaviour::OnRemoved(AActor* BehaviourOwner)
{
	RestoreTankSpecificMultipliers();
	ClearCachedTankDependencies();
	Super::OnRemoved(BehaviourOwner);
}

void UTankExperienceBehaviour::OnStack(UBehaviour* StackedBehaviour)
{
	Super::OnStack(StackedBehaviour);
	CacheTankDependencies();
	ApplyTankSpecificMultipliers();
}

void UTankExperienceBehaviour::CacheTankDependencies()
{
	ATankMaster* TankMaster = nullptr;
	(void)TryGetTankMaster(TankMaster);

	UTrackPathFollowingComponent* TrackPathFollowingComponent = nullptr;
	(void)TryGetTrackPathFollowingComponent(TrackPathFollowingComponent);
}

void UTankExperienceBehaviour::ApplyTankSpecificMultipliers()
{
	ApplyTurretRotationSpeedMultiplier();
	ApplyTankSpeedMultiplier();
}

void UTankExperienceBehaviour::ApplyTurretRotationSpeedMultiplier()
{
	if (FMath::IsNearlyEqual(TurretRotationSpeedMultiplier, 1.f))
	{
		return;
	}

	ATankMaster* TankMaster = nullptr;
	if (not TryGetTankMaster(TankMaster))
	{
		GetIsValidTankMaster();
		return;
	}

	const TArray<ACPPTurretsMaster*> Turrets = TankMaster->GetTurrets();
	for (ACPPTurretsMaster* Turret : Turrets)
	{
		if (not IsValid(Turret))
		{
			continue;
		}

		const TWeakObjectPtr<ACPPTurretsMaster> TurretPtr = Turret;
		FTankExperienceTurretState& TurretState = M_TurretStateByTurret.FindOrAdd(TurretPtr);
		if (FMath::IsNearlyZero(TurretState.M_BaseRotationSpeed))
		{
			TurretState.M_BaseRotationSpeed = Turret->GetTurretRotationSpeed();
		}

		TurretState.M_AppliedMultiplier *= TurretRotationSpeedMultiplier;
		Turret->SetTurretRotationSpeed(TurretState.M_BaseRotationSpeed * TurretState.M_AppliedMultiplier);
	}
}

void UTankExperienceBehaviour::ApplyTankSpeedMultiplier()
{
	if (FMath::IsNearlyEqual(TankSpeedMultiplier, 1.f))
	{
		return;
	}

	UTrackPathFollowingComponent* TrackPathFollowingComponent = nullptr;
	if (not TryGetTrackPathFollowingComponent(TrackPathFollowingComponent))
	{
		GetIsValidTrackPathFollowingComponent();
		return;
	}

	if (not M_SpeedState.bM_HasCachedBaseDesiredSpeed)
	{
		M_SpeedState.M_BaseDesiredSpeed = TrackPathFollowingComponent->BP_GetDesiredSpeed();
		M_SpeedState.bM_HasCachedBaseDesiredSpeed = true;
	}

	M_SpeedState.M_AppliedMultiplier *= TankSpeedMultiplier;
	TrackPathFollowingComponent->BP_SetNewVehicleSpeed(
		M_SpeedState.M_BaseDesiredSpeed * M_SpeedState.M_AppliedMultiplier,
		ESpeedUnits::CentimetersPerSecond
	);
}

void UTankExperienceBehaviour::RestoreTankSpecificMultipliers()
{
	RestoreTurretRotationSpeedMultiplier();
	RestoreTankSpeedMultiplier();
}

void UTankExperienceBehaviour::RestoreTurretRotationSpeedMultiplier()
{
	for (auto It = M_TurretStateByTurret.CreateIterator(); It; ++It)
	{
		const TWeakObjectPtr<ACPPTurretsMaster> TurretPtr = It.Key();
		const FTankExperienceTurretState TurretState = It.Value();
		if (not TurretPtr.IsValid())
		{
			continue;
		}

		TurretPtr->SetTurretRotationSpeed(TurretState.M_BaseRotationSpeed);
	}

	M_TurretStateByTurret.Empty();
}

void UTankExperienceBehaviour::RestoreTankSpeedMultiplier()
{
	if (not M_SpeedState.bM_HasCachedBaseDesiredSpeed)
	{
		return;
	}

	if (not GetIsValidTrackPathFollowingComponent())
	{
		return;
	}

	M_TrackPathFollowingComponent->BP_SetNewVehicleSpeed(
		M_SpeedState.M_BaseDesiredSpeed,
		ESpeedUnits::CentimetersPerSecond
	);
	M_SpeedState.M_AppliedMultiplier = 1.f;
	M_SpeedState.bM_HasCachedBaseDesiredSpeed = false;
}

void UTankExperienceBehaviour::ClearCachedTankDependencies()
{
	M_TankMaster = nullptr;
	M_TrackPathFollowingComponent = nullptr;
	M_TurretStateByTurret.Empty();
}

bool UTankExperienceBehaviour::TryGetTankMaster(ATankMaster*& OutTankMaster)
{
	OutTankMaster = M_TankMaster.Get();
	if (IsValid(OutTankMaster))
	{
		return true;
	}

	OutTankMaster = Cast<ATankMaster>(GetOwningActor());
	if (not IsValid(OutTankMaster))
	{
		return false;
	}

	M_TankMaster = OutTankMaster;
	return true;
}

bool UTankExperienceBehaviour::TryGetTrackPathFollowingComponent(
	UTrackPathFollowingComponent*& OutTrackPathFollowingComponent)
{
	OutTrackPathFollowingComponent = M_TrackPathFollowingComponent.Get();
	if (IsValid(OutTrackPathFollowingComponent))
	{
		return true;
	}

	ATankMaster* TankMaster = nullptr;
	if (not TryGetTankMaster(TankMaster))
	{
		return false;
	}

	if (not IsValid(TankMaster->GetAIController()))
	{
		return false;
	}

	UPathFollowingComponent* PathFollowingComponent = TankMaster->GetAIController()->GetPathFollowingComponent();
	OutTrackPathFollowingComponent = Cast<UTrackPathFollowingComponent>(PathFollowingComponent);
	if (not IsValid(OutTrackPathFollowingComponent))
	{
		return false;
	}

	M_TrackPathFollowingComponent = OutTrackPathFollowingComponent;
	return true;
}

bool UTankExperienceBehaviour::GetIsValidTankMaster() const
{
	if (M_TankMaster.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_TankMaster",
		"GetIsValidTankMaster",
		GetOwningActor()
	);
	return false;
}

bool UTankExperienceBehaviour::GetIsValidTrackPathFollowingComponent() const
{
	if (M_TrackPathFollowingComponent.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_TrackPathFollowingComponent",
		"GetIsValidTrackPathFollowingComponent",
		GetOwningActor()
	);
	return false;
}
