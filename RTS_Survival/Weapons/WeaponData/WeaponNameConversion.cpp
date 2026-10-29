// Copyright (c) Bas Blokzijl. All rights reserved.
#include "WeaponNameConversion.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponSystems.h"

EWeaponName Global_GetWeaponNameFromFName(const FName WeaponName)
{
	const UEnum* WeaponEnum = StaticEnum<EWeaponName>();
	const int64 Value = WeaponEnum ? WeaponEnum->GetValueByNameString(WeaponName.ToString()) : INDEX_NONE;
	if (Value == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError("Unknown weapon name: " + WeaponName.ToString()
			+ "\n Falling back to DEFAULT_WEAPON. Check the WeaponName pin of the Setup weapon call.");
		return EWeaponName::DEFAULT_WEAPON;
	}
	return static_cast<EWeaponName>(Value);
}

FName Global_GetWeaponNameAsFName(const EWeaponName WeaponName)
{
	const UEnum* WeaponEnum = StaticEnum<EWeaponName>();
	if (not WeaponEnum)
	{
		return NAME_None;
	}
	return FName(*WeaponEnum->GetNameStringByValue(static_cast<int64>(WeaponName)));
}

TArray<FName> Global_GetWeaponNameOptions()
{
	TArray<FName> Options;
	const UEnum* WeaponEnum = StaticEnum<EWeaponName>();
	if (not WeaponEnum)
	{
		return Options;
	}
	// NumEnums includes the generated _MAX entry, which is not a weapon.
	const int32 LastWeaponIndex = WeaponEnum->ContainsExistingMax() ? WeaponEnum->NumEnums() - 1 : WeaponEnum->NumEnums();
	for (int32 EnumIndex = 0; EnumIndex < LastWeaponIndex; ++EnumIndex)
	{
		const int64 Value = WeaponEnum->GetValueByIndex(EnumIndex);
		// Enum metadata only exists in builds with metadata; the options only feed editor dropdowns.
#if WITH_METADATA
		const bool bIsHidden = WeaponEnum->HasMetaData(TEXT("Hidden"), EnumIndex);
#else
		const bool bIsHidden = false;
#endif
		if (bIsHidden || Value == static_cast<int64>(EWeaponName::DEFAULT_WEAPON))
		{
			continue;
		}
		Options.Add(FName(*WeaponEnum->GetNameStringByIndex(EnumIndex)));
	}
	return Options;
}
