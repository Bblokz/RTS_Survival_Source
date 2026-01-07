// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "TE_AutoCannonAPCR.h"

#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"


void UTE_AutoCannonAPCR::ApplyEffectToWeapon(FWeaponData* ValidWeaponData)
{
	if (!ValidWeaponData)
	{
		return;
	}
	Super::ApplyEffectToWeapon(ValidWeaponData);
	if constexpr (DeveloperSettings::Debugging::GTechTree_Compile_DebugSymbols)
	{
		// RTSFunctionLibrary::PrintString(
		// 	"Upgraded weapon: " +
		// 	Global_GetWeaponDisplayName(ValidWeaponData->WeaponName()) + " with APCR shell (APCR)");
	}
	ValidWeaponData->ShellTypes.Add(EWeaponShellType::Shell_APCR);
}
