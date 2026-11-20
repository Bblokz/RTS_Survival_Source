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
			EAbilityID::IdNoAbility,
			EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
			EAbilityID::IdAttackGround,
			EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
			EAbilityID::IdNoAbility,
		});

		inline TArray<FUnitAbilityEntry> NotArmedBxpAbilities = FAbilityHelpers::ConvertAbilityIdsToEntries({
			EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
			EAbilityID::IdNoAbility,
			EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
			EAbilityID::IdNoAbility,
			EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility, EAbilityID::IdNoAbility,
			EAbilityID::IdNoAbility,
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
			BxpData.ConstructionTime = RoundToNearestMultipleOfFive(T1BxpBuildTime * 2);
			BxpData.Health = RoundToNearestMultipleOfFive(T2BxpHealth * 2);
			BxpData.VisionRadius = T1BxpVisionRadius;
			BxpData.Cost = FUnitCost({
				{
					ERTSResourceType::Resource_Radixite,
					Ger37mmCaptureBunker_Radixite
				},
				{
					ERTSResourceType::Resource_Metal,
					Ger37mmCaptureBunker_Metal
				},
			});
			BxpData.EnergySupply = 0;
			BxpData.Abilities = ArmedBxpAbilities;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_Ger37MMCaptureBunker, BxpData);


			// ----------------------- turret capture bunker -----------------------
			BxpData.ConstructionTime = RoundToNearestMultipleOfFive(T1BxpBuildTime * 2);
			BxpData.Health = RoundToNearestMultipleOfFive(T2BxpHealth * 4);
			BxpData.VisionRadius = T1BxpVisionRadius;
			BxpData.Cost = FUnitCost({
				{
					ERTSResourceType::Resource_Radixite,
					GerTurretCaptureBunker_Radixite
				},
				{
					ERTSResourceType::Resource_Metal,
					GerTurretCaptureBunker_Metal
				},
			});
			BxpData.EnergySupply = 0;
			BxpData.Abilities = ArmedBxpAbilities;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_GerTurretCaptureBunker, BxpData);
		}


		/**
		 * @brief Sandbagged pits for the team weapons' guns; the gun turns a full circle and its crew hands the
		 * rounds to the loader. The heavier the gun, the dearer and tougher the pit.
		 * @param InPlayerDataMap Player bxp data; the enemy map is copied from it afterwards.
		 */
		static void InitGerGunPitBxpData(TMap<EBuildingExpansionType, FBxpData>& InPlayerDataMap)
		{
			using namespace BxpHelpers;
			using namespace DeveloperSettings::GameBalance::TrainingTime;
			using namespace DeveloperSettings::GameBalance::UnitCosts::Bxps;
			using namespace DeveloperSettings::GameBalance::VisionRadii::BxpVision;
			using namespace DeveloperSettings::GameBalance::UnitHealth;
			struct FGunPitEntry
			{
				EBuildingExpansionType Type;
				int32 Radixite;
				int32 Metal;
				float HealthMlt;
				int32 BuildTime = BxpT2BuildTime;
			};
			const FGunPitEntry Pits[] = {
				{EBuildingExpansionType::BTX_GerPak40Pit, GerPak40Pit_Radixite, GerPak40Pit_Metal, 0.9f},
				{EBuildingExpansionType::BTX_GerLeFH18Pit, GerLeFH18Pit_Radixite, GerLeFH18Pit_Metal, 1.1f},
				{EBuildingExpansionType::BTX_GerSFH18Pit, GerSFH18Pit_Radixite, GerSFH18Pit_Metal, 1.25f},
				{EBuildingExpansionType::BTX_GerFlak37Pit, GerFlak37Pit_Radixite, GerFlak37Pit_Metal, 0.9f},
				// Light emplacements: a strong anti-tank weapon behind little cover.
				{
					EBuildingExpansionType::BTX_GerPuppchenPit, GerPuppchenPit_Radixite, GerPuppchenPit_Metal, 0.55f,
					GerPuppchenPitBuildTime
				},
				{
					EBuildingExpansionType::BTX_GerPanzerschreckPit, GerPanzerschreckPit_Radixite, GerPanzerschreckPit_Metal,
					0.4f, GerPanzerschreckPitBuildTime
				},
				{
					EBuildingExpansionType::BTX_GerTwinMG42Pit, GerTwinMG42Pit_Radixite, GerTwinMG42Pit_Metal,
					0.75f, GerTwinMG42PitBuildTime
				},
				// The Communication Center's two raised MG42 nests: the twin pit's turrets and crew on timber decks.
				{
					EBuildingExpansionType::BTX_GerCommMG42Nests, GerCommMG42Nests_Radixite, GerCommMG42Nests_Metal,
					0.75f, GerCommMG42NestsBuildTime
				},
				// A concrete pillbox with the rail Pak in its roof pit: much tougher than any sandbag pit.
				{
					EBuildingExpansionType::BTX_GerRailPakBunker, GerRailPakBunker_Radixite, GerRailPakBunker_Metal,
					1.8f, GerRailPakBunkerBuildTime
				},
				// A heavy gun behind thick, high bag walls: tougher than the open gun pits.
				{EBuildingExpansionType::BTX_GerFlak88Pit, GerFlak88Pit_Radixite, GerFlak88Pit_Metal, 1.3f},
				// The mortar is protected by a compact square sandbag wall, but remains a light emplacement.
				{EBuildingExpansionType::BTX_GerMortarPit, GerMortarPit_Radixite, GerMortarPit_Metal, 0.8f},
			};
			for (const FGunPitEntry& Pit : Pits)
			{
				FBxpData BxpData;
				BxpData.ConstructionTime = Pit.BuildTime;
				BxpData.VisionRadius = T2BxpVisionRadius;
				BxpData.Health = RoundToNearestMultipleOfFive(BxpHeavyBunkerHealth * Pit.HealthMlt);
				BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIReinforcedArmorResistances(BxpData.Health);
				BxpData.Cost = FUnitCost({
					{ERTSResourceType::Resource_Radixite, Pit.Radixite},
					{ERTSResourceType::Resource_Metal, Pit.Metal},
				});
				BxpData.EnergySupply = 0;
				BxpData.Abilities = ArmedBxpAbilities;
				InPlayerDataMap.Add(Pit.Type, BxpData);
			}
			// The field trench is not an expansion: the menu shows its price per metre, the trench actor charges it
			// for each piece it digs and digs at its own pace.
			FBxpData FieldTrenchData;
			FieldTrenchData.Cost = FUnitCost({{ERTSResourceType::Resource_Radixite, GerFieldTrench_RadixitePerMetre}});
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_GerFieldTrench, FieldTrenchData);
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
			BxpData.Health = RoundToNearestMultipleOfFive(T1BxpHealth * 2);
			BxpData.VisionRadius = T1BxpVisionRadius;
			BxpData.Cost = FUnitCost({
				{
					ERTSResourceType::Resource_Radixite,
					GerRefConverter_Radixite
				},
				{
					ERTSResourceType::Resource_Metal,
					GerRefConverter_Metal
				},
			});
			BxpData.EnergySupply = 10;
			BxpData.Abilities = NotArmedBxpAbilities;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_RefConverter, BxpData);

			// ----------------------- SD Refinery Kaskade distillation origin expansion -----------------------
			// Three distillation columns and a glass receiver bank: more radixite storage on the refinery.
			FBxpData KaskadeData;
			KaskadeData.ConstructionTime = RoundToNearestMultipleOfFive(25);
			KaskadeData.Health = RoundToNearestMultipleOfFive(T1BxpHealth * 2);
			KaskadeData.VisionRadius = T1BxpVisionRadius;
			KaskadeData.Cost = FUnitCost({
				{
					ERTSResourceType::Resource_Radixite,
					GerRefKaskade_Radixite
				},
				{
					ERTSResourceType::Resource_Metal,
					GerRefKaskade_Metal
				},
			});
			KaskadeData.EnergySupply = 0;
			KaskadeData.Abilities = NotArmedBxpAbilities;
			KaskadeData.ExtraResourceStorage = {
				{
					ERTSResourceType::Resource_Radixite,
					DeveloperSettings::GameBalance::Resources::RefKaskadeExtraRadixiteStorage
				}
			};
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_RefKaskade, KaskadeData);

			// ----------------------- SD Refinery flak stand origin expansion -----------------------
			// A 3.7 cm Flak 37 and its two crew on a steel stand over the storage tanks; priced like the Flak 37 pit.
			FBxpData FlakStandData;
			FlakStandData.ConstructionTime = RoundToNearestMultipleOfFive(25);
			FlakStandData.Health = RoundToNearestMultipleOfFive(T1BxpHealth * 2);
			FlakStandData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(
				FlakStandData.Health);
			FlakStandData.VisionRadius = T1BxpVisionRadius;
			FlakStandData.Cost = FUnitCost({
				{
					ERTSResourceType::Resource_Radixite,
					GerRefFlakStand_Radixite
				},
				{
					ERTSResourceType::Resource_Metal,
					GerRefFlakStand_Metal
				},
			});
			FlakStandData.EnergySupply = 0;
			FlakStandData.Abilities = ArmedBxpAbilities;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_RefFlakStand, FlakStandData);
		}

		static void InitGerMetalVaultBxpData(TMap<EBuildingExpansionType, FBxpData>& InPlayerDataMap)
		{
			using namespace BxpHelpers;
			using namespace DeveloperSettings::GameBalance::UnitCosts::Bxps;
			using namespace DeveloperSettings::GameBalance::VisionRadii::BxpVision;
			using namespace DeveloperSettings::GameBalance::UnitHealth;

			// ----------------------- SD Metal Vault smelter origin expansion -----------------------
			// A cupola in the vault's tower that turns metal into vehicle parts (its ResourceConverterComponent holds the
			// rates); priced and powered like the refinery's converter.
			FBxpData SmelterData;
			SmelterData.ConstructionTime = RoundToNearestMultipleOfFive(20);
			SmelterData.Health = RoundToNearestMultipleOfFive(T1BxpHealth * 2);
			SmelterData.VisionRadius = T1BxpVisionRadius;
			SmelterData.Cost = FUnitCost({
				{
					ERTSResourceType::Resource_Radixite,
					GerMetalVaultSmelter_Radixite
				},
				{
					ERTSResourceType::Resource_Metal,
					GerMetalVaultSmelter_Metal
				},
			});
			SmelterData.EnergySupply = 10;
			SmelterData.Abilities = NotArmedBxpAbilities;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_GerMetalVaultSmelter, SmelterData);
		}

		/**
		 * @brief The Strike Division mechanized depot's origin expansions: the radio van (vision) and the Wurfrahmen
		 * shop (Panzerwerfer reload). Each its own struct, so no field leaks from one entry to the next.
		 * @param InPlayerDataMap Player bxp data; the enemy map is copied from it afterwards.
		 */
		static void InitGerMechDepotBxpData(TMap<EBuildingExpansionType, FBxpData>& InPlayerDataMap)
		{
			using namespace BxpHelpers;
			using namespace DeveloperSettings::GameBalance::TrainingTime;
			using namespace DeveloperSettings::GameBalance::UnitCosts::Bxps;
			using namespace DeveloperSettings::GameBalance::VisionRadii::BxpVision;
			using namespace DeveloperSettings::GameBalance::UnitHealth;

			// ----------------------- SD Mechanized Depot radio van origin expansion -----------------------
			// The depot's eyes: a Kfz. 17 radio van and a 12 m aerial tower; no gun, but the widest sight of any expansion.
			FBxpData RadioVanData;
			RadioVanData.ConstructionTime = GerMechDepotRadioVanBuildTime;
			RadioVanData.Health = RoundToNearestMultipleOfFive(T1BxpHealth * 1.5);
			RadioVanData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(
				RadioVanData.Health);
			RadioVanData.VisionRadius = GerMechDepotRadioVanVisionRadius;
			RadioVanData.Cost = FUnitCost({
				{
					ERTSResourceType::Resource_Radixite,
					GerMechDepotRadioVan_Radixite
				},
				{
					ERTSResourceType::Resource_Metal,
					GerMechDepotRadioVan_Metal
				},
			});
			// The radio sets draw power like the cipher room's.
			RadioVanData.EnergySupply = -10;
			RadioVanData.Abilities = NotArmedBxpAbilities;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_GerMechDepotRadioVan, RadioVanData);

			// ----------------------- SD Mechanized Depot Wurfrahmen shop origin expansion -----------------------
			// A lean-to where a 251 gets its rocket racks; while it stands the player's Panzerwerfers reload faster.
			FBxpData WurfrahmenShopData;
			WurfrahmenShopData.ConstructionTime = GerMechDepotWurfrahmenShopBuildTime;
			WurfrahmenShopData.Health = RoundToNearestMultipleOfFive(T1BxpHealth * 2);
			WurfrahmenShopData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(
				WurfrahmenShopData.Health);
			WurfrahmenShopData.VisionRadius = T1BxpVisionRadius;
			WurfrahmenShopData.Cost = FUnitCost({
				{
					ERTSResourceType::Resource_Radixite,
					GerMechDepotWurfrahmenShop_Radixite
				},
				{
					ERTSResourceType::Resource_Metal,
					GerMechDepotWurfrahmenShop_Metal
				},
			});
			WurfrahmenShopData.EnergySupply = -5;
			WurfrahmenShopData.Abilities = NotArmedBxpAbilities;
			WurfrahmenShopData.PanzerwerferReloadMlt = GerMechDepotWurfrahmenShopPanzerwerferReloadMlt;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_GerMechDepotWurfrahmenShop, WurfrahmenShopData);
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
			BxpData.Health = RoundToNearestMultipleOfFive(T1BxpHealth * 2);
			BxpData.VisionRadius = T1BxpVisionRadius;
			BxpData.Cost = FUnitCost({
				{
					ERTSResourceType::Resource_Radixite,
					GerBarracksRadixReactor_Radixite
				},
				{
					ERTSResourceType::Resource_Metal,
					GerBarracksRadixReactor_Metal
				},
			});
			BxpData.EnergySupply = 8;
			BxpData.Abilities = NotArmedBxpAbilities;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_GerBarracksRadixReactor, BxpData);

			// ----------------------- GER Barracks Fuel Cell Origin Expansion -----------------------
			BxpData.ConstructionTime = RoundToNearestMultipleOfFive(20);
			BxpData.Health = RoundToNearestMultipleOfFive(T1BxpHealth * 2);
			BxpData.VisionRadius = T1BxpVisionRadius;
			BxpData.Cost = FUnitCost({
				{
					ERTSResourceType::Resource_Radixite,
					GerBarracksFuelCell_Radixite
				},
				{
					ERTSResourceType::Resource_Metal,
					GerBarracksFuelCell_Metal
				},
			});
			BxpData.EnergySupply = 5;
			BxpData.Abilities = NotArmedBxpAbilities;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_GerBarrackFuelCell, BxpData);

			// ----------------------- GER Barracks Sniper Loft Origin Expansion -----------------------
			// A timber loft with one Kar98k sniper on its deck; a light post, so tougher than the Panzerschreck foxhole
			// but softer than the Schreck frame's steel deck. Own struct: the shared BxpData above carries no new fields.
			FBxpData SniperLoftData;
			SniperLoftData.ConstructionTime = GerBarracksSniperLoftBuildTime;
			SniperLoftData.Health = RoundToNearestMultipleOfFive(T1BxpHealth * 1.5);
			SniperLoftData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(
				SniperLoftData.Health);
			SniperLoftData.VisionRadius = GerBarracksSniperLoftVisionRadius;
			SniperLoftData.Cost = FUnitCost({
				{
					ERTSResourceType::Resource_Radixite,
					GerBarracksSniperLoft_Radixite
				},
				{
					ERTSResourceType::Resource_Metal,
					GerBarracksSniperLoft_Metal
				},
			});
			SniperLoftData.EnergySupply = 0;
			SniperLoftData.Abilities = ArmedBxpAbilities;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_GerBarracksSniperLoft, SniperLoftData);
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
			BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(
				BxpData.Health);
			BxpData.VisionRadius = T1BxpVisionRadius;
			BxpData.Cost = FUnitCost({
				{
					ERTSResourceType::Resource_Radixite,
					GerHQHarvester_Radixite
				},
				{
					ERTSResourceType::Resource_Metal,
					GerHQHarvester_Metal
				},
			});
			BxpData.EnergySupply = 0;
			BxpData.Abilities = NotArmedBxpAbilities;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_GerHQHarvester, BxpData);


			// ----------------------- GER HQ Radar Socket Expansion -----------------------
			BxpData.ConstructionTime = RoundToNearestMultipleOfFive(20);
			BxpData.Health = RoundToNearestMultipleOfFive(T1BxpHealth);
			BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(
				BxpData.Health);
			BxpData.VisionRadius = T1BxpVisionRadius;
			BxpData.Cost = FUnitCost({
				{
					ERTSResourceType::Resource_Radixite,
					GerHQRadar_Radixite
				},
				{
					ERTSResourceType::Resource_Metal,
					GerHQRadar_Metal
				},
			});
			BxpData.EnergySupply = -5;
			BxpData.Abilities = NotArmedBxpAbilities;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_GerHQRadar, BxpData);

			// ----------------------- GER HQ Platform Socket Expansion -----------------------
			BxpData.ConstructionTime = RoundToNearestMultipleOfFive(15);
			BxpData.Health = RoundToNearestMultipleOfFive(T1BxpHealth * 1.25);
			BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(
				BxpData.Health);
			BxpData.VisionRadius = T1BxpVisionRadius;
			BxpData.Cost = FUnitCost({
				{
					ERTSResourceType::Resource_Radixite,
					GerHQPlatform_Radixite
				},
				{
					ERTSResourceType::Resource_Metal,
					GerHQPlatform_Metal
				},
			});
			BxpData.EnergySupply = -5;
			BxpData.Abilities = ArmedBxpAbilities;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_GerHQPlatform, BxpData);

			// ----------------------- GER HQ Tower Origin Expansion -----------------------
			BxpData.ConstructionTime = RoundToNearestMultipleOfFive(T1BxpBuildTime * 5);
			BxpData.Health = RoundToNearestMultipleOfFive(15);
			BxpData.ResistancesAndDamageMlt =
				FUnitResistanceDataHelpers::GetIReinforcedArmorResistances(BxpData.Health);
			BxpData.VisionRadius = T1BxpVisionRadius;
			BxpData.Cost = FUnitCost({
				{
					ERTSResourceType::Resource_Radixite,
					GerHQTower_Radixite
				},
				{
					ERTSResourceType::Resource_Metal,
					GerHQTower_Metal
				},
			});
			BxpData.EnergySupply = -8;
			BxpData.Abilities = ArmedBxpAbilities;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_GerHQTower, BxpData);

			// ----------------------- GER HQ Laser Platform Socket Expansion -----------------------
			// The Panzer V III laser turret on an HQ socket: about three Flak Platforms in cost and power draw.
			BxpData.ConstructionTime = RoundToNearestMultipleOfFive(30);
			BxpData.Health = RoundToNearestMultipleOfFive(T1BxpHealth * 1.5);
			BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(
				BxpData.Health);
			BxpData.VisionRadius = T1BxpVisionRadius;
			BxpData.Cost = FUnitCost({
				{
					ERTSResourceType::Resource_Radixite,
					GerHQLaser_Radixite
				},
				{
					ERTSResourceType::Resource_Metal,
					GerHQLaser_Metal
				},
			});
			BxpData.EnergySupply = -15;
			BxpData.Abilities = ArmedBxpAbilities;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_GerHQLaser, BxpData);

			// ----------------------- GER HQ Repair Bay Free Expansion -----------------------
			BxpData.ConstructionTime = RoundToNearestMultipleOfFive(25);
			BxpData.Health = RoundToNearestMultipleOfFive(T1BxpHealth * 3);
			BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(
				BxpData.Health);
			BxpData.VisionRadius = T1BxpVisionRadius;
			BxpData.Cost = FUnitCost({
				{
					ERTSResourceType::Resource_Radixite,
					GerHQRepairBay_Radixite
				},
				{
					ERTSResourceType::Resource_Metal,
					GerHQRepairBay_Metal
				},
			});
			BxpData.EnergySupply = -5;
			BxpData.Abilities = NotArmedBxpAbilities;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_GerHQRepairBay, BxpData);
		}

		static void InitGerCommCenterBxpData(TMap<EBuildingExpansionType, FBxpData>& InPlayerDataMap)
		{
			using namespace BxpHelpers;
			using namespace DeveloperSettings::GameBalance::TrainingTime;
			using namespace DeveloperSettings::GameBalance::UnitCosts::Bxps;
			using namespace DeveloperSettings::GameBalance::VisionRadii::BxpVision;
			using namespace DeveloperSettings::GameBalance::UnitHealth;
			FBxpData BxpData;

			// ----------------------- GER Communication Center Observation Post Socket Expansion -----------------------
			// A small concrete bunker; its gameplay role (artillery spotting) is still open.
			BxpData.ConstructionTime = GerCommObservationPostBuildTime;
			BxpData.Health = RoundToNearestMultipleOfFive(T1BxpHealth * 1.5);
			BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIReinforcedArmorResistances(
				BxpData.Health);
			BxpData.VisionRadius = T2BxpVisionRadius;
			BxpData.Cost = FUnitCost({
				{
					ERTSResourceType::Resource_Radixite,
					GerCommObservationPost_Radixite
				},
				{
					ERTSResourceType::Resource_Metal,
					GerCommObservationPost_Metal
				},
			});
			BxpData.EnergySupply = -5;
			BxpData.Abilities = NotArmedBxpAbilities;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_GerCommObservationPost, BxpData);

			// ----------------------- GER Communication Center Requisition Office Origin Expansion ----------------------
			// The economic pick: a cabin hung on the tower; while it stands the player's field defenses cost less.
			FBxpData OfficeData;
			OfficeData.ConstructionTime = GerCommRequisitionOfficeBuildTime;
			OfficeData.Health = RoundToNearestMultipleOfFive(T1BxpHealth * 1.5);
			OfficeData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIReinforcedArmorResistances(
				OfficeData.Health);
			OfficeData.VisionRadius = T2BxpVisionRadius;
			OfficeData.Cost = FUnitCost({
				{
					ERTSResourceType::Resource_Radixite,
					GerCommRequisitionOffice_Radixite
				},
				{
					ERTSResourceType::Resource_Metal,
					GerCommRequisitionOffice_Metal
				},
			});
			OfficeData.EnergySupply = -5;
			OfficeData.Abilities = NotArmedBxpAbilities;
			OfficeData.FieldDefenseCostMlt = GerCommRequisitionOfficeFieldDefenseCostMlt;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_GerCommRequisitionOffice, OfficeData);

			// ----------------------- GER Communication Center Cipher Room Origin Expansion -----------------------------
			// The technology pick: a cabin hung over the inner yard; while it stands the player's commander abilities
			// recharge faster.
			FBxpData CipherData;
			CipherData.ConstructionTime = GerCommCipherRoomBuildTime;
			CipherData.Health = RoundToNearestMultipleOfFive(T1BxpHealth * 1.5);
			CipherData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIReinforcedArmorResistances(
				CipherData.Health);
			CipherData.VisionRadius = T2BxpVisionRadius;
			CipherData.Cost = FUnitCost({
				{
					ERTSResourceType::Resource_Radixite,
					GerCommCipherRoom_Radixite
				},
				{
					ERTSResourceType::Resource_Metal,
					GerCommCipherRoom_Metal
				},
			});
			// Radio sets and teleprinters draw more than the office.
			CipherData.EnergySupply = -10;
			CipherData.Abilities = NotArmedBxpAbilities;
			CipherData.GlobalAbilityCooldownMlt = GerCommCipherRoomGlobalAbilityCooldownMlt;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_GerCommCipherRoom, CipherData);
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
			BxpData.ConstructionTime = RoundToNearestMultipleOfFive(T1BxpBuildTime * 2);
			BxpData.Health = RoundToNearestMultipleOfFive(T1BxpHealth * 1.5);
			BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(
				BxpData.Health);
			BxpData.VisionRadius = 100;
			BxpData.Cost = FUnitCost({
				{
					ERTSResourceType::Resource_Radixite,
					GerLFactoryRadixiteReactor_Radixite
				},
				{
					ERTSResourceType::Resource_Metal,
					GerLFactoryRadixiteReactor_Metal
				},
			});
			BxpData.EnergySupply = 10;
			BxpData.Abilities = NotArmedBxpAbilities;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_GerLFactoryRadixiteReactor, BxpData);

			BxpData.ConstructionTime = RoundToNearestMultipleOfFive(T1BxpBuildTime * 2);
			BxpData.Health = RoundToNearestMultipleOfFive(T1BxpHealth * 1.5);
			BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(
				BxpData.Health);
			BxpData.VisionRadius = 100;
			BxpData.Cost = FUnitCost({
				{
					ERTSResourceType::Resource_Radixite,
					GerLFactoryFuelStorage_Radixite
				},
				{
					ERTSResourceType::Resource_Metal,
					GerLFactoryFuelStorage_Metal
				},
			});
			BxpData.EnergySupply = 5;
			BxpData.Abilities = NotArmedBxpAbilities;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_GerLFactoryFuelStorage, BxpData);

			// ----------------------- SD Light Forge Schreck frame origin expansion -----------------------
			// The Panzerschreck pit's team on a steel deck over the forge's front arch: priced like the refinery's flak
			// stand, a far bigger structure than the foxhole.
			FBxpData SchreckFrameData;
			SchreckFrameData.ConstructionTime = RoundToNearestMultipleOfFive(25);
			SchreckFrameData.Health = RoundToNearestMultipleOfFive(T1BxpHealth * 2);
			SchreckFrameData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetIBasicBuildingArmorResistances(
				SchreckFrameData.Health);
			SchreckFrameData.VisionRadius = T1BxpVisionRadius;
			SchreckFrameData.Cost = FUnitCost({
				{
					ERTSResourceType::Resource_Radixite,
					GerForgeSchreckFrame_Radixite
				},
				{
					ERTSResourceType::Resource_Metal,
					GerForgeSchreckFrame_Metal
				},
			});
			SchreckFrameData.EnergySupply = 0;
			SchreckFrameData.Abilities = ArmedBxpAbilities;
			InPlayerDataMap.Add(EBuildingExpansionType::BTX_GerForgeSchreckFrame, SchreckFrameData);
		}
	}

	namespace RusBxpData
	{
		/** @brief Keeps each industrial BXP's balance inputs together for map initialization. */
		struct FRusIndustrialBxpDefinition
		{
			EBuildingExpansionType Type;
			// Medium tank shots that destroy it; see DeveloperSettings::GameBalance::SovietBuildingShotHealth.
			int32 MediumTankShotsToKill;
			float ConstructionTime;
			float VisionRadius;
			EResistancePresetType ResistancePreset;
		};

		inline constexpr float AntennaVisionRadiusMultiplier = 2.f;

		// Health comes from medium tank shots, in line with the other Soviet base buildings.
		inline constexpr FRusIndustrialBxpDefinition IndustrialBxpDefinitions[] =
		{
			{
				EBuildingExpansionType::BTX_RusAquirfierVertical,
				DeveloperSettings::GameBalance::SovietBuildingShotHealth::AquirfierVerticalShots,
				DeveloperSettings::GameBalance::TrainingTime::BxpT2BuildTime,
				DeveloperSettings::GameBalance::VisionRadii::BxpVision::T2BxpVisionRadius,
				EResistancePresetType::ReinforcedBuildingArmor
			},
			{
				EBuildingExpansionType::BTX_RusAquirfier,
				DeveloperSettings::GameBalance::SovietBuildingShotHealth::AquirfierShots,
				DeveloperSettings::GameBalance::TrainingTime::BxpT2BuildTime,
				DeveloperSettings::GameBalance::VisionRadii::BxpVision::T2BxpVisionRadius,
				EResistancePresetType::ReinforcedBuildingArmor
			},
			{
				EBuildingExpansionType::BTX_RusWarehouse,
				DeveloperSettings::GameBalance::SovietBuildingShotHealth::WarehouseShots,
				DeveloperSettings::GameBalance::TrainingTime::T1BxpBuildTime,
				DeveloperSettings::GameBalance::VisionRadii::BxpVision::T1BxpVisionRadius,
				EResistancePresetType::BasicBuildingArmor
			},
			{
				EBuildingExpansionType::BTX_RusChemicalPlant,
				DeveloperSettings::GameBalance::SovietBuildingShotHealth::ChemicalPlantShots,
				DeveloperSettings::GameBalance::TrainingTime::BxpT2BuildTime,
				DeveloperSettings::GameBalance::VisionRadii::BxpVision::T2BxpVisionRadius,
				EResistancePresetType::ReinforcedBuildingArmor
			},
			{
				EBuildingExpansionType::BTX_RusAntenna,
				DeveloperSettings::GameBalance::SovietBuildingShotHealth::AntennaShots,
				DeveloperSettings::GameBalance::TrainingTime::T1BxpBuildTime,
				DeveloperSettings::GameBalance::VisionRadii::BxpVision::T2BxpVisionRadius
				* AntennaVisionRadiusMultiplier,
				EResistancePresetType::BasicBuildingArmor
			}
		};

		static void InitRusIndustrialBxpData(TMap<EBuildingExpansionType, FBxpData>& InPlayerDataMap)
		{
			using namespace BxpHelpers;
			using namespace DeveloperSettings::GameBalance::SovietBuildingShotHealth;

			for (const FRusIndustrialBxpDefinition& Definition : IndustrialBxpDefinitions)
			{
				FBxpData BxpData;
				BxpData.ConstructionTime = Definition.ConstructionTime;
				BxpData.Health = Definition.ResistancePreset == EResistancePresetType::ReinforcedBuildingArmor
					                 ? ReinforcedHealthForShots(Definition.MediumTankShotsToKill)
					                 : BasicHealthForShots(Definition.MediumTankShotsToKill);
				BxpData.VisionRadius = Definition.VisionRadius;
				BxpData.Cost = FUnitCost();
				BxpData.EnergySupply = 0;
				BxpData.Abilities = NotArmedBxpAbilities;
				BxpData.ResistancesAndDamageMlt = FUnitResistanceDataHelpers::GetResistancePresetOfType(
					Definition.ResistancePreset, BxpData.Health);
				InPlayerDataMap.Add(Definition.Type, BxpData);
			}
		}
	}
}
