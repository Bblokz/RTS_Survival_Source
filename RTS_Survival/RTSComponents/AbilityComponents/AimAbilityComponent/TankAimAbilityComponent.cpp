// Copyright (C) Bas Blokzijl - All rights reserved.

#include "TankAimAbilityComponent.h"

#include "RTS_Survival/Units/Tanks/AITankMaster.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

namespace AimAbilityConstants
{
	constexpr float TurretRotationCheckIntervalSeconds = 0.2f;
}

UTankAimAbilityComponent::UTankAimAbilityComponent()
	: UAimAbilityComponent()
{
}

void UTankAimAbilityComponent::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_CacheTurretNextTick();
}

void UTankAimAbilityComponent::PostInitProperties()
{
	Super::PostInitProperties();
	M_TankMaster = Cast<ATankMaster>(GetOwner());
}

bool UTankAimAbilityComponent::CollectWeaponStates(TArray<UWeaponState*>& OutWeaponStates, float& OutMaxRange) const
{
	OutWeaponStates.Reset();
	OutMaxRange = 0.0f;
	if (not GetIsValidTurretWithMostRange())
	{
		return false;
	}

	OutMaxRange = GetTurretMaxRange(M_TurretWithMostRange.Get(), OutWeaponStates);
	if (OutWeaponStates.Num() > 0)
	{
		return true;
	}

	RTSFunctionLibrary::ReportError("Aim ability could not find valid turret weapon states.");
	return false;
}

void UTankAimAbilityComponent::SetWeaponsDisabled()
{
	if (not GetIsValidTurretWithMostRange())
	{
		return;
	}
	M_TurretWithMostRange->DisableTurret();
}

void UTankAimAbilityComponent::SetWeaponsAutoEngage(const bool bUseLastTarget)
{
	if (not GetIsValidTurretWithMostRange())
	{
		return;
	}
	M_TurretWithMostRange->SetAutoEngageTargets(bUseLastTarget);
}

void UTankAimAbilityComponent::FireWeaponsAtLocation(const FVector& TargetLocation)
{
	if (not GetIsValidTurretWithMostRange())
	{
		return;
	}
	M_TurretWithMostRange->SetEngageGroundLocation(TargetLocation);
}

void UTankAimAbilityComponent::RequestMoveToLocation(const FVector& MoveToLocation)
{
	if (not GetIsValidTankMaster())
	{
		return;
	}
	if (not M_TankMaster->GetIsValidAIController())
	{
		return;
	}
	AAITankMaster* TankController = M_TankMaster->GetAIController();
	TankController->MoveToLocationWithGoalAcceptance(MoveToLocation);
}

void UTankAimAbilityComponent::StopMovementForAbility()
{
	if (not GetIsValidTankMaster())
	{
		return;
	}
	if (not M_TankMaster->GetIsValidAIController())
	{
		return;
	}
	AAITankMaster* TankController = M_TankMaster->GetAIController();
	TankController->StopMovement();
}

void UTankAimAbilityComponent::BeginAbilityDurationTimer()
{
	ClearTurretRotationTimer();
	UWorld* World = GetWorld();
	if (not World)
	{
		HandleBehaviourDurationFinished(true);
		return;
	}

	FTimerDelegate RotationTimerDelegate;
	TWeakObjectPtr<UTankAimAbilityComponent> WeakThis(this);
	RotationTimerDelegate.BindLambda([WeakThis]()
	{
		if (not WeakThis.IsValid())
		{
			return;
		}
		WeakThis->CheckTurretRotationAndStartBehaviour();
	});
	World->GetTimerManager().SetTimer(
		M_TurretRotationCheckTimerHandle,
		RotationTimerDelegate,
		AimAbilityConstants::TurretRotationCheckIntervalSeconds,
		true);
}

void UTankAimAbilityComponent::ClearDerivedTimers()
{
	ClearTurretRotationTimer();
}

void UTankAimAbilityComponent::BeginPlay_CacheTurretNextTick()
{
	UWorld* World = GetWorld();
	if (not World)
	{
		return;
	}

	FTimerDelegate CacheDelegate;
	TWeakObjectPtr<UTankAimAbilityComponent> WeakThis(this);
	CacheDelegate.BindLambda([WeakThis]()
	{
		if (not WeakThis.IsValid())
		{
			return;
		}
		WeakThis->CacheTurretWithMostRange();
	});
	World->GetTimerManager().SetTimerForNextTick(CacheDelegate);
}

void UTankAimAbilityComponent::CacheTurretWithMostRange()
{
	if (not GetIsValidTankMaster())
	{
		return;
	}

	constexpr float UnsetRange = -1.0f;
	float BestRange = UnsetRange;
	ACPPTurretsMaster* BestTurret = nullptr;
	const TArray<ACPPTurretsMaster*> Turrets = M_TankMaster->GetTurrets();
	for (ACPPTurretsMaster* Turret : Turrets)
	{
		if (not IsValid(Turret))
		{
			continue;
		}
		TArray<UWeaponState*> WeaponStates;
		const float TurretRange = GetTurretMaxRange(Turret, WeaponStates);
		if (TurretRange > BestRange)
		{
			BestRange = TurretRange;
			BestTurret = Turret;
		}
	}

	if (not IsValid(BestTurret))
	{
		RTSFunctionLibrary::ReportError("Aim ability could not cache a turret with range.");
		return;
	}

	M_TurretWithMostRange = BestTurret;
}

float UTankAimAbilityComponent::GetTurretMaxRange(const ACPPTurretsMaster* Turret,
                                                  TArray<UWeaponState*>& OutWeaponStates) const
{
	OutWeaponStates.Reset();
	if (not IsValid(Turret))
	{
		return 0.0f;
	}

	float MaxRange = 0.0f;
	const TArray<UWeaponState*> WeaponStates = Turret->GetWeapons();
	for (UWeaponState* WeaponState : WeaponStates)
	{
		if (not IsValid(WeaponState))
		{
			continue;
		}
		OutWeaponStates.Add(WeaponState);
		MaxRange = FMath::Max(MaxRange, WeaponState->GetRange());
	}
	return MaxRange;
}

void UTankAimAbilityComponent::CheckTurretRotationAndStartBehaviour()
{
	if (not GetIsValidTurretWithMostRange())
	{
		return;
	}

	if (not M_TurretWithMostRange->GetIsRotatedToEngage())
	{
		return;
	}

	ClearTurretRotationTimer();
	StartBehaviourTimer(M_AimAbilitySettings.BehaviourDuration);
}

void UTankAimAbilityComponent::ClearTurretRotationTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_TurretRotationCheckTimerHandle);
	}
}

bool UTankAimAbilityComponent::GetIsValidTankMaster() const
{
	if (not M_TankMaster.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_TankMaster",
		                                                      "UTankAimAbilityComponent::GetIsValidTankMaster", this);
		return false;
	}

	return true;
}

bool UTankAimAbilityComponent::GetIsValidTurretWithMostRange() const
{
	if (not M_TurretWithMostRange.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_TurretWithMostRange",
		                                                      "UTankAimAbilityComponent::GetIsValidTurretWithMostRange",
		                                                      this);
		return false;
	}

	return true;
}
