#pragma once
#include "RTS_Survival/UnitData/ArmorAndResistanceData.h"

namespace FUnitResistanceDataHelpers
{
	FResistanceAndDamageReductionData GetResistancePresetOfType(const EResistancePresetType PresetType, const float MaxHealth);
	
	// Infantry
	FResistanceAndDamageReductionData GetIBasicInfantryResistances(const float MaxHealth);
	FResistanceAndDamageReductionData GetIHeavyInfantryArmorResistances(const float MaxHealth);
	FResistanceAndDamageReductionData GetCommandoInfantryArmorResistances(const float MaxHealth);
	FResistanceAndDamageReductionData GetIHazmatInfantryResistances(const float MaxHealth);
	FResistanceAndDamageReductionData GetArmoredIHazmatInfantryResistances(const float MaxHealth);
	

	// Vehicles
	FResistanceAndDamageReductionData GetIArmoredCarResistances(const float MaxHealth);
	FResistanceAndDamageReductionData GetILightArmorResistances(const float MaxHealth);
	FResistanceAndDamageReductionData GetIMediumArmorResistances(const float MaxHealth);
	FResistanceAndDamageReductionData GetIHeavyArmorResistances(const float MaxHealth);
	FResistanceAndDamageReductionData GetISuperHeavyArmorResistances(const float MaxHealth);

	// Buildings
	FResistanceAndDamageReductionData GetIBasicBuildingArmorResistances(const float MaxHealth);
	FResistanceAndDamageReductionData GetIReinforcedArmorResistances(const float MaxHealth);

	FResistanceAndDamageReductionData GetUnitResistanceData(const AActor* Unit, bool& bOutFoundValidData);

	
}
