// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "BehaviourWeaponAutoLoader.h"

namespace BehaviourWeaponAutoLoaderSettings
{
	constexpr float ReloadDurationMultiplierForDoubleReloadRate = 0.5f;
	constexpr int32 MagazineCapacityBonus = 2;
}

UBehaviourWeaponAutoLoader::UBehaviourWeaponAutoLoader()
{
	BehaviourLifeTime = EBehaviourLifeTime::Permanent;
	BehaviourIcon = EBehaviourIcon::TechAutoLoader;
	M_TitleText = "Auto Loader";
	M_DisplayText = "Reload rate increased by 100% and magazine capacity increased by 2.";
	BehaviourWeaponMultipliers.ReloadSpeedMlt =
		BehaviourWeaponAutoLoaderSettings::ReloadDurationMultiplierForDoubleReloadRate;
	BehaviourWeaponAttributes.MagSize = BehaviourWeaponAutoLoaderSettings::MagazineCapacityBonus;
}
