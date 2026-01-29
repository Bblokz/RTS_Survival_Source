// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "TankEnergyComponent.h"

#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"

void UTankEnergyComponent::CacheOwnerMaterials()
{
	ATankMaster* TankMaster = Cast<ATankMaster>(GetOwner());
	if (TankMaster == nullptr)
	{
		const FString OwnerName = IsValid(GetOwner()) ? GetOwner()->GetName() : "Invalid Owner";
		RTSFunctionLibrary::ReportError("Tank energy component is not attached to a tank master!"
			"\n Component: " + GetName()
			+ "\n Owner: " + OwnerName);
		return;
	}

	CacheMeshesFromActor(TankMaster);

	const TArray<ACPPTurretsMaster*> Turrets = TankMaster->GetTurrets();
	for (ACPPTurretsMaster* Turret : Turrets)
	{
		CacheMeshesFromActor(Turret);
	}
}
