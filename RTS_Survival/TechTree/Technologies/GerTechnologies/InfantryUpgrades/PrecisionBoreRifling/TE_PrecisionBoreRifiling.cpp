// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "TE_PrecisionBoreRifiling.h"

#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

void UTE_PrecisionBoreRifiling::ApplyEffectToWeapon(FWeaponData* ValidWeaponData)
{
	ValidWeaponData->Accuracy *= M_AccuracyMlt;
}
