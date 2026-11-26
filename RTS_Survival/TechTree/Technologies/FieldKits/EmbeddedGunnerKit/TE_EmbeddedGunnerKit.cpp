// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#include "TE_EmbeddedGunnerKit.h"

#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/Turret/EmbeddedInfantry/EmbeddedTurretMountComponent.h"

UTE_EmbeddedGunnerKit::UTE_EmbeddedGunnerKit()
{
	Technology = ETechnology::Tech_FieldKit_HatchGunnerMG42;
	// The owner's pick (2026-11-26). The Pz III J and Pz IV F1 are left out on purpose; variants share their base's
	// subtype and inherit its kit mount.
	TanksToApplyTo = {
		ETankSubtype::Tank_PzIII_M,
		ETankSubtype::Tank_PzIII_FLamm,
		ETankSubtype::Tank_Pz38t_R,
		ETankSubtype::Tank_Jaguar,
		ETankSubtype::Tank_PzIV_G,
		ETankSubtype::Tank_PzIV_H,
		ETankSubtype::Tank_Pz38t,
		ETankSubtype::Tank_Pz38t_RailGun,
	};
}

void UTE_EmbeddedGunnerKit::ApplyOnTank_Internal(ATankMaster* Tank)
{
	if (not IsValid(Tank))
	{
		return;
	}

	TArray<UEmbeddedTurretMountComponent*> Mounts;
	Tank->GetComponents<UEmbeddedTurretMountComponent>(Mounts);
	UnmountReplacedGunners(Mounts);
	if (MountKitGunners(Mounts) > 0)
	{
		return;
	}
	RTSFunctionLibrary::ReportError("Field kit " + Global_GetTechDisplayName(Technology) + " applies to "
		+ Tank->GetName() + " but the tank has no UEmbeddedTurretMountComponent with RequiredTechnology "
		+ Global_GetTechAsString(Technology) + "; it gets no gunner.");
}

int32 UTE_EmbeddedGunnerKit::MountKitGunners(const TArray<UEmbeddedTurretMountComponent*>& Mounts) const
{
	int32 RequestedMountCount = 0;
	for (UEmbeddedTurretMountComponent* Mount : Mounts)
	{
		if (not IsValid(Mount) || Mount->GetRequiredTechnology() != Technology)
		{
			continue;
		}
		Mount->RequestMount();
		++RequestedMountCount;
	}
	return RequestedMountCount;
}

void UTE_EmbeddedGunnerKit::UnmountReplacedGunners(const TArray<UEmbeddedTurretMountComponent*>& Mounts) const
{
	for (UEmbeddedTurretMountComponent* Mount : Mounts)
	{
		if (IsValid(Mount) && Mount->GetUnmountOnTechnology() == Technology)
		{
			Mount->Unmount();
		}
	}
}
