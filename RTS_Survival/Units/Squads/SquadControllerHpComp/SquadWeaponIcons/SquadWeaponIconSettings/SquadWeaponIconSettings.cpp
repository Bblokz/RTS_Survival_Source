// Copyright (C) Bas Blokzijl - All rights reserved.


#include "SquadWeaponIconSettings.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"

USquadWeaponIconSettings::USquadWeaponIconSettings()
{
	
	CategoryName = TEXT("UI");
	SectionName  = TEXT("Squad Weapon Icons");
}

USlateBrushAsset* USquadWeaponIconSettings::GetBrushForWeaponIconType(const ESquadWeaponIcon WeaponIconType) const
{
	if(WeaponIconType == ESquadWeaponIcon::None)
	{
		// Silently ignore as squad does not use a weapon icon.
		return nullptr;
	}
	if(not SquadWeaponIconData.TypeToBrush.Contains(WeaponIconType))
	{
		const FString WeaponIconNameFromUEnum = UEnum::GetValueAsString(WeaponIconType);
		RTSFunctionLibrary::ReportError("The Squad Weapon Icon: " + WeaponIconNameFromUEnum + " does not have a brush assigned in SquadWeaponIconSettings. Please assign one in Project Settings under 'Squad Weapon Icons'.");
		return nullptr;
	}
	return SquadWeaponIconData.TypeToBrush[WeaponIconType];
}
