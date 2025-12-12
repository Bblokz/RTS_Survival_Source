// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "TE_SuppressiveFireTraining.h"

#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

void UTE_SuppressiveFireTraining::ApplyEffectToWeapon(FWeaponData* ValidWeaponData)
{
	ValidWeaponData->BaseCooldown *= M_RateOfFireMlt;
}
