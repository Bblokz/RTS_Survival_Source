// Copyright (C) Bas Blokzijl - All rights reserved.


#include "SquadWeaponIconSettings.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"

USquadWeaponIconSettings::USquadWeaponIconSettings()
{
	
	CategoryName = TEXT("Game");
	SectionName  = TEXT("Squad Weapon Icons");
}

bool USquadWeaponIconSettings::TryGetWeaponIconSettings(const ESquadWeaponIcon WeaponIconType,
                                      FSquadWeaponIconDisplaySettings& OutIconSettings) const
{
        if (WeaponIconType == ESquadWeaponIcon::None)
        {
                // Silently ignore as squad does not use a weapon icon.
                OutIconSettings = FSquadWeaponIconDisplaySettings();
                return true;
        }
        if (not SquadWeaponIconData.TypeToDisplaySettings.Contains(WeaponIconType))
        {
                const FString WeaponIconNameFromUEnum = UEnum::GetValueAsString(WeaponIconType);
                RTSFunctionLibrary::ReportError("The Squad Weapon Icon: " + WeaponIconNameFromUEnum + " does not have display settings assigned in SquadWeaponIconSettings. Please assign them in Project Settings under 'Squad Weapon Icons'.");
                return false;
        }
        OutIconSettings = SquadWeaponIconData.TypeToDisplaySettings[WeaponIconType];
        return true;
}
