// Copyright (C) Bas Blokzijl - All rights reserved.

#include "LowEnergyBehaviour.h"

#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"

ULowEnergyBehaviour::ULowEnergyBehaviour()
{
}

void ULowEnergyBehaviour::OnAdded(AActor* BehaviourOwner)
{
	Super::OnAdded(BehaviourOwner);
	ApplyLowEnergyTurretRotation();
}

void ULowEnergyBehaviour::OnRemoved(AActor* BehaviourOwner)
{
	RestoreTurretRotation();
	Super::OnRemoved(BehaviourOwner);
}

void ULowEnergyBehaviour::ApplyLowEnergyTurretRotation()
{
	if (M_CachedTurretRotationSpeeds.Num() > 0)
	{
		return;
	}

	TArray<ACPPTurretsMaster*> Turrets;
	if (not TryGetOwnerTurrets(Turrets))
	{
		return;
	}

	for (ACPPTurretsMaster* Turret : Turrets)
	{
		if (Turret == nullptr)
		{
			continue;
		}

		const TWeakObjectPtr<ACPPTurretsMaster> TurretPtr = Turret;
		if (M_CachedTurretRotationSpeeds.Contains(TurretPtr))
		{
			continue;
		}

		const float BaseRotationSpeed = Turret->RotationSpeed;
		M_CachedTurretRotationSpeeds.Add(TurretPtr, BaseRotationSpeed);
		Turret->RotationSpeed = BaseRotationSpeed * TurretLowEnergyRotationSpeedMlt;
	}
}

void ULowEnergyBehaviour::RestoreTurretRotation()
{
	for (const TPair<TWeakObjectPtr<ACPPTurretsMaster>, float>& CachedTurret : M_CachedTurretRotationSpeeds)
	{
		if (not CachedTurret.Key.IsValid())
		{
			continue;
		}

		CachedTurret.Key->RotationSpeed = CachedTurret.Value;
	}

	M_CachedTurretRotationSpeeds.Empty();
}

bool ULowEnergyBehaviour::TryGetOwnerTurrets(TArray<ACPPTurretsMaster*>& OutTurrets) const
{
	AActor* OwnerActor = GetOwningActor();
	if (OwnerActor == nullptr)
	{
		return false;
	}

	if (ATankMaster* TankMaster = Cast<ATankMaster>(OwnerActor))
	{
		OutTurrets = TankMaster->GetTurrets();
		return true;
	}

	if (ABuildingExpansion* BuildingExpansion = Cast<ABuildingExpansion>(OwnerActor))
	{
		OutTurrets = BuildingExpansion->GetTurrets();
		return true;
	}

	return false;
}
