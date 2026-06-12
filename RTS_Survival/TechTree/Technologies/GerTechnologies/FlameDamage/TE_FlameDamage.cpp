// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "TE_FlameDamage.h"

#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Weapons/FlameThrowerWeapon/UWeaponStateFlameThrower.h"
#include "RTS_Survival/Weapons/HullWeaponComponent/HullWeaponComponent.h"
#include "RTS_Survival/Weapons/InfantryWeapon/InfantryWeaponMaster.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"

namespace FlameDamageTechnologySettings
{
	const FLinearColor DefaultFlameColor = FLinearColor(1.f, 0.18f, 0.02f, 1.f);
}

UTE_FlameDamage::UTE_FlameDamage()
	: M_FlameColor(FlameDamageTechnologySettings::DefaultFlameColor)
{
	Technology = ETechnology::Tech_FlameDamage;
	TanksToApplyTo = {
		ETankSubtype::Tank_FlamePuma,
		ETankSubtype::Tank_PZII_Flame,
		ETankSubtype::Tank_PzIII_FLamm,
		ETankSubtype::Tank_IS_3_Flame,
	};
	SquadsToApplyTo = {
		ESquadSubtype::Squad_Rus_ToxicGuard,
	};
}

void UTE_FlameDamage::ApplyOnTank_Internal(ATankMaster* Tank)
{
	if (not IsValid(Tank))
	{
		return;
	}

	ApplyFlameColorToTankTurrets(*Tank);
	ApplyFlameColorToTankHullWeapons(*Tank);
}

void UTE_FlameDamage::ApplyOnSquad_Internal(ASquadController* Squad)
{
	if (not IsValid(Squad))
	{
		return;
	}

	for (ASquadUnit* SquadUnit : Squad->GetSquadUnitsChecked())
	{
		if (not IsValid(SquadUnit))
		{
			continue;
		}

		ApplyFlameColorToSquadUnit(*SquadUnit);
	}
}

void UTE_FlameDamage::ApplyFlameColorToTankTurrets(ATankMaster& Tank) const
{
	for (ACPPTurretsMaster* Turret : Tank.GetTurrets())
	{
		if (not IsValid(Turret))
		{
			continue;
		}

		ApplyFlameColorToTurret(*Turret);
	}
}

void UTE_FlameDamage::ApplyFlameColorToTankHullWeapons(ATankMaster& Tank) const
{
	for (UHullWeaponComponent* HullWeapon : Tank.GetHullWeapons())
	{
		if (not IsValid(HullWeapon))
		{
			continue;
		}

		ApplyFlameColorToHullWeapon(*HullWeapon);
	}
}

void UTE_FlameDamage::ApplyFlameColorToSquadUnit(ASquadUnit& SquadUnit) const
{
	AInfantryWeaponMaster* InfantryWeapon = SquadUnit.GetInfantryWeapon();
	if (not IsValid(InfantryWeapon))
	{
		return;
	}

	ApplyFlameColorToInfantryWeapon(*InfantryWeapon);
}

void UTE_FlameDamage::ApplyFlameColorToInfantryWeapon(AInfantryWeaponMaster& InfantryWeapon) const
{
	ApplyFlameColorToWeapons(InfantryWeapon.GetWeapons());
}

void UTE_FlameDamage::ApplyFlameColorToTurret(ACPPTurretsMaster& Turret) const
{
	ApplyFlameColorToWeapons(Turret.GetWeapons());
}

void UTE_FlameDamage::ApplyFlameColorToHullWeapon(UHullWeaponComponent& HullWeapon) const
{
	ApplyFlameColorToWeapons(HullWeapon.GetWeapons());
}

void UTE_FlameDamage::ApplyFlameColorToWeapons(const TArray<UWeaponState*>& Weapons) const
{
	for (UWeaponState* Weapon : Weapons)
	{
		UWeaponStateFlameThrower* FlameThrower = Cast<UWeaponStateFlameThrower>(Weapon);
		if (not IsValid(FlameThrower))
		{
			continue;
		}

		FlameThrower->SetFlameColor(M_FlameColor);
	}
}
