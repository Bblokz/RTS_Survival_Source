#pragma once


#include "CoreMinimal.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/UnitData/BuildingExpansionData.h"


namespace BxpHelpers
{
	inline constexpr int32 RoundToNearestMultipleOfFive(const int32 Value)
	{
		return (Value + 2) / 5 * 5;
	}

	inline TArray<EAbilityID> ArmedBxpAbilities = {
		EAbilityID::IdAttack, EAbilityID::IdNoAbility, EAbilityID::IdStop, EAbilityID::IdNoAbility,
		EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility
	};
	inline TArray<EAbilityID> NotArmedBxpAbilities = {
		EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdStop, EAbilityID::IdNoAbility,
		EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
		EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility
	};

	namespace GerBxpData
	{

		
		static void InitGerCaptureBunkersBxpData(TMap<EBuildingExpansionType, FBxpData>& InPlayerDataMap)
		{
			
			using namespace BxpHelpers;
			using namespace DeveloperSettings::GameBalance::TrainingTime;
			using namespace DeveloperSettings::GameBalance::UnitCosts;
			using namespace DeveloperSettings::GameBalance::VisionRadii::BxpVision;
			using namespace DeveloperSettings::GameBalance::UnitHealth;
			FBxpData BxpData;
			// ----------------------- 37MM Flak capture bunker -----------------------
			BxpData.ConstructionTime = RoundToNearestMultipleOfFive(T1BxpBuildTime*2);
			BxpData.Health = RoundToNearestMultipleOfFive(T2BxpHealth*2);
			BxpData.VisionRadius = T1BxpVisionRadius;
			BxpData.Cost = FUnitCost({
				{ERTSResourceType::Resource_Radixite,
					RoundToNearestMultipleOfFive( 700* GameCostMlt)},
				{ERTSResourceType::Resource_Metal,
					RoundToNearestMultipleOfFive( 300* GameCostMlt)},
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
					RoundToNearestMultipleOfFive( 700* GameCostMlt)},
				{ERTSResourceType::Resource_Metal,
					RoundToNearestMultipleOfFive( 300* GameCostMlt)},
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
			using namespace DeveloperSettings::GameBalance::VisionRadii::BxpVision;
			using namespace DeveloperSettings::GameBalance::UnitHealth;
			FBxpData BxpData;

			// ----------------------- GER Refinery Radixite converter building expansion -----------------------
			BxpData.ConstructionTime = RoundToNearestMultipleOfFive(T1BxpBuildTime*2);
			BxpData.Health = RoundToNearestMultipleOfFive(T1BxpHealth*2);
			BxpData.VisionRadius = T1BxpVisionRadius;
			BxpData.Cost = FUnitCost({
				{ERTSResourceType::Resource_Radixite,
					RoundToNearestMultipleOfFive( 500* GameCostMlt)},
				{ERTSResourceType::Resource_Metal,
					RoundToNearestMultipleOfFive( 300* GameCostMlt)},
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
			using namespace DeveloperSettings::GameBalance::VisionRadii::BxpVision;
			using namespace DeveloperSettings::GameBalance::UnitHealth;
			FBxpData BxpData;
			
			// ----------------------- GER Barracks Radixite Reactor Origin Expansion -----------------------
			BxpData.ConstructionTime = RoundToNearestMultipleOfFive(T1BxpBuildTime*3);
			BxpData.Health = RoundToNearestMultipleOfFive(T1BxpHealth*2);
			BxpData.VisionRadius = T1BxpVisionRadius;
			BxpData.Cost = FUnitCost({
				{ERTSResourceType::Resource_Radixite,
					RoundToNearestMultipleOfFive( 200* GameCostMlt)},
				{ERTSResourceType::Resource_Metal,
					RoundToNearestMultipleOfFive( 200* GameCostMlt)},
			});
			BxpData.EnergySupply = 8;
			BxpData.Abilities = NotArmedBxpAbilities;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_GerBarracksRadixReactor, BxpData);
			
			// ----------------------- GER Barracks Fuel Cell Origin Expansion -----------------------
			BxpData.ConstructionTime = RoundToNearestMultipleOfFive(T1BxpBuildTime*2);
			BxpData.Health = RoundToNearestMultipleOfFive(T1BxpHealth*2);
			BxpData.VisionRadius = T1BxpVisionRadius;
			BxpData.Cost = FUnitCost({
				{ERTSResourceType::Resource_Radixite,
					RoundToNearestMultipleOfFive( 150* GameCostMlt)},
				{ERTSResourceType::Resource_Metal,
					RoundToNearestMultipleOfFive( 150* GameCostMlt)},
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
			using namespace DeveloperSettings::GameBalance::VisionRadii::BxpVision;
			using namespace DeveloperSettings::GameBalance::UnitHealth;

			FBxpData BxpData;
			// ----------------------- GER HQ Harvester Socket Expansion -----------------------
			BxpData.ConstructionTime = RoundToNearestMultipleOfFive(T1BxpBuildTime*2);
			BxpData.Health = RoundToNearestMultipleOfFive(T1BxpHealth);
			BxpData.VisionRadius = T1BxpVisionRadius;
			BxpData.Cost = FUnitCost({
				{ERTSResourceType::Resource_Radixite,
					RoundToNearestMultipleOfFive( 300* GameCostMlt)},
				{ERTSResourceType::Resource_Metal,
					RoundToNearestMultipleOfFive( 200* GameCostMlt)},
			});
			BxpData.EnergySupply = 0;
			BxpData.Abilities = NotArmedBxpAbilities;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_GerHQHarvester, BxpData);

			
			// ----------------------- GER HQ Radar Socket Expansion -----------------------
			BxpData.ConstructionTime = RoundToNearestMultipleOfFive(T1BxpBuildTime*2);
			BxpData.Health = RoundToNearestMultipleOfFive(T1BxpHealth);
			BxpData.VisionRadius = T1BxpVisionRadius;
			BxpData.Cost = FUnitCost({
				{ERTSResourceType::Resource_Radixite,
					RoundToNearestMultipleOfFive( 200* GameCostMlt)},
				{ERTSResourceType::Resource_Metal,
					RoundToNearestMultipleOfFive( 180* GameCostMlt)},
			});
			BxpData.EnergySupply = 5;
			BxpData.Abilities = NotArmedBxpAbilities;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_GerHQRadar, BxpData);
			
			// ----------------------- GER HQ Platform Socket Expansion -----------------------
			BxpData.ConstructionTime = RoundToNearestMultipleOfFive(T1BxpBuildTime*2);
			BxpData.Health = RoundToNearestMultipleOfFive(T1BxpHealth*1.25);
			BxpData.VisionRadius = T1BxpVisionRadius;
			BxpData.Cost = FUnitCost({
				{ERTSResourceType::Resource_Radixite,
					RoundToNearestMultipleOfFive( 200* GameCostMlt)},
				{ERTSResourceType::Resource_Metal,
					RoundToNearestMultipleOfFive( 100* GameCostMlt)},
			});
			BxpData.EnergySupply = 5;
			BxpData.Abilities = ArmedBxpAbilities;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_GerHQPlatform, BxpData);

			
			// ----------------------- GER HQ Tower Origin Expansion -----------------------
			BxpData.ConstructionTime = RoundToNearestMultipleOfFive(T1BxpBuildTime*5);
			BxpData.Health = RoundToNearestMultipleOfFive(T1BxpHealth*3);
			BxpData.VisionRadius = T1BxpVisionRadius;
			BxpData.Cost = FUnitCost({
				{ERTSResourceType::Resource_Radixite,
					RoundToNearestMultipleOfFive( 400* GameCostMlt)},
				{ERTSResourceType::Resource_Metal,
					RoundToNearestMultipleOfFive( 300* GameCostMlt)},
			});
			BxpData.EnergySupply = 8;
			BxpData.Abilities = ArmedBxpAbilities;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_GerHQTower, BxpData);

			
			// ----------------------- GER HQ Repair Bay Free Expansion -----------------------
			BxpData.ConstructionTime = RoundToNearestMultipleOfFive(T1BxpBuildTime*3);
			BxpData.Health = RoundToNearestMultipleOfFive(T1BxpHealth*3);
			BxpData.VisionRadius = T1BxpVisionRadius;
			BxpData.Cost = FUnitCost({
				{ERTSResourceType::Resource_Radixite,
					RoundToNearestMultipleOfFive( 400* GameCostMlt)},
				{ERTSResourceType::Resource_Metal,
					RoundToNearestMultipleOfFive( 150* GameCostMlt)},
			});
			BxpData.EnergySupply = 5;
			BxpData.Abilities = NotArmedBxpAbilities;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_GerHQRepairBay, BxpData);
		}
	
