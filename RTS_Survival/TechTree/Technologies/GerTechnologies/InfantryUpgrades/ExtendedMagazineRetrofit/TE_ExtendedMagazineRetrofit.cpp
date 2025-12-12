// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "TE_ExtendedMagazineRetrofit.h"

#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

void UTE_ExtendedMagazineRetrofit::ApplyEffectToWeapon(FWeaponData* ValidWeaponData)
{
	if(WeaponMagazineSizeIncrease.Contains(ValidWeaponData->WeaponName))
	{
		ValidWeaponData->MagCapacity += WeaponMagazineSizeIncrease[ValidWeaponData->WeaponName];
	}
}
