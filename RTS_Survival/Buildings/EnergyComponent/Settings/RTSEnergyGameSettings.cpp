// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "RTSEnergyGameSettings.h"

#include "Materials/MaterialInterface.h"
#include "RTS_Survival/Behaviours/Derived/BehaviourWeapon/LowEnergyBehaviour.h"

URTSEnergyGameSettings::URTSEnergyGameSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("RTSEnergyGameSettings");
}

UMaterialInterface* URTSEnergyGameSettings::GetLowEnergyMaterial() const
{
	if (LowEnergyMaterial.IsNull())
	{
		return nullptr;
	}

	if (M_CachedLowEnergyMaterial == nullptr)
	{
		M_CachedLowEnergyMaterial = LowEnergyMaterial.LoadSynchronous();
	}

	return M_CachedLowEnergyMaterial;
}

TSubclassOf<ULowEnergyBehaviour> URTSEnergyGameSettings::GetLowEnergyBehaviourClass() const
{
	return LowEnergyBehaviourClass;
}
