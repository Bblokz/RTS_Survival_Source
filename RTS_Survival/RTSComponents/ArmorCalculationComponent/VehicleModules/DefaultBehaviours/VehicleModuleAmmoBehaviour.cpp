// Copyright (C) Bas Blokzijl - All rights reserved.

#include "VehicleModuleAmmoBehaviour.h"

#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/VehicleModules/VehicleModuleBalance.h"

UVehicleModuleAmmoBehaviour::UVehicleModuleAmmoBehaviour()
{
	BehaviourLifeTime = EBehaviourLifeTime::None;
	M_BuffType = EBuffDebuffType::Debuff;
	M_TitleText = TEXT("Damaged Ammunition");
	M_DisplayText = TEXT("Large guns reload more slowly; destroyed ammunition prevents new reloads.");
	BehaviourWeaponMultipliers.ReloadSpeedMlt =
		VehicleModuleBalance::DefaultBehaviours::AmmoYellowReloadDurationMultiplier;
}

bool UVehicleModuleAmmoBehaviour::CheckRequirement(UWeaponState* WeaponState) const
{
	return IsValid(WeaponState) && GetModuleContext().Type == EVehicleModuleTypes::Ammo
		&& WeaponState->GetRawWeaponData().WeaponCalibre
		> VehicleModuleBalance::DefaultBehaviours::AmmoAffectedWeaponCalibreExclusiveMm;
}

void UVehicleModuleAmmoBehaviour::ApplyBehaviourToWeapon(UWeaponState* WeaponState)
{
	if (not IsValid(WeaponState))
	{
		return;
	}
	if (GetModuleContext().State == EVehicleModuleState::Destroyed)
	{
		WeaponState->SetModuleReloadRestriction(this, true);
		return;
	}
	if (GetModuleContext().State == EVehicleModuleState::Damaged)
	{
		Super::ApplyBehaviourToWeapon(WeaponState);
	}
}

void UVehicleModuleAmmoBehaviour::RemoveBehaviourFromWeapon(UWeaponState* WeaponState)
{
	if (not IsValid(WeaponState))
	{
		return;
	}
	WeaponState->SetModuleReloadRestriction(this, false);
	Super::RemoveBehaviourFromWeapon(WeaponState);
}

void UVehicleModuleAmmoBehaviour::OnModuleContextUpdated(const FVehicleModuleBehaviourContext& PreviousContext)
{
	RefreshAppliedWeaponEffects();
	Super::OnModuleContextUpdated(PreviousContext);
}
