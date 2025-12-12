// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "TE_WarheadTuning.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

void UTE_WarheadTuning::ApplyEffectToWeapon(FWeaponData* ValidWeaponData)
{
	if (DamageMap.Contains(ValidWeaponData->WeaponName))
	{
		ValidWeaponData->BaseDamage += DamageMap[ValidWeaponData->WeaponName];
	}
	else
	{
		RTSFunctionLibrary::ReportError(
			"Did not find weapon in damage map, weapon name: " + Global_GetWeaponDisplayName(
				ValidWeaponData->WeaponName));
	}
}
