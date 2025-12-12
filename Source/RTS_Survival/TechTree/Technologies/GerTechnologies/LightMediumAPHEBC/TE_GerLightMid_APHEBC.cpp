// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "TE_GerLightMid_APHEBC.h"

#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

void UTE_GerLightMid_APHEBC::ApplyEffectToWeapon(FWeaponData* ValidWeaponData)
{
	int IndexToRemoveAt = -1;
	for(int i = 0; i < ValidWeaponData->ShellTypes.Num(); ++i)
	{
		if(ValidWeaponData->ShellTypes[i] == EWeaponShellType::Shell_APHE)
		{
			IndexToRemoveAt = i;
			break;
		}
	}
	if(IndexToRemoveAt != -1)
	{
		ValidWeaponData->ShellTypes.RemoveAt(IndexToRemoveAt);
		ValidWeaponData->ShellTypes.Add(EWeaponShellType::Shell_APHEBC);
		if(ValidWeaponData->ShellType == EWeaponShellType::Shell_APHE)
		{
			ValidWeaponData->ShellType = EWeaponShellType::Shell_APHEBC;
		}
	}
}
