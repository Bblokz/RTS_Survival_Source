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
	// Returns the loaded material directly once it is in memory.
	return LowEnergyMaterial.LoadSynchronous();
}

TSubclassOf<ULowEnergyBehaviour> URTSEnergyGameSettings::GetLowEnergyBehaviourClass() const
{
	return LowEnergyBehaviourClass;
}
