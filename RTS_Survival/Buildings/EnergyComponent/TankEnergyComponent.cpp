// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "TankEnergyComponent.h"

#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"

void UTankEnergyComponent::CacheOwnerMaterials()
{
	ATankMaster* TankMaster = Cast<ATankMaster>(GetOwner());
	if (TankMaster == nullptr)
	{
		return;
	}

	CacheMeshesFromActor(TankMaster);

	const TArray<ACPPTurretsMaster*> Turrets = TankMaster->GetTurrets();
	for (ACPPTurretsMaster* Turret : Turrets)
	{
		CacheMeshesFromActor(Turret);
	}
}
