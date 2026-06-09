#pragma once


#include "CoreMinimal.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Game/GameState/UnitDataHelpers/UnitDataHelpers.h"
#include "RTS_Survival/UnitData/BuildingExpansionData.h"


namespace BxpHelpers
{
	inline constexpr int32 RoundToNearestMultipleOfFive(const int32 Value)
	{
		return (Value + 2) / 5 * 5;
	}

namespace BxpHelpers
{
    inline TArray<FUnitAbilityEntry> ArmedBxpAbilities = FAbilityHelpers::ConvertAbilityIdsToEntries({
        EAbilityID::IdAttack, EAbilityID::IdNoAbility, EAbilityID::IdStop, EAbilityID::IdNoAbility,
        EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
        EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility
    });

    inline TArray<FUnitAbilityEntry> NotArmedBxpAbilities = FAbilityHelpers::ConvertAbilityIdsToEntries({
        EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdStop, EAbilityID::IdNoAbility,
        EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
        EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility
    });
}
	namespace GerBxpData
	{

		
		static void InitGerCaptureBunkersBxpData(TMap<EBuildingExpansionType, FBxpData>& InPlayerDataMap)
		{
			
			using namespace BxpHelpers;
			using namespace DeveloperSettings::GameBalance::TrainingTime;
			using namespace DeveloperSettings::GameBalance::UnitCosts;
			using namespace DeveloperSettings::GameBalance::UnitCosts::Bxps;
			using namespace DeveloperSettings::GameBalance::VisionRadii::BxpVision;
			using namespace DeveloperSettings::GameBalance::UnitHealth;
			FBxpData BxpData;
			// ----------------------- 37MM Flak capture bunker -----------------------
			BxpData.ConstructionTime = RoundToNearestMultipleOfFive(T1BxpBuildTime*2);
			BxpData.Health = RoundToNearestMultipleOfFive(T2BxpHealth*2);
			BxpData.VisionRadius = T1BxpVisionRadius;
			BxpData.Cost = FUnitCost({
				{ERTSResourceType::Resource_Radixite,
					Ger37mmCaptureBunker_Radixite},
				{ERTSResourceType::Resource_Metal,
					Ger37mmCaptureBunker_Metal},
			});
			BxpData.EnergySupply = 0;
			BxpData.Abilities = ArmedBxpAbilities;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_Ger37MMCaptureBunker, BxpData);

			
			// ----------------------- turret capture bunker -----------------------
			BxpData.ConstructionTime = RoundToNearestMultipleOfFive(T1BxpBuildTime*2);
			BxpData.Health = RoundToNearestMultipleOfFive(T2BxpHealth*4);
			BxpData.VisionRadius = T1BxpVisionRadius;
			BxpData.Cost = FUnitCost({
				{ERTSResourceType::Resource_Radixite,
					GerTurretCaptureBunker_Radixite},
				{ERTSResourceType::Resource_Metal,
					GerTurretCaptureBunker_Metal},
			});
			BxpData.EnergySupply = 0;
			BxpData.Abilities = ArmedBxpAbilities;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_GerTurretCaptureBunker, BxpData);
		}
			
		
		static void InitRefineryBxpData(TMap<EBuildingExpansionType, FBxpData>& InPlayerDataMap)
		{
			using namespace BxpHelpers;
			using namespace DeveloperSettings::GameBalance::TrainingTime;
			using namespace DeveloperSettings::GameBalance::UnitCosts;
			using namespace DeveloperSettings::GameBalance::UnitCosts::Bxps;
			using namespace DeveloperSettings::GameBalance::VisionRadii::BxpVision;
			using namespace DeveloperSettings::GameBalance::UnitHealth;
			FBxpData BxpData;

			// ----------------------- GER Refinery Radixite converter building expansion -----------------------
			BxpData.ConstructionTime = RoundToNearestMultipleOfFive(15);
			BxpData.Health = RoundToNearestMultipleOfFive(T1BxpHealth*2);
			BxpData.VisionRadius = T1BxpVisionRadius;
			BxpData.Cost = FUnitCost({
				{ERTSResourceType::Resource_Radixite,
					GerRefConverter_Radixite},
				{ERTSResourceType::Resource_Metal,
					GerRefConverter_Metal},
			});
			BxpData.EnergySupply = 10;
			BxpData.Abilities = NotArmedBxpAbilities;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_RefConverter, BxpData);
		}
		
		static void InitGerBarracksBxpData(TMap<EBuildingExpansionType, FBxpData>& InPlayerDataMap)
		{
			
			using namespace BxpHelpers;
			using namespace DeveloperSettings::GameBalance::TrainingTime;
			using namespace DeveloperSettings::GameBalance::UnitCosts;
			using namespace DeveloperSettings::GameBalance::UnitCosts::Bxps;
			using namespace DeveloperSettings::GameBalance::VisionRadii::BxpVision;
			using namespace DeveloperSettings::GameBalance::UnitHealth;
			FBxpData BxpData;
			
			// ----------------------- GER Barracks Radixite Reactor Origin Expansion -----------------------
			BxpData.ConstructionTime = RoundToNearestMultipleOfFive(20);
			BxpData.Health = RoundToNearestMultipleOfFive(T1BxpHealth*2);
			BxpData.VisionRadius = T1BxpVisionRadius;
			BxpData.Cost = FUnitCost({
				{ERTSResourceType::Resource_Radixite,
					GerBarracksRadixReactor_Radixite},
				{ERTSResourceType::Resource_Metal,
					GerBarracksRadixReactor_Metal},
			});
			BxpData.EnergySupply = 8;
			BxpData.Abilities = NotArmedBxpAbilities;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_GerBarracksRadixReactor, BxpData);
			
			// ----------------------- GER Barracks Fuel Cell Origin Expansion -----------------------
			BxpData.ConstructionTime = RoundToNearestMultipleOfFive(20);
			BxpData.Health = RoundToNearestMultipleOfFive(T1BxpHealth*2);
			BxpData.VisionRadius = T1BxpVisionRadius;
			BxpData.Cost = FUnitCost({
				{ERTSResourceType::Resource_Radixite,
					GerBarracksFuelCell_Radixite},
				{ERTSResourceType::Resource_Metal,
					GerBarracksFuelCell_Metal},
			});
			BxpData.EnergySupply = 5;
			BxpData.Abilities = NotArmedBxpAbilities;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_GerBarrackFuelCell, BxpData);
		}
		
		static void InitGerHQBxpData(TMap<EBuildingExpansionType, FBxpData>& InPlayerDataMap)
		{
			using namespace BxpHelpers;
			using namespace DeveloperSettings::GameBalance::TrainingTime;
			using namespace DeveloperSettings::GameBalance::UnitCosts;
			using namespace DeveloperSettings::GameBalance::UnitCosts::Bxps;
			using namespace DeveloperSettings::GameBalance::VisionRadii::BxpVision;
			using namespace DeveloperSettings::GameBalance::UnitHealth;

			FBxpData BxpData;
			// ----------------------- GER HQ Harvester Socket Expansion -----------------------
			BxpData.ConstructionTime = RoundToNearestMultipleOfFive(15);
			BxpData.Health = RoundToNearestMultipleOfFive(T1BxpHealth);
			BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(BxpData.Health);
			BxpData.VisionRadius = T1BxpVisionRadius;
			BxpData.Cost = FUnitCost({
				{ERTSResourceType::Resource_Radixite,
					GerHQHarvester_Radixite},
				{ERTSResourceType::Resource_Metal,
					GerHQHarvester_Metal},
			});
			BxpData.EnergySupply = 0;
			BxpData.Abilities = NotArmedBxpAbilities;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_GerHQHarvester, BxpData);

			
			// ----------------------- GER HQ Radar Socket Expansion -----------------------
			BxpData.ConstructionTime = RoundToNearestMultipleOfFive(20);
			BxpData.Health = RoundToNearestMultipleOfFive(T1BxpHealth);
			BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(BxpData.Health);
			BxpData.VisionRadius = T1BxpVisionRadius;
			BxpData.Cost = FUnitCost({
				{ERTSResourceType::Resource_Radixite,
					GerHQRadar_Radixite},
				{ERTSResourceType::Resource_Metal,
					GerHQRadar_Metal},
			});
			BxpData.EnergySupply = -5;
			BxpData.Abilities = NotArmedBxpAbilities;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_GerHQRadar, BxpData);
			
			// ----------------------- GER HQ Platform Socket Expansion -----------------------
			BxpData.ConstructionTime = RoundToNearestMultipleOfFive(15);
			BxpData.Health = RoundToNearestMultipleOfFive(T1BxpHealth*1.25);
			BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(BxpData.Health);
			BxpData.VisionRadius = T1BxpVisionRadius;
			BxpData.Cost = FUnitCost({
				{ERTSResourceType::Resource_Radixite,
					GerHQPlatform_Radixite},
				{ERTSResourceType::Resource_Metal,
					GerHQPlatform_Metal},
			});
			BxpData.EnergySupply = -5;
			BxpData.Abilities = ArmedBxpAbilities;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_GerHQPlatform, BxpData);

			// ----------------------- GER HQ Tower Origin Expansion -----------------------
			BxpData.ConstructionTime = RoundToNearestMultipleOfFive(T1BxpBuildTime*5);
			BxpData.Health = RoundToNearestMultipleOfFive(15);
			BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIReinforcedArmorResistances(BxpData.Health);
			BxpData.VisionRadius = T1BxpVisionRadius;
			BxpData.Cost = FUnitCost({
				{ERTSResourceType::Resource_Radixite,
					GerHQTower_Radixite},
				{ERTSResourceType::Resource_Metal,
					GerHQTower_Metal},
			});
			BxpData.EnergySupply = -8;
			BxpData.Abilities = ArmedBxpAbilities;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_GerHQTower, BxpData);

			// ----------------------- GER HQ Repair Bay Free Expansion -----------------------
			BxpData.ConstructionTime = RoundToNearestMultipleOfFive(25);
			BxpData.Health = RoundToNearestMultipleOfFive(T1BxpHealth*3);
			BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(BxpData.Health);
			BxpData.VisionRadius = T1BxpVisionRadius;
			BxpData.Cost = FUnitCost({
				{ERTSResourceType::Resource_Radixite,
					GerHQRepairBay_Radixite},
				{ERTSResourceType::Resource_Metal,
					GerHQRepairBay_Metal},
			});
			BxpData.EnergySupply = -5;
			BxpData.Abilities = NotArmedBxpAbilities;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_GerHQRepairBay, BxpData);
		}
	
		static void InitGerLFactoryBxpData(TMap<EBuildingExpansionType, FBxpData>& InPlayerDataMap)
		{
			using namespace BxpHelpers;
			using namespace DeveloperSettings::GameBalance::TrainingTime;
			using namespace DeveloperSettings::GameBalance::UnitCosts;
			using namespace DeveloperSettings::GameBalance::UnitCosts::Bxps;
			using namespace DeveloperSettings::GameBalance::VisionRadii::BxpVision;
			using namespace DeveloperSettings::GameBalance::UnitHealth;

			FBxpData BxpData;
		
			// ----------------------- GER LFactory RadixiteReactor Expansion -----------------------
			BxpData.ConstructionTime = RoundToNearestMultipleOfFive(T1BxpBuildTime*2);
			BxpData.Health = RoundToNearestMultipleOfFive(T1BxpHealth*1.5);
			BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(BxpData.Health);
			BxpData.VisionRadius = 100;
			BxpData.Cost = FUnitCost({
				{ERTSResourceType::Resource_Radixite,
					GerLFactoryRadixiteReactor_Radixite},
				{ERTSResourceType::Resource_Metal,
					GerLFactoryRadixiteReactor_Metal},
			});
			BxpData.EnergySupply = 10;
			BxpData.Abilities = NotArmedBxpAbilities;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_GerLFactoryRadixiteReactor, BxpData);
		
			BxpData.ConstructionTime = RoundToNearestMultipleOfFive(T1BxpBuildTime*2);
			BxpData.Health = RoundToNearestMultipleOfFive(T1BxpHealth*1.5);
			BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(BxpData.Health);
			BxpData.VisionRadius = 100;
			BxpData.Cost = FUnitCost({
				{ERTSResourceType::Resource_Radixite,
					GerLFactoryFuelStorage_Radixite},
				{ERTSResourceType::Resource_Metal,
					GerLFactoryFuelStorage_Metal},
			});
			BxpData.EnergySupply = 5;
			BxpData.Abilities = NotArmedBxpAbilities;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_GerLFactoryFuelStorage, BxpData);
		}
	}
}