		static void InitGerLFactoryBxpData(TMap<EBuildingExpansionType, FBxpData>& InPlayerDataMap)
		{
			using namespace BxpHelpers;
			using namespace DeveloperSettings::GameBalance::TrainingTime;
			using namespace DeveloperSettings::GameBalance::UnitCosts;
			using namespace DeveloperSettings::GameBalance::VisionRadii::BxpVision;
			using namespace DeveloperSettings::GameBalance::UnitHealth;

			FBxpData BxpData;
		
			// ----------------------- GER LFactory RadixiteReactor Expansion -----------------------
			BxpData.ConstructionTime = RoundToNearestMultipleOfFive(T1BxpBuildTime*2);
			BxpData.Health = RoundToNearestMultipleOfFive(T1BxpHealth*1.5);
			BxpData.VisionRadius = 100;
			BxpData.Cost = FUnitCost({
				{ERTSResourceType::Resource_Radixite,
					RoundToNearestMultipleOfFive( 300* GameCostMlt)},
				{ERTSResourceType::Resource_Metal,
					RoundToNearestMultipleOfFive( 300* GameCostMlt)},
			});
			BxpData.EnergySupply = 10;
			BxpData.Abilities = NotArmedBxpAbilities;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_GerLFactoryRadixiteReactor, BxpData);
		
			BxpData.ConstructionTime = RoundToNearestMultipleOfFive(T1BxpBuildTime*2);
			BxpData.Health = RoundToNearestMultipleOfFive(T1BxpHealth*1.5);
			BxpData.VisionRadius = 100;
			BxpData.Cost = FUnitCost({
				{ERTSResourceType::Resource_Radixite,
					RoundToNearestMultipleOfFive( 200* GameCostMlt)},
				{ERTSResourceType::Resource_Metal,
					RoundToNearestMultipleOfFive( 200* GameCostMlt)},
			});
			BxpData.EnergySupply = 5;
			BxpData.Abilities = NotArmedBxpAbilities;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_GerLFactoryFuelStorage, BxpData);
		}
	}
}
