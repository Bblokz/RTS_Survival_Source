// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_OnHoverAmmoDescription.h"

#include "RTS_Survival/Weapons/WeaponData/WeaponShellType/WeaponShellType.h"

void UW_OnHoverAmmoDescription::ShowAmmoDescription(const EWeaponShellType ShellType)
{
	if (ShellType == EWeaponShellType::Shell_None)
	{
		RTSFunctionLibrary::ReportError(
			TEXT(
				"UW_OnHoverAmmoDescription::ShowAmmoDescription: Tried to show ammo description for Shell_None type."));
		HideAmmoDescription();
		return;
	}
	SetVisibility(ESlateVisibility::Visible);
	BP_SetAmmoDescription(ShellType);
}

void UW_OnHoverAmmoDescription::HideAmmoDescription()
{
	SetVisibility(ESlateVisibility::Hidden);
}
