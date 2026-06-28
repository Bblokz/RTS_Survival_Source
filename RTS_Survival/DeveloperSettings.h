#pragma once
#include "CoreMinimal.h"
#include "Audio/RTSVoiceLineHelpers/RTS_VoiceLineHelpers.h"
#include "RTS_Survival/RTSComponents/HealthInterface/HealthBarIcons/HealthBarIcons.h"
#include "RTSComponents/ArmorCalculationComponent/Resistances/Resistances.h"
#include "RTSComponents/ArmorComponent/Armor.h"
#include "RTSComponents/DamageReduction/DamageReduction.h"

enum class EArmorPlateDamageType : uint8;

namespace DeveloperSettings
{
	inline constexpr int32 RoundToNearestMultipleOf(const int32 Value, const int32 Multiple)
	{
		return (Value / Multiple) * Multiple;
	}

	inline constexpr int32 RoundToNearestMultipleOfFive(const int32 Value)
	{
		return (Value + 2) / 5 * 5;
	}

	namespace Procedural
	{
		// How often RTSActorProxy will try to get a random seeded class using the RTSRuntimeSeedManager.
		// If the RuntimeSeedManager is still not initialized after this amount of attempts, the RTSActorProxy will fall
		// back to using a normal random stream.
		inline constexpr int32 RTSActorProxyMaxAttemptsGetRandomSeededClass = 10;

		namespace RTSSplineSystems
		{
			// A radius around the borders of a region that we want to sample.
			inline constexpr int32 RegionBorderRadius = 150;

			// The minimal distance the first and last point in a region should be away from eachother for intermediary
			// points in the spline to be allowed to generate.
			inline constexpr float MinDistanceFirstLastPoint = 2500.f;
		}
	}

	namespace Async
	{
		// How often the UGameUnitManager writes data of potential actor targets for weapon systems
		// to the AsyncGetTargetThread.
		inline constexpr float GameThreadUpdateAsyncWithActorTargetsInterval = 1.f;

		// How often the UGameUnitManager writes detailed actor data to the AsyncGetTargetThread.
		inline constexpr float GameThreadUpdateAsyncWithAllDetailedActorDataInterval = 8.67f;

		// How Long the AsyncGetTargetThread waits before handling new incoming target requests from the game thread.
		// Increasing this will cause a longer delay on the thread and hence a longer wait for target request to be processed.
		inline constexpr float AsyncGetTargetThreadUpdateInterval = 0.1f;

		// How often the UGameResourceManager writes Resource DropOff Data to the AsyncGetResourceThread.
		// Decreasing this will keep the async thread more up to date with the game state but
		// increases the load on the game thread.
		inline constexpr float GameThreadUpdateAsyncWithResourceDropOffsInterval = 2.f;
		// How often the UGameResourceManager writes Resource Data to the AsyncGetResourceThread.
		// Decreasing this will keep the async thread more up to date with the game state but
		// increases the load on the game thread.
		inline constexpr float GameThreadUpdateAsyncWithResourcesInterval = 1.25;

		// How long the AsyncGetResourceThread waits before handling new incoming TargetResource and TargetDropOff
		// requests from the game thread. Increasing this will cause a longer delay on the thread and hence a longer wait for
		// target request to be processed.
		inline constexpr float AsyncGetResourceThreadUpdateInterval = 0.25f;
	}

	namespace TargetAcquisition
	{
		inline constexpr float AddedAggroRange = 2500.f;
		inline constexpr float AddedAggroEnemyRange = 3500.f;
	}

	namespace GameBalance
	{
		namespace Resources
		{
			inline constexpr int32 HQBaseRadixiteStorage = 800;
			inline constexpr int32 HQBaseMetalStorage = 350;
			inline constexpr int32 HQBaseVehiclePartsStorage = 250;

			inline constexpr int32 RefineryRadixiteStorage = 1500;
			inline constexpr int32 MetalVaultMetalStorage = 800;
			inline constexpr int32 MetalVaultVehiclePartsStorage = 400;

			inline constexpr float ScavengeTimePer70Parts = 28.f;
			inline constexpr int32 PercentageVehiclePartsFromDestroyedVehicleMin = 40;
			inline constexpr int32 PercentageVehiclePartsFromDestroyedVehicleMax = 70;
			inline const FVector2D VehiclePartsFromDestroyedVehicleMltRange = FVector2D(
				PercentageVehiclePartsFromDestroyedVehicleMin / 100.f,
				PercentageVehiclePartsFromDestroyedVehicleMax / 100.f);

			inline constexpr int32 PercentageResourceFromDestroyedTeamWeaponMin = 40;
			inline constexpr int32 PercentageResourceFromDestroyedTeamWeaponMax = 70;
			inline const FVector2D ResourceFromDestroyedTeamWeaponMltRange = FVector2D(
				PercentageResourceFromDestroyedTeamWeaponMin / 100.f,
				PercentageResourceFromDestroyedTeamWeaponMax / 100.f);

			inline constexpr int32 PercentageResourceFromDestroyedNomadicRadixiteMin = 35;
			inline constexpr int32 PercentageResourceFromDestroyedNomadicRadixiteMax = 60;
			inline const FVector2D ResourceFromDestroyedNomadicRadixiteMltRange = FVector2D(
				PercentageResourceFromDestroyedNomadicRadixiteMin / 100.f,
				PercentageResourceFromDestroyedNomadicRadixiteMax / 100.f);

			inline constexpr int32 PercentageResourceFromDestroyedNomadicMetalMin = 30;
			inline constexpr int32 PercentageResourceFromDestroyedNomadicMetalMax = 55;
			inline const FVector2D ResourceFromDestroyedNomadicMetalMltRange = FVector2D(
				PercentageResourceFromDestroyedNomadicMetalMin / 100.f,
				PercentageResourceFromDestroyedNomadicMetalMax / 100.f);
		}

		namespace DigInWalls
		{
			inline constexpr float DigInWallStartHPMlt = 0.33f;
			inline constexpr float DigInWallUpdateHealthDuringConstructionInterval = 1.f;
		}

		namespace Experience
		{
			inline constexpr float GlobalExpNeededMlt = 1.f;

			inline constexpr int32 BaseInfantryExp = 10;
			inline constexpr int32 ArmoredInfantryExp = 15;
			inline constexpr int32 HazmatInfantryExp = 15;
			inline constexpr int32 ArmoredHazmatInfantryExp = 15;
			inline constexpr int32 T2InfantryExp = 25;
			inline constexpr int32 EliteInfantryExp = 35;
			inline constexpr int32 BaseLightTankExp = 150;
			inline constexpr int32 BaseArmoredCarExp = 100;
			inline constexpr int32 BaseLightMediumTankExp = 250;
			inline constexpr int32 BaseMediumTankExp = 350;
			inline constexpr int32 BaseSpecialMediumTankExp = 450;
			inline constexpr int32 BaseT2HeavyTankExp = 550;
			inline constexpr int32 BaseHeavyTankExp = 700;
			inline constexpr int32 BaseSuperHeavyTankExp = 1100;

			inline constexpr int32 T1NomadicExp = 300;
			inline constexpr int32 T2NomadicExp = 500;
			inline constexpr int32 T3NomadicExp = 800;
			inline constexpr int32 HQNomadicExp = 1200;

			inline constexpr int32 BaseAircraftExp = 500;
		}

		namespace BuildingTime
		{
			inline constexpr float GameBuildingTimeMlt = 1;
			inline constexpr float HQVehicleConversionTime = 3 * GameBuildingTimeMlt;
			inline constexpr float HQBuildingAnimationTime = 7 * GameBuildingTimeMlt;
			inline constexpr float HQPackUpTime = 10 * GameBuildingTimeMlt;
			inline constexpr float T1TruckVehicleConversionTime = 3 * GameBuildingTimeMlt;
			inline constexpr float T1TruckVehiclePackUpTime = 10 * GameBuildingTimeMlt;
			inline constexpr float BarracksBuildingAnimationTime = 6 * GameBuildingTimeMlt;
			inline constexpr float T1ResourceStorageAnimationTime = 6 * GameBuildingTimeMlt;
			inline constexpr float MechanizedDepotAnimationTime = 6 * GameBuildingTimeMlt;
			inline constexpr float LightTankFactoryBuildingAnimationTime = 10 * GameBuildingTimeMlt;

			// Nomadic T2 Building times.
			inline constexpr float T2TruckVehicleConversionTime = 4 * GameBuildingTimeMlt;
			inline constexpr float T2TruckVehiclePackUpTime = 15 * GameBuildingTimeMlt;
			inline constexpr float T2CommunicationCenterBuildingAnimationTime = 20 * GameBuildingTimeMlt;
			inline constexpr float T2MedTankFactoryBuildingAnimationTime = 20 * GameBuildingTimeMlt;
			// Nomadic T3 Building times.
			inline constexpr float T3TruckVehicleConversionTime = 4 * GameBuildingTimeMlt;
			inline constexpr float T3TruckVehiclePackUpTime = 15 * GameBuildingTimeMlt;
			inline constexpr float T3ExperimentalUnitsFactoryBuildingAnimationTime = 25 * GameBuildingTimeMlt;


			// Dig in walls building time settings.
			inline constexpr float TankWallBuildingTimeMlp = 1.f; // GameBuildingTimeMlt;
			inline constexpr float LightTankDigInWallBuildingTime = 8.f * TankWallBuildingTimeMlp;
			inline constexpr float MediumTankDigInWallBuildingTime = 12.f * TankWallBuildingTimeMlp;
			inline constexpr float HeavyTankDigInWallBuildingTime = 16.f * TankWallBuildingTimeMlp;
		}

		namespace BuildingRadii
		{
			inline constexpr float GameRadiusMlt = 1;
			inline constexpr float GameExpansionRadiusMlt = 1.f;

			inline constexpr float HQBuildingRadius = 4500.f * GameRadiusMlt;
			inline constexpr float HQExpansionRadius = 2500 * GameExpansionRadiusMlt;
			inline constexpr float T1BuildingRadius = 2500.f * GameRadiusMlt;
			inline constexpr float T2BuildingRadius = 3000.f * GameRadiusMlt;
			inline constexpr float T1ExpansionRadius = 1400.f * GameExpansionRadiusMlt;
			inline constexpr float T2ExpansionRadius = 2000.f * GameExpansionRadiusMlt;
		}


		namespace TrainingTime
		{
			inline constexpr float GameTimeMlt = 1;
			// Infantry Training Times.
			inline constexpr int32 BasicInfantryTrainingTime = RoundToNearestMultipleOf(
				12.f * GameTimeMlt, 2);
			inline constexpr int32 ArmoredInfantryTrainingTime = RoundToNearestMultipleOf(
				15.f * GameTimeMlt, 2);
			inline constexpr int32 T2EngineersTrainingTime = RoundToNearestMultipleOf(
				15.f * GameTimeMlt, 2);
			inline constexpr int32 T2InfantryTrainingTime = RoundToNearestMultipleOf(
				20.f * GameTimeMlt, 2);
			inline constexpr int32 EliteInfantryTrainingTime = RoundToNearestMultipleOf(
				30.f * GameTimeMlt, 2);

			// Team weapon squad training times.
			inline constexpr int32 MachineGunTeamWeaponTrainingTime = RoundToNearestMultipleOf(
				12.f * GameTimeMlt, 2);
			inline constexpr int32 MediumAntiTankGunTeamWeaponTrainingTime = RoundToNearestMultipleOf(
				15.f * GameTimeMlt, 2);
			inline constexpr int32 HeavyAntiTankGunTeamWeaponTrainingTime = RoundToNearestMultipleOf(
				15.f * GameTimeMlt, 2);
			inline constexpr int32 MediumMortarTeamWeaponTrainingTime = RoundToNearestMultipleOf(
				12.f * GameTimeMlt, 2);
			inline constexpr int32 HeavyMortarTeamWeaponTrainingTime = RoundToNearestMultipleOf(
				15.f * GameTimeMlt, 2);
			inline constexpr int32 MediumFlakTeamWeaponTrainingTime = RoundToNearestMultipleOf(
				20.f * GameTimeMlt, 2);
			inline constexpr int32 HeavyFlakTeamWeaponTrainingTime = RoundToNearestMultipleOf(
				20.f * GameTimeMlt, 2);
			inline constexpr int32 MediumArtilleryTeamWeaponTrainingTime = RoundToNearestMultipleOf(
				25.f * GameTimeMlt, 2);
			inline constexpr int32 HeavyArtilleryTeamWeaponTrainingTime = RoundToNearestMultipleOf(
				25.f * GameTimeMlt, 2);

			// Harvester Training Time.
			inline constexpr int32 HarvesterTrainingTime = RoundToNearestMultipleOf(
				10.f * GameTimeMlt, 2);

			// Tank Training Times.
			inline constexpr int32 LightTankTrainingTime = RoundToNearestMultipleOf(
				15.f * GameTimeMlt, 2);
			inline constexpr int32 LightTankDestroyerTrainingTime = RoundToNearestMultipleOf(
				15.f * GameTimeMlt, 2);
			inline constexpr int32 LightMediumTankTrainingTime = RoundToNearestMultipleOf(
				20.f * GameTimeMlt, 2);
			inline constexpr int32 MediumTankTrainingTime = RoundToNearestMultipleOf(
				25.f * GameTimeMlt, 2);
			inline constexpr int32 MediumTankDestroyerTrainingTime = RoundToNearestMultipleOf(
				25.f * GameTimeMlt, 2);
			inline constexpr int32 T2HeavyTankTrainingTime = RoundToNearestMultipleOf(
				30.f * GameTimeMlt, 2);
			inline constexpr int32 T3HeavyTankTrainingTime = RoundToNearestMultipleOf(
				35.f * GameTimeMlt, 2);
			inline constexpr int32 T3MediumTankTrainingTime = RoundToNearestMultipleOf(
				30.f * GameTimeMlt, 2);
			// Like PZ V_III or V_IV
			inline constexpr int32 T3SpecialMediumTankTrainingTime = RoundToNearestMultipleOf(
				35.f * GameTimeMlt, 2);
			inline constexpr int32 T3MediumTankDestroyerTrainingTime = RoundToNearestMultipleOf(
				35.f * GameTimeMlt, 2);
			inline constexpr int32 SuperHeavyTankTrainingTime = RoundToNearestMultipleOf(
				45.f * GameTimeMlt, 2);

			// Aircraft training time.
			inline constexpr int32 BasicAircaftTrainingTime = RoundToNearestMultipleOf(
				30.f * GameTimeMlt, 2);
			inline constexpr int32 SpecialistAircaftTrainingTime = RoundToNearestMultipleOf(
				45.f * GameTimeMlt, 2);

			// Armored Car Training Times.
			inline constexpr int32 ArmoredCarTrainingTime = RoundToNearestMultipleOf(
				12.f * GameTimeMlt, 2);
			inline constexpr int32 ArmoredCarMediumCalibreTrainingTime = RoundToNearestMultipleOf(
				15.f * GameTimeMlt, 2);

			// Nomadic Training Times.
			inline constexpr int32 NomadicT1PowerPlantTrainingTime = RoundToNearestMultipleOf(
				10.f * GameTimeMlt, 2);
			inline constexpr int32 NomadicT1RefineryTrainingTime = RoundToNearestMultipleOf(
				15.f * GameTimeMlt, 2);
			inline constexpr int32 NomadicT1BarracksTrainingTime = RoundToNearestMultipleOf(
				15.f * GameTimeMlt, 2);
			inline constexpr int32 NomadicT1VehicleFactoryTrainingTime = RoundToNearestMultipleOf(
				18.f * GameTimeMlt, 2);
			inline constexpr int32 NomadicT1MechanizedDepotTrainingTime = RoundToNearestMultipleOf(
				18.f * GameTimeMlt, 2);

			// Nomadic T2
			inline constexpr int32 NomadicT2CommunicationCenterTrainingTime = 22.f * GameTimeMlt;
			inline constexpr int32 NomadicT2ArmoryTrainingTime = 22.f * GameTimeMlt;
			inline constexpr int32 NomadicT2MedFactoryTrainingTime = 22.f * GameTimeMlt;
			inline constexpr int32 NomadicT3ExperimentalUnitsFactoryTrainingTime = 30.f * GameTimeMlt;

			// Bxp Training Times.
			inline constexpr int32 T1BxpBuildTime = RoundToNearestMultipleOf(15.f * GameTimeMlt, 2);
			inline constexpr int32 BxpT2BuildTime = RoundToNearestMultipleOf(25.f * GameTimeMlt, 2);
		}


		namespace UnitCosts
		{
			/**
			 * @brief On balancing costs; metal is twice as rare as radixite, and vehicle parts are three times as rare as radixite. 
			 */
			// ----------------------------------
			// ------ Tank Costs ---------------
			// ----------------------------------
			inline constexpr float GameCostMlt = 1;
			inline constexpr int32 LightTankRadixiteCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(200 * GameCostMlt));
			inline constexpr int32 LightTankVehiclePartsCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(50 * GameCostMlt));
			// todo ammo and fuel costs.

			inline constexpr int32 LightTankDestroyerRadixiteCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(100 * GameCostMlt));
			inline constexpr int32 LightTankDestroyerVehiclePartsCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(30 * GameCostMlt));

			inline constexpr int32 LightMediumTankRadixiteCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(250 * GameCostMlt));
			inline constexpr int32 LightMediumTankVehiclePartsCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(100 * GameCostMlt));

			inline constexpr int32 MediumTankRadixiteCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(400 * GameCostMlt));
			inline constexpr int32 MediumTankVehiclePartsCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(200 * GameCostMlt));

			inline constexpr int32 MediumTankDestroyerRadixiteCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(300 * GameCostMlt));
			inline constexpr int32 MediumTankDestroyerVehiclePartsCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(125 * GameCostMlt));

			inline constexpr int32 T2HeavyTankRadixiteCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(550 * GameCostMlt));
			inline constexpr int32 T2HeavyTankVehiclePartsCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(250 * GameCostMlt));

			inline constexpr int32 T3HeavyTankRadixiteCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(900 * GameCostMlt));
			inline constexpr int32 T3HeavyTankVehiclePartsCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(450 * GameCostMlt));
			inline constexpr int32 T3MediumTankRadixiteCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(600 * GameCostMlt));
			inline constexpr int32 T3MediumTankVehiclePartsCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(350 * GameCostMlt));
			inline constexpr int32 T3MediumTankDestroyerRadixiteCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(500 * GameCostMlt));
			inline constexpr int32 T3MediumTankDestroyerVehiclePartsCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(300 * GameCostMlt));

			inline constexpr int32 SuperHeavyTankRadixiteCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(1500 * GameCostMlt));
			inline constexpr int32 SuperHeavyTankVehiclePartsCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(1000 * GameCostMlt));

			// ----------------------------------
			// ------ Armored Cars costs --------
			// ----------------------------------
			inline constexpr int32 ArmoredCarRadixiteCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(100 * GameCostMlt));
			inline constexpr int32 ArmoredCarVehiclePartsCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(40 * GameCostMlt));

			inline constexpr int32 ArmoredCarMediumCalibreRadixiteCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(180 * GameCostMlt));
			inline constexpr int32 ArmoredCarMediumCalibreVehiclePartsCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(80 * GameCostMlt));

			namespace ArmoredCars
			{
				inline constexpr int32 Puma_Radixite = 180;
				inline constexpr int32 Puma_VehicleParts = 80;
				inline constexpr int32 FlamePuma_Radixite = 180;
				inline constexpr int32 FlamePuma_VehicleParts = 80;
				inline constexpr int32 Panzerwerfer_Radixite = 234;
				inline constexpr int32 Panzerwerfer_VehicleParts = 144;
				inline constexpr int32 Sdkfz232_3_Radixite = 230;
				inline constexpr int32 Sdkfz232_3_VehicleParts = 105;
				inline constexpr int32 Sdkfz250_Radixite = 100;
				inline constexpr int32 Sdkfz250_VehicleParts = 40;
				inline constexpr int32 Sdkfz9_37mm_Radixite = 100;
				inline constexpr int32 Sdkfz9_37mm_VehicleParts = 40;
				inline constexpr int32 Sdkfz251_22_Radixite = 225;
				inline constexpr int32 Sdkfz251_22_VehicleParts = 90;
				inline constexpr int32 Sdkfz251_Mortar_Radixite = 150;
				inline constexpr int32 Sdkfz251_Mortar_VehicleParts = 53;
				inline constexpr int32 Sdkfz251_Transport_Radixite = 130;
				inline constexpr int32 Sdkfz251_Transport_VehicleParts = 40;
				inline constexpr int32 Sdkfz231_Radixite = 100;
				inline constexpr int32 Sdkfz231_VehicleParts = 40;
				inline constexpr int32 Sdkfz251_PzIV_Radixite = 130;
				inline constexpr int32 Sdkfz251_PzIV_VehicleParts = 65;
			}

			namespace LightTanks
			{
				inline constexpr int32 Pz38t_Radixite = 250;
				inline constexpr int32 Pz38t_VehicleParts = 50;
				inline constexpr int32 Pz38t_R_Radixite = 375;
				inline constexpr int32 Pz38t_R_VehicleParts = 90;
				inline constexpr int32 Pz38t_RailGun_Radixite = 200;
				inline constexpr int32 Pz38t_RailGun_VehicleParts = 100;
				inline constexpr int32 PzIIF_Radixite = 200;
				inline constexpr int32 PzIIF_VehicleParts = 50;
				inline constexpr int32 PzIIFlame_Radixite = 200;
				inline constexpr int32 PzIIFlame_VehicleParts = 75;
				inline constexpr int32 PzIHarvester_Radixite = 300;
				inline constexpr int32 Sdkfz140_Radixite = 225;
				inline constexpr int32 Sdkfz140_VehicleParts = 70;
				inline constexpr int32 PzJager_Radixite = 100;
				inline constexpr int32 PzJager_VehicleParts = 30;
				inline constexpr int32 PzJagerLaser_Radixite = 100;
				inline constexpr int32 PzJagerLaser_VehicleParts = 45;
				inline constexpr int32 Marder_Radixite = 180;
				inline constexpr int32 Marder_VehicleParts = 80;
				inline constexpr int32 PzI15cm_Radixite = 125;
				inline constexpr int32 PzI15cm_VehicleParts = 70;
				inline constexpr int32 Hetzer_Radixite = 200;
				inline constexpr int32 Hetzer_VehicleParts = 100;
			}

			namespace MediumTanks
			{
				inline constexpr int32 PzIIIJ_Radixite = 250;
				inline constexpr int32 PzIIIJ_VehicleParts = 100;
				inline constexpr int32 PzIIIAA_Radixite = 325;
				inline constexpr int32 PzIIIAA_VehicleParts = 125;
				inline constexpr int32 PzIIIJCommander_Radixite = 250;
				inline constexpr int32 PzIIIJCommander_VehicleParts = 100;
				inline constexpr int32 PzIIIM_Radixite = 400;
				inline constexpr int32 PzIIIM_VehicleParts = 200;
				inline constexpr int32 PzIIIFlamm_Radixite = 400;
				inline constexpr int32 PzIIIFlamm_VehicleParts = 300;
				inline constexpr int32 PzIVG_Radixite = 400;
				inline constexpr int32 PzIVG_VehicleParts = 200;
				inline constexpr int32 PzIVH_Radixite = 500;
				inline constexpr int32 PzIVH_VehicleParts = 300;
				inline constexpr int32 PzIVRockets_Radixite = 600;
				inline constexpr int32 PzIVRockets_VehicleParts = 350;
				inline constexpr int32 PzIVF1_Radixite = 275;
				inline constexpr int32 PzIVF1_VehicleParts = 150;
				inline constexpr int32 PzIVF1Commander_Radixite = 275;
				inline constexpr int32 PzIVF1Commander_VehicleParts = 150;
				inline constexpr int32 Jaguar_Radixite = 550;
				inline constexpr int32 Jaguar_VehicleParts = 300;
				inline constexpr int32 Brumbar_Radixite = 550;
				inline constexpr int32 Brumbar_VehicleParts = 400;
				inline constexpr int32 Stug_Radixite = 300;
				inline constexpr int32 Stug_VehicleParts = 125;
				inline constexpr int32 E25_Radixite = 450;
				inline constexpr int32 E25_VehicleParts = 200;
			}

			namespace HeavyTanks
			{
				inline constexpr int32 JagdPanther_Radixite = 600;
				inline constexpr int32 JagdPanther_VehicleParts = 300;
				inline constexpr int32 Maus_Radixite = 1500;
				inline constexpr int32 Maus_VehicleParts = 1000;
				inline constexpr int32 E100_Radixite = 1500;
				inline constexpr int32 E100_VehicleParts = 1000;
				inline constexpr int32 PantherG_Radixite = 600;
				inline constexpr int32 PantherG_VehicleParts = 350;
				inline constexpr int32 PanzerV_IV_Radixite = 900;
				inline constexpr int32 PanzerV_IV_VehicleParts = 400;
				inline constexpr int32 PanzerV_III_Radixite = 900;
				inline constexpr int32 PanzerV_III_VehicleParts = 400;
				inline constexpr int32 PantherD_Radixite = 900;
				inline constexpr int32 PantherD_VehicleParts = 400;
				inline constexpr int32 PantherII_Radixite = 900;
				inline constexpr int32 PantherII_VehicleParts = 600;
				inline constexpr int32 Tiger_Radixite = 900;
				inline constexpr int32 Tiger_VehicleParts = 450;
				inline constexpr int32 TigerRail_Radixite = 900;
				inline constexpr int32 TigerRail_VehicleParts = 700;
				inline constexpr int32 SturmTiger_Radixite = 1350;
				inline constexpr int32 SturmTiger_VehicleParts = 600;
				inline constexpr int32 KingTiger_Radixite = 1300;
				inline constexpr int32 KingTiger_VehicleParts = 650;
				inline constexpr int32 JagdTiger_Radixite = 1300;
				inline constexpr int32 JagdTiger_VehicleParts = 850;
				inline constexpr int32 Tiger105_Radixite = 1500;
				inline constexpr int32 Tiger105_VehicleParts = 750;
			}

			namespace Infantry
			{
				inline constexpr int32 Scavengers_Radixite = 250;
				inline constexpr int32 Jager_Radixite = 200;
				inline constexpr int32 SteelFistAssaultSquad_Radixite = 200;
				inline constexpr int32 SteelFistAssaultSquad_Metal = 100;
				inline constexpr int32 Gebirgsjagerin_Radixite = 200;
				inline constexpr int32 Vultures_Radixite = 525;
				inline constexpr int32 Vultures_Metal = 100;
				inline constexpr int32 SniperTeam_Radixite = 650;
				inline constexpr int32 SniperTeam_Metal = 150;
				inline constexpr int32 FeuerSturm_Radixite = 500;
				inline constexpr int32 FeuerSturm_Metal = 200;
				inline constexpr int32 SturmPionieren_Radixite = 400;
				inline constexpr int32 SturmPionieren_Metal = 200;
				inline constexpr int32 PanzerGrenadiere_Radixite = 675;
				inline constexpr int32 PanzerGrenadiere_Metal = 200;
				inline constexpr int32 SturmKommandos_Radixite = 800;
				inline constexpr int32 SturmKommandos_Metal = 200;
				inline constexpr int32 LightBringers_Radixite = 800;
				inline constexpr int32 LightBringers_Metal = 200;
				inline constexpr int32 IronStorm_Radixite = 700;
				inline constexpr int32 IronStorm_Metal = 300;
			}

			namespace TeamWeapons
			{
				inline constexpr int32 LmgSquad_Radixite = 300;
				inline constexpr int32 PaK38_Radixite = 450;
				inline constexpr int32 PaK38_Metal = 180;
				inline constexpr int32 PaK40_Radixite = 450;
				inline constexpr int32 PaK40_Metal = 180;
				inline constexpr int32 GrW42_80mm_Radixite = 425;
				inline constexpr int32 GrW42_80mm_Metal = 140;
				inline constexpr int32 Flak37mm_Radixite = 500;
				inline constexpr int32 Flak37mm_Metal = 220;
				inline constexpr int32 Flak88mm_Radixite = 500;
				inline constexpr int32 Flak88mm_Metal = 220;
				inline constexpr int32 LefH18_Radixite = 600;
				inline constexpr int32 LefH18_Metal = 260;
				inline constexpr int32 Morser210mm_Radixite = 800;
				inline constexpr int32 Morser210mm_Metal = 410;
				inline constexpr int32 SFH18_150mm_Radixite = 600;
				inline constexpr int32 SFH18_150mm_Metal = 260;
				inline constexpr int32 Nebelwerfer_Radixite = 600;
				inline constexpr int32 Nebelwerfer_Metal = 260;
			}

			namespace Aircraft
			{
				inline constexpr int32 Bf109_Radixite = 600;
				inline constexpr int32 Bf109_VehicleParts = 225;
				inline constexpr int32 Ju87_Radixite = 600;
				inline constexpr int32 Ju87_VehicleParts = 250;
				inline constexpr int32 Me410_Radixite = 800;
				inline constexpr int32 Me410_VehicleParts = 300;
				inline constexpr int32 Ju390B_Radixite = 1000;
				inline constexpr int32 Ju390B_VehicleParts = 400;
				inline constexpr int32 PE8_Radixite = 1000;
				inline constexpr int32 PE8_VehicleParts = 400;
				inline constexpr int32 PE2_Radixite = 800;
				inline constexpr int32 PE2_VehicleParts = 300;
				inline constexpr int32 Horten229_Radixite = 700;
				inline constexpr int32 Horten229_VehicleParts = 275;
				inline constexpr int32 Sturmovic_Radixite = 700;
				inline constexpr int32 Sturmovic_VehicleParts = 275;
				inline constexpr int32 Yak_Radixite = 600;
				inline constexpr int32 Yak_VehicleParts = 225;
			}

			// ----------------------------------
			// ------ Infantry Costs ------------
			// ----------------------------------
			namespace InfantryCosts
			{
				inline constexpr float InfantryCostMlt = 1.f;
				// Ger -- Basic infantry
				inline constexpr int32 ScavengersRadixiteCost = RoundToNearestMultipleOfFive(
					static_cast<int32>(250 * GameCostMlt * InfantryCostMlt));
				inline constexpr int32 JagerRadixiteCost = RoundToNearestMultipleOfFive(
					static_cast<int32>(200 * GameCostMlt * InfantryCostMlt));
				inline constexpr int32 BasicSniperRadixiteCost = RoundToNearestMultipleOfFive(
					static_cast<int32>(200 * GameCostMlt * InfantryCostMlt));
				inline constexpr int32 ArmoredInfantryRadixiteCost = RoundToNearestMultipleOfFive(
					static_cast<int32>(325 * GameCostMlt * InfantryCostMlt));
				inline constexpr int32 ArmoredInfantryMetalCost = RoundToNearestMultipleOfFive(
					static_cast<int32>(50 * GameCostMlt * InfantryCostMlt));
				// Rus -- Basic Infantry
				inline constexpr int32 HazmatEngineersRadixiteCost = RoundToNearestMultipleOfFive(
					static_cast<int32>(300 * GameCostMlt * InfantryCostMlt));
				inline constexpr int32 MosinSquadRadixiteCost = RoundToNearestMultipleOfFive(
					static_cast<int32>(200 * GameCostMlt * InfantryCostMlt));
				inline constexpr int32 PTRS_SquadRadixiteCost = RoundToNearestMultipleOfFive(
					static_cast<int32>(300 * GameCostMlt * InfantryCostMlt));
				// Ger -- Armory unlocked infantry
				inline constexpr int32 VulturesFg42RadixiteCost = RoundToNearestMultipleOfFive(
					static_cast<int32>(525 * GameCostMlt * InfantryCostMlt));
				inline constexpr int32 VulturesFg42MetalCost = RoundToNearestMultipleOfFive(
					static_cast<int32>(100 * GameCostMlt * InfantryCostMlt));
				inline constexpr int32 SniperTeamRadixiteCost = RoundToNearestMultipleOfFive(
					static_cast<int32>(650 * GameCostMlt * InfantryCostMlt));
				inline constexpr int32 SniperTeamMetalCost = RoundToNearestMultipleOfFive(
					static_cast<int32>(150 * GameCostMlt * InfantryCostMlt));
				// Ger -- T2 Infantry Costs
				inline constexpr int32 T2FeuerStormRadixiteCosts = RoundToNearestMultipleOfFive(
					static_cast<int32>(500 * GameCostMlt * InfantryCostMlt));
				inline constexpr int32 T2FeuerStormMetalCosts = RoundToNearestMultipleOfFive(
					static_cast<int32>(200 * GameCostMlt * InfantryCostMlt));
				inline constexpr int32 T2SturmPioneerRadixiteCosts = RoundToNearestMultipleOfFive(
					static_cast<int32>(400 * GameCostMlt * InfantryCostMlt));
				inline constexpr int32 T2SturmPioneerMetalCosts = RoundToNearestMultipleOfFive(
					static_cast<int32>(200 * GameCostMlt * InfantryCostMlt));
				// Rus --- T2 Infantry costs
				inline constexpr int32 T2RedHammerRadixiteCosts = RoundToNearestMultipleOfFive(
					static_cast<int32>(375 * GameCostMlt * InfantryCostMlt));
				inline constexpr int32 T2RedHammerMetalCosts = RoundToNearestMultipleOfFive(
					static_cast<int32>(200 * GameCostMlt * InfantryCostMlt));
				inline constexpr int32 T2ToxicGuardRadixiteCosts = RoundToNearestMultipleOfFive(
					static_cast<int32>(450 * GameCostMlt * InfantryCostMlt));
				// Ger -- Elite infantry costs
				inline constexpr int32 PanzerGrenadierRadixiteCosts = RoundToNearestMultipleOfFive(
					static_cast<int32>(675 * GameCostMlt * InfantryCostMlt));
				inline constexpr int32 PanzerGrenadierMetalCosts = RoundToNearestMultipleOfFive(
					static_cast<int32>(200 * GameCostMlt * InfantryCostMlt));
				inline constexpr int32 SturmkommandoRadixiteCost = RoundToNearestMultipleOfFive(
					static_cast<int32>(800 * GameCostMlt * InfantryCostMlt));
				inline constexpr int32 SturmkommandoMetalCost = RoundToNearestMultipleOfFive(
					static_cast<int32>(200 * GameCostMlt * InfantryCostMlt));
				inline constexpr int32 LightBringersRadixiteCosts = RoundToNearestMultipleOfFive(
					static_cast<int32>(800 * GameCostMlt * InfantryCostMlt));
				inline constexpr int32 LightBringersMetalCosts = RoundToNearestMultipleOfFive(
					static_cast<int32>(200 * GameCostMlt * InfantryCostMlt));
				inline constexpr int32 EliteInfantryRadixiteCost = RoundToNearestMultipleOfFive(
					static_cast<int32>(700 * GameCostMlt * InfantryCostMlt));
				inline constexpr int32 EliteInfantryMetalCost = RoundToNearestMultipleOfFive(
					static_cast<int32>(300 * GameCostMlt * InfantryCostMlt));

				inline constexpr int32 GhostOfStalingradRadixiteCost = RoundToNearestMultipleOfFive(
					static_cast<int32>(1000 * GameCostMlt * InfantryCostMlt));

				inline constexpr int32 EliteRusCyborgAtRadixiteCost = RoundToNearestMultipleOfFive(
					static_cast<int32>(1000 * GameCostMlt * InfantryCostMlt));
				inline constexpr int32 EliteRusCyborgAtMetalCost = RoundToNearestMultipleOfFive(
					static_cast<int32>(330 * GameCostMlt * InfantryCostMlt));

				inline constexpr int32 EliteRusCyborgFedrovRadixiteCost = RoundToNearestMultipleOfFive(
					static_cast<int32>(700 * GameCostMlt * InfantryCostMlt));
				inline constexpr int32 EliteRusCyborgFedrovMetalCost = RoundToNearestMultipleOfFive(
					static_cast<int32>(200 * GameCostMlt * InfantryCostMlt));

				inline constexpr int32 CortextOfficerRadixiteCost = RoundToNearestMultipleOfFive(
					static_cast<int32>(1000 * GameCostMlt * InfantryCostMlt));
				inline constexpr int32 CortextOfficerMetalCost = RoundToNearestMultipleOfFive(
					static_cast<int32>(300 * GameCostMlt * InfantryCostMlt));

				// Team weapon squad costs.
				inline constexpr int32 MachineGunTeamWeaponRadixiteCost = RoundToNearestMultipleOfFive(
					static_cast<int32>(300 * GameCostMlt * InfantryCostMlt));
				inline constexpr int32 AntiTankGunTeamWeaponRadixiteCost = RoundToNearestMultipleOfFive(
					static_cast<int32>(450 * GameCostMlt * InfantryCostMlt));
				inline constexpr int32 AntiTankGunTeamWeaponMetalCost = RoundToNearestMultipleOfFive(
					static_cast<int32>(180 * GameCostMlt * InfantryCostMlt));
				inline constexpr int32 MortarTeamWeaponRadixiteCost = RoundToNearestMultipleOfFive(
					static_cast<int32>(425 * GameCostMlt * InfantryCostMlt));
				inline constexpr int32 MortarTeamWeaponMetalCost = RoundToNearestMultipleOfFive(
					static_cast<int32>(140 * GameCostMlt * InfantryCostMlt));
				inline constexpr int32 FlakTeamWeaponRadixiteCost = RoundToNearestMultipleOfFive(
					static_cast<int32>(500 * GameCostMlt * InfantryCostMlt));
				inline constexpr int32 FlakTeamWeaponMetalCost = RoundToNearestMultipleOfFive(
					static_cast<int32>(220 * GameCostMlt * InfantryCostMlt));
				inline constexpr int32 ArtilleryTeamWeaponRadixiteCost = RoundToNearestMultipleOfFive(
					static_cast<int32>(600 * GameCostMlt * InfantryCostMlt));
				inline constexpr int32 ArtilleryTeamWeaponMetalCost = RoundToNearestMultipleOfFive(
					static_cast<int32>(260 * GameCostMlt * InfantryCostMlt));
			}

			namespace EnergySupplies
			{
				inline constexpr int32 HqEnergySupply = 25;
				inline constexpr int32 T1EnergyBuildingSupply = 20;
				inline constexpr int32 T1ResourceStorageBuildingSupply = -10;
				inline constexpr int32 T1BarracksSupply = -15;
				inline constexpr int32 T1MechanizedDepotSupply = -15;
				inline constexpr int32 T1VehicleFactorySupply = -30;
				inline constexpr int32 T2VehicleFactorySupply = -50;
				inline constexpr int32 T2CommunicationCenterSupply = -40;
				inline constexpr int32 T2ArmorySupply = -15;
				inline constexpr int32 T2TrainingCenterSupply = -20;
				inline constexpr int32 T2AirPadSupply = -40;
				inline constexpr int32 T3ExperimentalUnitsFactorySupply = -60;
			}

			// ----------------------------------
			// ------ Nomadic Costs -------------
			// ----------------------------------
			inline constexpr int32 NomadicSolarFarmRadixiteCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(200 * GameCostMlt));
			inline constexpr int32 NomadicSolarFarmMetalCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(100 * GameCostMlt));

			inline constexpr int32 T1StorageBuildingRadixite = RoundToNearestMultipleOfFive(
				static_cast<int32>(250 * GameCostMlt));
			inline constexpr int32 T1StorageBuildingMetal = RoundToNearestMultipleOfFive(
				static_cast<int32>(125 * GameCostMlt));

			inline constexpr int32 NomadicBarracksRadixiteCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(300 * GameCostMlt));
			inline constexpr int32 NomadicBarracksMetalCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(150 * GameCostMlt));

			inline constexpr int32 NomadicMechanizedDepotRadixiteCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(350 * GameCostMlt));
			inline constexpr int32 NomadicMechanizedDepotMetalCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(200 * GameCostMlt));
			inline constexpr int32 NomadicMechanizedDepotVehiclePartsCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(50 * GameCostMlt));

			inline constexpr int32 NomadicT1VehicleFactoryRadixiteCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(500 * GameCostMlt));
			inline constexpr int32 NomadicT1VehicleFactoryMetalCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(250 * GameCostMlt));\

			// --------------- T2 Nomadic Costs ---------------
			inline constexpr int32 NomadicT2CommunicationCenterRadixiteCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(600 * GameCostMlt));
			inline constexpr int32 NomadicT2CommunicationCenterMetalCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(300 * GameCostMlt));
			inline constexpr int32 NomadicT2ArmoryRadixiteCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(350 * GameCostMlt));
			inline constexpr int32 NomadicT2ArmoryMetalCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(250 * GameCostMlt));

			inline constexpr int32 NomadicT2MedFactoryRadixiteCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(600 * GameCostMlt));
			inline constexpr int32 NomadicT2MedFactoryMetalCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(400 * GameCostMlt));

			inline constexpr int32 NomadicT2AirbaseRadixiteCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(800 * GameCostMlt));
			inline constexpr int32 NomadicT2AirbaseMetalCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(500 * GameCostMlt));

			inline constexpr int32 NomadicT3ExperimentalUnitsFactoryRadixiteCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(1000 * GameCostMlt));
			inline constexpr int32 NomadicT3ExperimentalUnitsFactoryMetalCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(500 * GameCostMlt));

			// ----------------------------------
			// ------ BXP Costs -----------------
			// ----------------------------------
			inline constexpr int32 BxpT1DefensiveRadixiteCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(140 * GameCostMlt));
			inline constexpr int32 BxpT1DefensiveMetalCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(80 * GameCostMlt));
			inline constexpr int32 BxpT1UtilityRadixiteCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(80 * GameCostMlt));
			inline constexpr int32 BxpT1UtilityMetalCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(40 * GameCostMlt));
			inline constexpr int32 BxpT2DefensiveRadixiteCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(250 * GameCostMlt));
			inline constexpr int32 BxpT2DefensiveMetalCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(150 * GameCostMlt));
			inline constexpr int32 BxpT2BunkerRadixiteCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(400 * GameCostMlt));
			inline constexpr int32 BxpT2BunkerMetalCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(200 * GameCostMlt));


			inline constexpr int32 HeavyBunkerRadixiteCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(1200 * GameCostMlt));
			inline constexpr int32 HeavyBunkerMetalCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(750 * GameCostMlt));

			namespace Nomadics
			{
				inline constexpr int32 GerHq_Radixite = 100;
				inline constexpr int32 GerHq_VehicleParts = 100;
				inline constexpr int32 GerGammaFacility_Radixite = 200;
				inline constexpr int32 GerGammaFacility_Metal = 100;
				inline constexpr int32 GerRefinery_Radixite = 250;
				inline constexpr int32 GerRefinery_Metal = 125;
				inline constexpr int32 GerMetalVault_Radixite = 250;
				inline constexpr int32 GerMetalVault_Metal = 125;
				inline constexpr int32 GerBarracks_Radixite = 300;
				inline constexpr int32 GerBarracks_Metal = 150;
				inline constexpr int32 GerMechanizedDepot_Radixite = 350;
				inline constexpr int32 GerMechanizedDepot_Metal = 200;
				inline constexpr int32 GerLightSteelForge_Radixite = 500;
				inline constexpr int32 GerLightSteelForge_Metal = 250;
				inline constexpr int32 GerCommunicationCenter_Radixite = 600;
				inline constexpr int32 GerCommunicationCenter_Metal = 300;
				inline constexpr int32 GerArmory_Radixite = 350;
				inline constexpr int32 GerArmory_Metal = 250;
				inline constexpr int32 GerMedTankFactory_Radixite = 600;
				inline constexpr int32 GerMedTankFactory_Metal = 600;
				inline constexpr int32 GerTrainingCentre_Radixite = 0;
				inline constexpr int32 GerTrainingCentre_Metal = 0;
				inline constexpr int32 GerAirbase_Radixite = 800;
				inline constexpr int32 GerAirbase_Metal = 500;
				inline constexpr int32 GerExperimentalUnitsFactory_Radixite = 1000;
				inline constexpr int32 GerExperimentalUnitsFactory_Metal = 500;
			}

			namespace Bxps
			{
				inline constexpr int32 Ger37mmFlak_Radixite = 140;
				inline constexpr int32 Ger37mmFlak_Metal = 80;
				inline constexpr int32 Ger88mmFlak_Radixite = 420;
				inline constexpr int32 Ger88mmFlak_Metal = 320;
				inline constexpr int32 SolarSmall_Radixite = 80;
				inline constexpr int32 SolarSmall_Metal = 40;
				inline constexpr int32 SolarLarge_Radixite = 155;
				inline constexpr int32 SolarLarge_Metal = 90;
				inline constexpr int32 GerPak38_Radixite = 250;
				inline constexpr int32 GerPak38_Metal = 150;
				inline constexpr int32 GerFlakBunker_Radixite = 1600;
				inline constexpr int32 GerFlakBunker_Metal = 1500;
				inline constexpr int32 GerLeFH150mm_Radixite = 450;
				inline constexpr int32 GerLeFH150mm_Metal = 225;
				inline constexpr int32 Ger37mmCaptureBunker_Radixite = 700;
				inline constexpr int32 Ger37mmCaptureBunker_Metal = 300;
				inline constexpr int32 GerTurretCaptureBunker_Radixite = 700;
				inline constexpr int32 GerTurretCaptureBunker_Metal = 300;
				inline constexpr int32 GerRefConverter_Radixite = 200;
				inline constexpr int32 GerRefConverter_Metal = 100;
				inline constexpr int32 GerBarracksRadixReactor_Radixite = 200;
				inline constexpr int32 GerBarracksRadixReactor_Metal = 200;
				inline constexpr int32 GerBarracksFuelCell_Radixite = 150;
				inline constexpr int32 GerBarracksFuelCell_Metal = 150;
				inline constexpr int32 GerHQHarvester_Radixite = 300;
				inline constexpr int32 GerHQHarvester_Metal = 150;
				inline constexpr int32 GerHQRadar_Radixite = 200;
				inline constexpr int32 GerHQRadar_Metal = 180;
				inline constexpr int32 GerHQPlatform_Radixite = 150;
				inline constexpr int32 GerHQPlatform_Metal = 75;
				inline constexpr int32 GerHQTower_Radixite = 200;
				inline constexpr int32 GerHQTower_Metal = 75;
				inline constexpr int32 GerHQRepairBay_Radixite = 300;
				inline constexpr int32 GerHQRepairBay_Metal = 150;
				inline constexpr int32 GerLFactoryRadixiteReactor_Radixite = 300;
				inline constexpr int32 GerLFactoryRadixiteReactor_Metal = 300;
				inline constexpr int32 GerLFactoryFuelStorage_Radixite = 200;
				inline constexpr int32 GerLFactoryFuelStorage_Metal = 200;
				inline constexpr int32 RusFactory_Radixite = 2000;
				inline constexpr int32 RusFactory_Metal = 1000;
				inline constexpr int32 GerMarderStugFactory_Radixite = 2000;
				inline constexpr int32 GerMarderStugFactory_Metal = 1000;
				inline constexpr int32 RusPlatformFactory_Radixite = 3500;
				inline constexpr int32 RusPlatformFactory_Metal = 1750;
				inline constexpr int32 RusResearchCenter_Radixite = 750;
				inline constexpr int32 RusResearchCenter_Metal = 370;
				inline constexpr int32 RusCoolingTowers_Radixite = 750;
				inline constexpr int32 RusCoolingTowers_Metal = 370;
				inline constexpr int32 RusBarracks_Radixite = 210;
				inline constexpr int32 RusBarracks_Metal = 120;
				inline constexpr int32 RusBunkerMG_Radixite = 180;
				inline constexpr int32 RusBunkerMG_Metal = 100;
				inline constexpr int32 RusGuardTower_Radixite = 250;
				inline constexpr int32 RusGuardTower_Metal = 140;
				inline constexpr int32 RusBoforsPosition_Radixite = 280;
				inline constexpr int32 RusBoforsPosition_Metal = 160;
				inline constexpr int32 RusWall_Radixite = 140;
				inline constexpr int32 RusWall_Metal = 160;
				inline constexpr int32 RusAmmoStorage_Radixite = 140;
				inline constexpr int32 RusAmmoStorage_Metal = 80;
				inline constexpr int32 RusFuelStorage1_Radixite = 140;
				inline constexpr int32 RusFuelStorage1_Metal = 80;
				inline constexpr int32 RusFuelStorage2_Radixite = 280;
				inline constexpr int32 RusFuelStorage2_Metal = 160;
				inline constexpr int32 RusBunker05_Radixite = 600;
				inline constexpr int32 RusBunker05_Metal = 400;
				inline constexpr int32 RusBunker05WithTurrets_Radixite = 1000;
				inline constexpr int32 RusBunker05WithTurrets_Metal = 650;
				inline constexpr int32 RusBunker02ZIS_Radixite = 1200;
				inline constexpr int32 RusBunker02ZIS_Metal = 750;
				inline constexpr int32 RusBunker02_204MM_Radixite = 1800;
				inline constexpr int32 RusBunker02_204MM_Metal = 980;
				inline constexpr int32 RusLongCamoBunker_Radixite = 400;
				inline constexpr int32 RusLongCamoBunker_Metal = 220;
				inline constexpr int32 RusDomeBunker_Radixite = 520;
				inline constexpr int32 RusDomeBunker_Metal = 240;
			}

			// ----------------------------------
			// ------ Aircraft Costs-----------------
			// ----------------------------------
			inline constexpr int32 FighterRadixiteCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(600 * GameCostMlt));
			inline constexpr int32 FighterVehiclePartsCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(225 * GameCostMlt));
			inline constexpr int32 AttackAircraftRadixiteCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(800 * GameCostMlt));
			inline constexpr int32 AttackAircraftVehiclePartsCost = RoundToNearestMultipleOfFive(
				static_cast<int32>(300 * GameCostMlt));
		}

		namespace Ranges
		{
			// How much further weapon trace is allowed to fly than its range; flight range = weapon range * this mlt.
			inline constexpr float TraceSimulationRangeMlt = 2.25f;
			inline constexpr float GameRangeMlt = 0.4f;
			inline constexpr float BasicSmallArmsRange = RoundToNearestMultipleOfFive(8000.f * GameRangeMlt);
			inline constexpr float SmallArmsRifleRange = RoundToNearestMultipleOfFive(8400.f * GameRangeMlt);
			inline constexpr float SmallArmsSniperRange = RoundToNearestMultipleOfFive(8700.f * GameRangeMlt);
			inline constexpr float AircraftSmallArmsRange = RoundToNearestMultipleOfFive(8000);

			inline constexpr float LightCannonRange = RoundToNearestMultipleOfFive(8700.f * GameRangeMlt);
			inline constexpr float LightAssaultCannonRange = RoundToNearestMultipleOfFive(9800.f * GameRangeMlt);
			inline constexpr float MediumCannonRange = RoundToNearestMultipleOfFive(9200.f * GameRangeMlt);
			inline constexpr float MediumAssaultCannonRange = RoundToNearestMultipleOfFive(10600.f * GameRangeMlt);
			inline constexpr float HeavyCannonRange = RoundToNearestMultipleOfFive(9200.f * GameRangeMlt);
			inline constexpr float HeavyAssaultCannonRange = RoundToNearestMultipleOfFive(10600.f * GameRangeMlt);

			// Medium artillery range ~105 mm howitzers
			inline constexpr float LightArtilleryRange = RoundToNearestMultipleOfFive(14000.f * GameRangeMlt);
			inline constexpr float MediumArtilleryRange = 17000.f * GameRangeMlt;
			inline constexpr float HeavyArtilleryRange = 19000.f * GameRangeMlt;

			// Static artillery range  /  howitzers
			inline constexpr float HowitzerArtilleryRange = 22000.f * GameRangeMlt;

			// Laser weapons
			inline constexpr float MediumLaserWeaponRange = RoundToNearestMultipleOf(MediumCannonRange * 1.2f, 100);

			// Flame weapons
			inline constexpr float LightFlameRange = RoundToNearestMultipleOf(1800 * GameRangeMlt, 5);
			inline constexpr float MediumFlameRange = RoundToNearestMultipleOf(3000 * GameRangeMlt, 5);
		}

		namespace VisionRadii
		{
			namespace BuildingVision
			{
				inline constexpr float BuildRadiusToVisionRadius = 1.222;
				// Originally the build radius for hq was 4500 for which 5500 is a good vision radius.
				inline constexpr float HQVisionRadius = BuildingRadii::HQBuildingRadius * BuildRadiusToVisionRadius;
				inline constexpr float T1BuildingVisionRadius = 2000.f * BuildRadiusToVisionRadius;
				inline constexpr float T2BuildingVisionRadius = 3000.f * BuildRadiusToVisionRadius;
			}

			namespace BxpVision
			{
				// Like light tanks.
				inline constexpr float T1BxpVisionRadius = 4000.f;
				// like medium tanks.
				inline constexpr float T2BxpVisionRadius = 4500.f;
			}

			namespace UnitVision
			{
				inline constexpr float T1TankVisionRadius = 4000.f;
				inline constexpr float OpenTopVehicleVisionRadius = 4800.f;
				inline constexpr float ArmoredCarVisionRadius = 5500;
				inline constexpr float T2TankVisionRadius = 4500.f;
				inline constexpr float T3TankVisionRadius = 5000.f;
				inline constexpr float SuperHeavyTankVision = 5000.f;
				inline constexpr float TrainVisionRadius = 12000.f;

				inline constexpr float InfantryVisionRadius = 4500.f;
				inline constexpr float MachineGunTeamWeaponVisionRadius = 5000.f;
				inline constexpr float AntiTankGunTeamWeaponVisionRadius = 5800.f;
				inline constexpr float MortarTeamWeaponVisionRadius = 6500.f;
				inline constexpr float FlakTeamWeaponVisionRadius = 6200.f;
				inline constexpr float ArtilleryTeamWeaponVisionRadius = 7000.f;
				inline constexpr float AircraftVisionRadius = 12000.f;
			}
		}

		namespace Repair
		{
			inline constexpr float RepairTickInterval = 0.5;
			inline constexpr float RepairPower4UnitSquadIn10Seconds = 300.f;
			// We assume a 4 unit sq can repair 300 hp in 10 seconds. -> 30/sec = 15/ 4repairticks.
			inline constexpr float HpRepairedPerTick = RepairPower4UnitSquadIn10Seconds / 4.f / 10.f / (1 /
				RepairTickInterval);
		}

		namespace Weapons
		{
			namespace ImpactAOE
			{
				/**
				 * @brief Designer-facing bounds for AoE damage falloff caused by shrapnel particle count.
				 *
				 * The final falloff exponent blends between these values:
				 * - Fewer shrapnel particles use a value closer to Max, causing sharper center-focused damage.
				 * - More shrapnel particles use a value closer to Min, causing flatter damage across the AoE.
				 *
				 * Lower values make edge damage stronger and the AoE feel more even.
				 * Higher values make damage drop off faster toward the edge.
				 * If you only make MaxExponent higher, you make the falloff harsher mostly for low shrapnel particle counts.
				 */
				inline constexpr float AOEMinParticlesScaleDamageFallOffExponent = 0.5;
				inline constexpr float AOEMaxParticlesScaleDamageFallOffExponent = 3;
			}

			namespace FlameWeapons
			{
				// How often the flame weapon does traces for dealing damage; how long one flame duration tick is.
				inline constexpr float FlameIterationTime = 1.f;
				// With how many degrees we multiply the ConeAngleMlt on flame weapons to derive the ConeAngle to set on the niagara
				// flame thrower system
				inline constexpr float FlameConeAngleUnit = 10.f;
			}

			namespace LaserWeapons
			{
				// How often the laser deals damage again and updates end location within one pulse (laser weapons are
				// always single fire and this settings adjusts how many beam updates exist within one shot).
				inline constexpr float LaserIterationTimeInOnePulse = 0.25f;
				// Global multiplier for all laser weapon base damage.
				inline constexpr float LaserWeaponDamageMlt = 1.0f;
				// Used by PZ V/III Strahlkanone 39 laser tank cannon.
				inline constexpr float Strahlkanone39BaseDamage = 20.f;
				// Used by LB 14 handheld laser weapon.
				inline constexpr float LB14BaseDamage = 8.f;
				// Used by Spektr V infantry laser weapon.
				inline constexpr float SpektrVBaseDamage = 8.f;
				// Used by LightStorm light laser MG handheld weapon.
				inline constexpr float LightStormBaseDamage = 8.f;
				// Used by T-34/76L laser cannon.
				inline constexpr float Luch50LBaseDamage = 18.f;
				// Used by SU-85L laser tank destroyer cannon.
				inline constexpr float Luch85LBaseDamage = 24.f;
				// Used by T-44/100L laser tank cannon.
				inline constexpr float Zarya100LBaseDamage = 35.f;
			}

			namespace SmallArmsAccuracy
			{
				inline constexpr int32 HMGAccuracy = 30.f;
				inline constexpr int32 RifleAccuracy = 70.f;
				inline constexpr int32 SniperAccuracy = 100.f;
				inline constexpr int32 SmgAccuracy = 40.f;
			}

			namespace ArmorAndModules
			{
				inline TMap<EArmorPlateDamageType, int32> PlateTypeToHeHeatDamageChance =
				{
					{EArmorPlateDamageType::DamageFront, 25},
					{EArmorPlateDamageType::DamageSides, 33},
					{EArmorPlateDamageType::DamageRear, 50}
				};

				inline TMap<EArmorPlateDamageType, float> PlateTypeToHeHeatDamageMlt =
				{
					{EArmorPlateDamageType::DamageFront, 0.15f},
					{EArmorPlateDamageType::DamageSides, 0.2},
					{EArmorPlateDamageType::DamageRear, 0.25}
				};
				// Multiplied with the real full damage to get the damage when the shell bounces.
				inline constexpr float HeHeatBounceDamageMlt = 0.2;
			}

			inline constexpr float DamagePerMM = 1.2;
			inline constexpr float DamagePerMM_FireProjectileBonus = 0.5f;
			inline constexpr float DamageBonusSmallArmsMlt = 1.25;
			inline constexpr float SniperDamage = 100;

			namespace Mortars
			{
				inline constexpr float MortarArmorPenPerMM = 320.f / 380.f;
			}

			namespace RailGun
			{
				// Flat bonus damage added on top of the base cannon damage which is dmg per mm
				inline constexpr float DamageBonusMlt = 3;
				// Percentage bonus to apply to the base cannon range for rail gun variants.
				inline constexpr float RangeBonusPercentage = 20.f;
				//  range mlt applied only rail gun weapon projectiles to be able to over-pen near max range. 
				inline constexpr float RailGunProjectileRangeMlt = 2;
				// What the projectile speed is multiplied with after Over penetration.
				inline constexpr float OverPenProjectileSpeedMlt = 0.33f;
			}

			inline constexpr float DamagePerTNTEquivalentGrams = 1.8f;
			inline constexpr float DamageFluxPercentage = 10.f;
			inline constexpr float CooldownFluxPercentage = 10.f;
			inline constexpr float BaseTankMGReloadTime = 8.f;
			// These cannons usually have less pen but get more damage for their explosive munitions.
			inline constexpr float DamageBonusFlakAmmoPercentage = 10.f;
			// Multiply calibre of the shell with this to obtain the shrapnel amount.
			inline constexpr float ShrapnelAmountPerMM = 0.5;
			// Divide mm of the gun to obtain shrapnel range.
			inline constexpr float ShrapnelRangePerMM = 2;
			// Dived gun pen by this factor to obtain shrapnel pen.
			inline constexpr float ShrapnelPenPerMM = 0.5;
			// Shrapnel damage per gram of TNT equivalent.
			inline constexpr float ShrapnelDamagePerTNTGram = 0.5;
			// Bonus multiplier for mortar AOE ranges compared to standard shrapnel range.
			inline constexpr float MortarAOEMlt = 3.f;
			// How much a projectile's penetration can differ from the base value in percentages
			inline constexpr float ArmorPenFluxPercentage = 10.f;


			// @ZEROaccuracy how many centimeters may the trace bullet be divereged on every axis?
			inline constexpr float TraceBulletAccuracy = 400.f;
			// @ZEROaccuracy how many centimeters may the trace bullet be divereged on every axis?
			inline constexpr float TraceBulletAircraftAccuracy = 1200.f;
			// @ZEROaccuracy how many centimeters may the trace laser be divereged on every axis?
			inline constexpr float TraceLaserAccuracy = 400.f;

			inline constexpr float SmallArmsCooldownMlt = 2.f;
			inline constexpr float SmallArmsCooldownFluxMlt = 3.f;

			// Multiplied with the base degree accuracy calculation for projectiles, 1 makes it 10 degrees of variation
			// for a weapon with zero accuracy.
			inline constexpr float ProjectileAccuracyMlt = 0.8;
			inline constexpr float ProjectileGravityScale = 0.05f;
			inline constexpr float RocketProjectileGravityScale = 1.f;
			inline constexpr float RocketProjectileAccuracyMlt = 0.8f;
			inline constexpr int32 MortarAccuracy = 10;

			// Influences the projectile accuracy for mortars and artillery this the amount of centimeters in deviation
			// from the targeted position (deviates only in x,y).
			inline constexpr float ArchProjectileAccuracyMlt = 8.f;

			// 100 pen with decay of 2 example:
			// 30 degrees --> 76.5
			// 60 degrees --> 36.8
			// 90 degrees --> 13.5
			// 100 pen with decay of 1.5 example:
			// 30 degrees --> 81.8
			// 60 degrees --> 47.2
			// 90 degrees --> 22.3
			// 100 pen with decay of 1 example:
			// 30 degrees --> 87.4
			// 60 degrees --> 60.7
			// 90 degrees --> 36.8
			// 100 pen with decay of 0.5 example:
			// 30 degrees --> 93.5
			// 60 degrees --> 77.8
			// 90 degrees --> 60.7

			// Used for exponential decay of the armor pen potential of the round given the impact angle.
			// e ^−k×(1−cos(θ))
			inline constexpr float PenetrationExponentialDecayFactor = 0.75f;


			inline constexpr int32 MaxArmorPlatesPerMesh = 16;

			namespace Projectiles
			{
				// Range bonus applied to projectiles to fly beyond the aim radius of the weapon
				inline constexpr float DefaultProjectileRangeMlt = 1.67f;
				// Range bonus applied to projectiles from trace weapons to fly beyond the aim radius of the weapon
				inline constexpr float DefaultTraceRangeMlt = 1.67f;
				// Armor pen of projectile gets divided by this after a bounce.
				inline constexpr float AmorPenBounceDivider = 4.f;
				inline constexpr int MaxBouncesPerProjectile = 3;
				// How much bigger the raw pen value of the projectile must at least be than the raw armor value
				// of the plate to go through regardless of the angle.
				// 3*80 = 240mm pen needed to go through 80mm armor plate.
				inline constexpr float AllowPenRegardlessOfAngleFactor = 3.f;
				// How APCR manipulates the projectile speed.
				inline constexpr float APCR_ProjectileSpeedMlt = 1.4f;
				// How much APCR increases pen.
				inline constexpr float APCR_ArmorPenMlt = 1.5f;
				// How much APCR changes damage.
				inline constexpr float APCR_DamageMlt = 0.7;
				// How the HE shell manipulates the projectile speed.
				inline constexpr float HE_ProjectileSpeedMlt = 0.66;
				// How much HE changes pen.
				inline constexpr float HE_ArmorPenMlt = 0.1f;
				// How much HE changes damage.
				inline constexpr float HE_DamageMlt = 1.6;
				// How much HE changes shrapnel damage.
				inline constexpr float HE_ShrapnelDamageMlt = 1.5;
				// How much HE change shrapnell range.
				inline constexpr float HE_ShrapnelRangeMlt = 1.5;
				inline constexpr float HE_TNTExplosiveGramsMlt = 3;
				// How much HE changes shrapnel pen.
				inline constexpr float HE_ShrapnelPenMlt = 1.5;
				// How much HE changes shrapnel particles amount.
				inline constexpr float HE_ShrapnelParticlesMlt = 1.5;
				// How HEAT shells manipulate the projectile speed.
				inline constexpr float HEAT_ProjectileSpeedMlt = 0.85f;
				// How much Heat changes armor pen, also make sure to set the max range pen to min range pen!
				inline constexpr float HEAT_ArmorPenMlt = 1.2f;
				// How much Radixite changes base damage.
				inline constexpr float Radixite_DamageMlt = 1.2f;
				// How much Radixite reduces armor penetration at max range.
				inline constexpr float Radixite_ArmorPenMaxRangeMlt = 0.67f;
				// Accuracy change in points for Radixite shells.
				inline constexpr int32 Radixite_AccuracyDelta = -10;
				// How much APHEBC changes the distance between min and max range pen (lerp factor)
				inline constexpr float APHEBC_ArmorPenLerpFactor = 0.5f;

				// Minimal callibre needed to stun vehicles with HE or HEAT rounds.
				inline constexpr float StunCalibreThreshold = 100.f;
			}
		}

		namespace UnitHealth
		{
			// To scale all game health calculations.
			inline constexpr float OverallHealthMlt = 0.8f;

			// One light tank shot on avg in hp points.
			inline constexpr float OneLightTankShotHp = RoundToNearestMultipleOfFive(100.f * OverallHealthMlt);
			// one medium tank shot on avg in hp points.
			inline constexpr float OneMediumTankShotHp = RoundToNearestMultipleOfFive(225.f * OverallHealthMlt);
			// one heavy tank shot on avg in hp points.
			inline constexpr float OneHeavyTankShotHp = RoundToNearestMultipleOfFive(350.f * OverallHealthMlt);


			// T1 light calibre 37mm-50mm deals 80~105 damage. base armored vehicle gets killed in 3-4 shots.
			inline constexpr float ArmoredCarHealthBase = RoundToNearestMultipleOfFive(450.f * OverallHealthMlt);
			// T1 Light calibre 37mm~50mm deals 80~105 damage. Base light tank gets killed in 5 shots. 
			inline constexpr float LightTankHealthBase = RoundToNearestMultipleOfFive(450.f * OverallHealthMlt);
			// T1.5 Between a light and medium tank, takes 6 light shots or 3 mediums shots on avg.
			inline constexpr float LightMediumTankBase = RoundToNearestMultipleOfFive(600.f * OverallHealthMlt);
			inline constexpr float CommandVehicleHp = RoundToNearestMultipleOfFive(1100.f * OverallHealthMlt);
			// T2 medium calibre 75mm~85mm deals 220~260 damage. Base medium tank gets killed in 4 shots.
			inline constexpr float MediumTankHealthBase = RoundToNearestMultipleOfFive(900.f * OverallHealthMlt);
			// T2 heavy tank. A mid game heavy, not as durable as a T3 heavy tank, gets killed in 6 medium tank shots.
			inline constexpr float T2HeavyTankBase = RoundToNearestMultipleOfFive(1300.f * OverallHealthMlt);
			// T3 medium tank, a late game medium tank, gets killed in 6 medium tank shots.
			inline constexpr float T3MediumTankBase = RoundToNearestMultipleOfFive(1300.f * OverallHealthMlt);
			// T3 Heavy tank, very durable vehicle, gets killed in 6 heavy tank shots or 9-10 medium tank shots. 6*350=2100
			inline constexpr float T3HeavyTankBase = RoundToNearestMultipleOfFive(2000.f * OverallHealthMlt);
			// Super heavy tank, the most durable vehicle in the game, gets killed in 8 heavy tank shots.
			inline constexpr float SuperHeavyTankBase = RoundToNearestMultipleOfFive(2800.f * OverallHealthMlt);
			inline constexpr float LightTrainHealth = RoundToNearestMultipleOfFive(4000.f * OverallHealthMlt);
			inline constexpr float MediumTrainHealth = RoundToNearestMultipleOfFive(7000.f * OverallHealthMlt);
			inline constexpr float HeavyTrainHealth = RoundToNearestMultipleOfFive(10000.f * OverallHealthMlt);

			// How much Health on the destroyed version of the tank.
			inline constexpr float DestroyedTankHealthFraction = 0.8;
			// How much the destroyed tank health can differ between instances.
			inline constexpr float DestroyedTankHealthFlux = 0.1;

			// Nomadic Trucks Health settings.

			// The most durable nomadic vehicle.
			inline constexpr float NomadicHQTruckHealth = RoundToNearestMultipleOfFive(7000 * OverallHealthMlt);
			// More health in base mode; takes 10 heavy tank shots to destroy.
			inline constexpr float NomadicHQBuildingHealth = RoundToNearestMultipleOfFive(12000 * OverallHealthMlt);
			inline constexpr float T1NomadicTruckHealth = RoundToNearestMultipleOfFive(1600 * OverallHealthMlt);
			inline constexpr float T1NomadicBuildingHealth = RoundToNearestMultipleOfFive(5000 * OverallHealthMlt);
			inline constexpr float T2NomadicTruckHealth = RoundToNearestMultipleOfFive(4000 * OverallHealthMlt);
			inline constexpr float T2NomadicBuildingHealth = RoundToNearestMultipleOfFive(9000 * OverallHealthMlt);
			inline constexpr float T3NomadicTruckHealth = RoundToNearestMultipleOfFive(5000 * OverallHealthMlt);
			inline constexpr float T3NomadicBuildingHealth = RoundToNearestMultipleOfFive(10000 * OverallHealthMlt);

			// Building expansions health settings.
			inline constexpr float T1BxpHealth = RoundToNearestMultipleOfFive(1000 * OverallHealthMlt);
			inline constexpr float T2BxpHealth = RoundToNearestMultipleOfFive(1300 * OverallHealthMlt);
			inline constexpr float BxpWallHealth = RoundToNearestMultipleOfFive(1500 * OverallHealthMlt);
			inline constexpr float BxpGateHealth = RoundToNearestMultipleOfFive(2000 * OverallHealthMlt);
			inline constexpr float T2BxpBunkerHealth = RoundToNearestMultipleOfFive(1000 * OverallHealthMlt);
			inline constexpr float T3BxpBunkerHealth = RoundToNearestMultipleOfFive(1600 * OverallHealthMlt);
			inline constexpr float BxpHeavyBunkerHealth = RoundToNearestMultipleOfFive(2400 * OverallHealthMlt);

			// Infantry health Settings
			inline constexpr float BasicInfantryHealth = RoundToNearestMultipleOfFive(200.f * OverallHealthMlt);
			inline constexpr float BasicSniperHealth = RoundToNearestMultipleOfFive(300.f * OverallHealthMlt);
			inline constexpr float ArmoredInfantryHealth = RoundToNearestMultipleOfFive(300.f * OverallHealthMlt);
			inline constexpr float HazmatInfantryHealth = RoundToNearestMultipleOfFive(300.f * OverallHealthMlt);
			inline constexpr float ArmoredHazmatInfantryHealth = RoundToNearestMultipleOfFive(400.f * OverallHealthMlt);
			inline constexpr float T2InfantryHealth = RoundToNearestMultipleOfFive(400.f * OverallHealthMlt);
			inline constexpr float EliteInfantryHealth = RoundToNearestMultipleOfFive(500.f * OverallHealthMlt);
			inline constexpr float CommandoInfantryHealth = RoundToNearestMultipleOfFive(1000.f * OverallHealthMlt);

			// Dig in walls health settings.
			inline constexpr float LightTankDigInWallHealth = RoundToNearestMultipleOfFive(400.f * OverallHealthMlt);
			inline constexpr float MediumTankDigInWallHealth = RoundToNearestMultipleOfFive(700.f * OverallHealthMlt);
			inline constexpr float HeavyTankDigInWallHealth = RoundToNearestMultipleOfFive(1200.f * OverallHealthMlt);

			// Aircraft Health settings.
			inline constexpr float FighterHealth = RoundToNearestMultipleOfFive(800.f * OverallHealthMlt);
			inline constexpr float AttackAircraftHealth = RoundToNearestMultipleOfFive(800.f * OverallHealthMlt);
			inline constexpr float HeavyBomberHealth = 1500.f;
		}

		namespace UnitResistances
		{
			namespace BasicInfantry
			{
				// Takes 10% laser damage 
				// Radiation damage is 100%
				inline constexpr FLaserRadiationDamageMlt LaserRadiationMlt{
					FDamageMltPerSide::Uniform(0.1f),
					FDamageMltPerSide::Uniform(1.f)
				};
				inline constexpr float FireThresholdMaxHPMlt = 0.1f;
				inline constexpr float FireRecoveryPerSec = 5.f;
				inline constexpr ETargetTypeIcon TargetTypeIcon = ETargetTypeIcon::BasicInfantryArmor;
				// This damage reduction struct applies to all types of damage
				inline constexpr FDamageReductionSettings DamageReduction(0, 1.f);
			}


			namespace HeavyInfantryArmor
			{
				// Takes 15% laser damage 
				// Radiation damage is 70%
				inline constexpr FLaserRadiationDamageMlt LaserRadiationMlt{
					FDamageMltPerSide::Uniform(0.15f),
					FDamageMltPerSide::Uniform(0.7f)
				};
				inline constexpr float FireThresholdMaxHPMlt = 0.1f;
				inline constexpr float FireRecoveryPerSec = 5.f;
				inline constexpr ETargetTypeIcon TargetTypeIcon = ETargetTypeIcon::HeavyInfantryArmor;
				// This damage reduction struct applies to all types of damage
				inline constexpr FDamageReductionSettings DamageReduction(5, 0.9f);
			}

			namespace CommandoInfantryArmor
			{
				// Takes 15% laser damage 
				// Radiation damage is 50%
				inline constexpr FLaserRadiationDamageMlt LaserRadiationMlt{
					FDamageMltPerSide::Uniform(0.15f),
					FDamageMltPerSide::Uniform(0.5f)
				};
				inline constexpr float FireThresholdMaxHPMlt = 0.25f;
				inline constexpr float FireRecoveryPerSec = 15.f;
				inline constexpr ETargetTypeIcon TargetTypeIcon = ETargetTypeIcon::CommandoInfantryArmor;
				// This damage reduction struct applies to all types of damage
				inline constexpr FDamageReductionSettings DamageReduction(5, 0.66f);
			}

			namespace HazmatInfantry
			{
				// Takes 10% laser Damage
				// Radiation damage is 0%
				inline constexpr FLaserRadiationDamageMlt LaserRadiationMlt{
					FDamageMltPerSide::Uniform(0.1f),
					FDamageMltPerSide::Uniform(0.f)
				};
				inline constexpr float FireThresholdMaxHPMlt = 0.1f;
				inline constexpr float FireRecoveryPerSec = 5.f;
				inline constexpr ETargetTypeIcon TargetTypeIcon = ETargetTypeIcon::HazmatInfantryArmor;
				// This damage reduction struct applies to all types of damage
				inline constexpr FDamageReductionSettings DamageReduction(2, 1.f);
			}

			namespace ArmoredHazmatInfantry
			{
				// Takes 10% laser Damage
				// Radiation damage is 0%
				inline constexpr FLaserRadiationDamageMlt LaserRadiationMlt{
					FDamageMltPerSide::Uniform(0.1f),
					FDamageMltPerSide::Uniform(0.f)
				};
				inline constexpr float FireThresholdMaxHPMlt = 0.1f;
				inline constexpr float FireRecoveryPerSec = 5.f;
				inline constexpr ETargetTypeIcon TargetTypeIcon = ETargetTypeIcon::HazmatInfantryArmor;
				// This damage reduction struct applies to all types of damage
				inline constexpr FDamageReductionSettings DamageReduction(4, 0.9f);
			}

			namespace ArmoredCar
			{
				// Takes 6% laser damage on front and 10% on sides and 15% on rear.
				// Radiation damage is 45% front, 55% sides and 60% rear.
				inline constexpr FLaserRadiationDamageMlt LaserRadiationMlt{
					FDamageMltPerSide{0.06f, 0.1f, 0.15f},
					FDamageMltPerSide{0.45f, 0.55f, 0.6f}
				};
				inline constexpr float FireThresholdMaxHPMlt = 0.1f;
				inline constexpr float FireRecoveryPerSec = 15.f;
				inline constexpr ETargetTypeIcon TargetTypeIcon = ETargetTypeIcon::ArmoredCarArmor;
				// This damage reduction struct applies to all types of damage
				inline constexpr FDamageReductionSettings DamageReduction(0, 1.f);
			}

			namespace LightArmor
			{
				// Takes 20% laser damage on front and 25% on sides and 30% on rear.
				// Radiation damage is 30% front, 40% sides and 45% rear.
				inline constexpr FLaserRadiationDamageMlt LaserRadiationMlt{
					FDamageMltPerSide{0.2f, 0.25, 0.3f},
					FDamageMltPerSide{0.3f, 0.40f, 0.45f}
				};
				inline constexpr float FireThresholdMaxHPMlt = 0.15f;
				inline constexpr float FireRecoveryPerSec = 20.f;
				inline constexpr ETargetTypeIcon TargetTypeIcon = ETargetTypeIcon::LightArmor;
				// This damage reduction struct applies to all types of damage
				inline constexpr FDamageReductionSettings DamageReduction(0, 1.f);
			}

			namespace MediumArmor
			{
				// Takes 60% laser damage on front and 65% on sides and 70% on rear.
				// Radiation damage is 20% front, 30% sides and 35% rear.
				inline constexpr FLaserRadiationDamageMlt LaserRadiationMlt{
					FDamageMltPerSide{0.6f, 0.65f, 0.7f},
					FDamageMltPerSide{0.2f, 0.3f, 0.35f}
				};
				inline constexpr float FireThresholdMaxHPMlt = 0.25;
				inline constexpr float FireRecoveryPerSec = 25.f;
				inline constexpr ETargetTypeIcon TargetTypeIcon = ETargetTypeIcon::MediumArmor;
				// This damage reduction struct applies to all types of damage
				inline constexpr FDamageReductionSettings DamageReduction(0, 1.f);
			}

			namespace HeavyArmor
			{
				// Takes 90% laser damage on front and 95% on sides and 100% on rear.
				// Radiation damage is 5% front, 8% sides and 12% rear.
				inline constexpr FLaserRadiationDamageMlt LaserRadiationMlt{
					FDamageMltPerSide{0.9f, 0.95f, 1.f},
					FDamageMltPerSide{0.05f, 0.08f, 0.12f}
				};
				inline constexpr float FireThresholdMaxHPMlt = 0.30;
				inline constexpr float FireRecoveryPerSec = 35.f;
				inline constexpr ETargetTypeIcon TargetTypeIcon = ETargetTypeIcon::HeavilyArmored;
				// This damage reduction struct applies to all types of damage
				inline constexpr FDamageReductionSettings DamageReduction(0, 1.f);
			}

			namespace SuperHeavyArmor
			{
				// Takes 110% laser damage on front and 115% on sides and 120% on rear.
				// radiation damage is 1% front, 3% sides and 5% rear.
				inline constexpr FLaserRadiationDamageMlt LaserRadiationMlt{
					FDamageMltPerSide{1.1f, 1.15f, 1.2f},
					FDamageMltPerSide{0.01f, 0.03f, 0.05f}
				};
				inline constexpr float FireThresholdMaxHPMlt = 0.33;
				inline constexpr float FireRecoveryPerSec = 55.f;
				inline constexpr ETargetTypeIcon TargetTypeIcon = ETargetTypeIcon::SuperHeavyArmor;
				// This damage reduction struct applies to all types of damage
				inline constexpr FDamageReductionSettings DamageReduction(0, 1.f);
			}

			// A type of building armor.
			namespace BasicBuildingArmor
			{
				// Takes 60% laser damage
				// Takes 15% radiation damage
				inline constexpr FLaserRadiationDamageMlt LaserRadiationMlt{
					FDamageMltPerSide::Uniform(0.6),
					FDamageMltPerSide::Uniform(0.15)
				};
				inline constexpr float FireThresholdMaxHPMlt = 0.05;
				inline constexpr float FireRecoveryPerSec = 5.f;
				inline constexpr ETargetTypeIcon TargetTypeIcon = ETargetTypeIcon::BasicBuildingArmor;
				// This damage reduction struct applies to all types of damage
				inline constexpr FDamageReductionSettings DamageReduction(20, 1.f);
			};

			// A type of building armor.
			namespace ReinforcedArmor
			{
				// Takes 80% laser damage
				// Takes 10% radiation damage
				inline constexpr FLaserRadiationDamageMlt LaserRadiationMlt{
					FDamageMltPerSide::Uniform(0.8),
					FDamageMltPerSide::Uniform(0.1)
				};
				inline constexpr float FireThresholdMaxHPMlt = 0.05;
				inline constexpr float FireRecoveryPerSec = 8.f;
				inline constexpr ETargetTypeIcon TargetTypeIcon = ETargetTypeIcon::Reinforced;
				// This damage reduction struct applies to all types of damage
				inline constexpr FDamageReductionSettings DamageReduction(25, 0.7f);
			};
		}


		namespace InfantrySettings
		{
			// Infantry Speed Settings.
			inline constexpr float BasicInfantrySpeed = 500.f;
			inline constexpr float BasicInfantryAcceleration = 1350.f;
			inline constexpr float FastInfantrySpeed = 700.f;
			inline constexpr float FastInfantryAcceleration = 1800.f;
			inline constexpr float SlowInfantrySpeed = 400.f;
			inline constexpr float SlowInfantryAcceleration = 1050.f;
		}
	}

	namespace GamePlay
	{
		namespace Audio
		{
			// How many spatial audio actors we pool to play spatial voice lines.
			inline constexpr int32 SpatialAudioPoolSize = 32;
			inline constexpr float BaseIntervalBetweenSpatialVoiceLines = 12.f;
			inline constexpr float FluxPercentageIntervalBetweenSpatialVoiceLines = 30.f;
			inline constexpr float MaxStaggerBetweenSpatialVoiceLines = 1.f;
			// Default cooldown used when no per-type override is configured.
			inline constexpr float CoolDownBetweenSpatialAudioOfSameType = 2.f;
			inline constexpr float CheckResourceStorageFullInterval = 5.6f;
			inline constexpr float MinDelayBetweenVoiceLines = 2.f;

			/**
			 * @brief Optional per-type overrides for the spatial cooldown between
			 *        voice lines of the same type.
			 *
			 * If a given ERTSVoiceLine is not present in this map, the engine will
			 * fall back to CoolDownBetweenSpatialAudioOfSameType.
			 */
			inline const TMap<ERTSVoiceLine, float> SpatialVoiceLineCooldowns = {
				{ERTSVoiceLine::Fire, 1.0f},
				{ERTSVoiceLine::ShotBounced, 1.5f},
				{ERTSVoiceLine::ShotConnected, 1.5f},
				{ERTSVoiceLine::IdleTalk, 8.0f},
				{ERTSVoiceLine::InCombatTalk, 6.f},
			};
			/**
			 * @brief Chance (0–1) that a given spatial voice line will actually fire.
			 * Used by USpatialVoiceLinePlayer::ShouldPlaySpatialVoiceLine().
			 * @note This does only contain voice lines we expect to be spatial no radio voice lines.
			 */
			inline const TMap<ERTSVoiceLine, float> SpatialVoiceLinePlayProbability = {
				{ERTSVoiceLine::Fire, 0.50f},
				{ERTSVoiceLine::ShotBounced, 0.70f},
				{ERTSVoiceLine::ShotConnected, 0.70f},
				{ERTSVoiceLine::UnitDies, 1.00f},
				{ERTSVoiceLine::EnemyDestroyed, 1.f},
			};

			namespace SquadUnits
			{
				inline constexpr float BaseIntervalBetweenSpatialVoiceLines = 30.f;
				inline constexpr float FluxPercentageIntervalBetweenSpatialVoiceLines = 30.f;
				inline constexpr float TimeBetweenIdleVoiceLines = 60.f;
				inline constexpr int32 IdleVoiceLineFluxPercentage = 10.f;

				/**
				 * @brief Chance (0–1) that a given spatial voice line will actually fire.
				 * Used by USpatialVoiceLinePlayer::ShouldPlaySpatialVoiceLine().
				 * @note This does only contain voice lines we expect to be spatial no radio voice lines.
				 */
				inline const TMap<ERTSVoiceLine, float> SpatialVoiceLinePlayProbability = {
					{ERTSVoiceLine::Fire, 0.1f},
					{ERTSVoiceLine::ShotBounced, 1.f},
					{ERTSVoiceLine::ShotConnected, 1.f},
					{ERTSVoiceLine::UnitDies, 1.00f},
					{ERTSVoiceLine::EnemyDestroyed, 1.f},
				};
			}
		}

		namespace Formation
		{
			// The maximum amount of priorities used in unit formation. 
			inline constexpr uint64 MaxPriorities = 30;

			inline constexpr bool LetPlayerTanksCollide = false;

			inline constexpr float SpawnFormationEdgeLenght = 800;

			// Determines the size of the array of formation position effects.
			// These effects are used to visualize the formation, should be a power of two.
			inline constexpr int32 MaxFormationEffects = 32;
		}

		namespace Navigation
		{
			namespace Aircraft
			{
				// scales how far the point is placed to get the aircraft out of the deadzone.
				inline constexpr float InDeadZonePointDistMlt = 2.f;
				// Divides the angle to the deadzone point to obtain the angle at which we place the point to get out of the deadzone.
				inline constexpr float InDeadZonePointAngleDivider = 5.f;
				// Divides the total angle to the move to point to calculate how many bezier samples we need to use in the curve.
				inline constexpr float MoveToAngleBezierSampleDivider = 2.f;
				// How far from the out-of-dive point the next path point on the aircraft will be to restore rotation.
				inline constexpr float OutOfDivePointOffset = 1000.f;
				// Multiplies how much longer the path of altitude recovery should be after an aircraft performed a dive.
				inline constexpr float AltitudeRecoveryPathLengthMlt = 1.5f;

				// Percentage margin we use for when the target is too far and we want  to determine to fly-to point within
				// attack range for the aircraft. if set to 75 we fly to a point that is 75% of the max attack range.
				// do not set this higher than 99 as this will cause the aircraft to fly to a point that is too far away
				inline constexpr float PercentageMaxAttackRangeToTarget = 75.f;

				// Delta from  abs degrees to enemy location to determine whether we are on an extreme angle to the enemy.
				// within range of 180 degrees.
				inline constexpr float DeltaAngleExtreme = 25.f;

				// How much the aircraft will turn on each UTurn Approach point.
				// These points are placed before the bezier of the U turn.
				inline constexpr float UTurnApproachPointAngle = 20.f;

				// Scales the max attack distance to find beyond which distance we consider the target as extremely far and
				// perform a regular move towards it first.
				inline constexpr float MaxAttackDistanceScale = 1.5f;
			}

			// If the final destination is within this incline degree range, then the vehicle will reduce throttle.
			// If not the vehicle will not reduce throttle as it is slowed by gravity.
			inline constexpr uint64 TrackVehicleIncline = 5;

			// How low the speed of a vehicle will count to being stuck.
			inline constexpr float StuckSpeedThreshold = 30.f;

			// Determines how strong the gravity is applied to the tank's mesh.
			inline float TankTrackGravityMultiplier = 0.5f;

			// If above this threshold the tank will try to unflip the vehicle.
			inline constexpr float TankPitchRollConstrainAngle = 45.f;

			// The distance used as offset to the path finding point for units in the squad
			// to make the squad move more spread out.
			inline constexpr float SqPathFinding_OffsetDistance = 75.f;
			// The offset / spread of the squad when moving to the final destination.
			inline constexpr float SqPathFinding_FinalDestSpread = 150.f;


			// How big the nav radius gets around the target location for the squad to move to per 2 units increase.
			inline constexpr float OffsetPer2UnitsInSquadnavigation = 150.f;

			inline constexpr float SquadUnitAcceptanceRadius = 30.f;
			inline constexpr float SquadUnitCargoEntranceAcceptanceRadius = 350.f;

			// At this distance the squad can instantly pickup the weapon, to prevent errors where the overlap event will not
			// occur properly.
			inline constexpr float SquadUnitWeaponPickupDistance = 250.f;
			inline constexpr float SquadUnitFieldConstructionDistance = 250.f;

			inline FVector RTSToNavProjectionExtent = FVector(300.f, 300.f, 100.f);

			// At this distance the squad can instantly scavenge the object, to prevent errors where the overlap event will not
			// occur properly.
			// takes 100 as avg radius for object into account.
			inline constexpr float SquadUnitScavengeDistance = 350.f;

			// Extent used to project potential harvester positions to the navmesh.
			inline constexpr float HarvesterPositionProjectedExtent = 450.f;

			inline constexpr float MinTimeBetweenDropOffNotificationHarvester = 12.5f;

			// Extent used in X-Y to project potential formation locations for enemy formations.
			inline constexpr float EnemyFormationPositionProjectionExtent = 200.f;
		}

		namespace Environment
		{
			constexpr float RTSDecalHeight = 50.f;
		}

		namespace FoW
		{
			inline const FName TagUnitInEnemyVision = "IsInEnemyVision";
			inline constexpr float VisibleEnoughToRevealEnemy = 0.10;
			// Note that enemy vision is binary as either a player unit is in enemy vision which means that the particle
			// associated with this unit has size 1 or not in which case the size is zero.
			inline constexpr float VisibleEnoughToRevealPlayer = 0.20;
		}

		namespace Turret
		{
			// The amount of cm we subtract from the max range of the turret to obtain a radius within which we
			// search for a valid target.
			inline constexpr float RangeMargin = 201.f;
			// Base time between idle turret animations.
			inline constexpr float RotateAnimationInterval = 15.f;
			inline constexpr float RotateAnimationFlux = 3.f;

			// Minimum elapsed time between turret-driven move-to-target reissues while already moving.
			inline constexpr float MoveToTargetReissueElapsedSeconds = 2.0f;
		}

		namespace Projectile
		{
			// Projectile pool size for smallarms.
			inline constexpr int32 SmallArmsProjectilePoolSize = 256;
			inline constexpr int32 SmallArmsProjectileSpeed = 7500;
			// Projectile pool size for tank projectiles.
			inline constexpr int32 TankProjectilePoolSize = 64;
			// For any regular tank weapon.
			inline constexpr float BaseProjectileSpeed = 7000;
			// For high velocity guns.
			inline constexpr float HighVelocityProjectileSpeed = 8000;
			// For He shells and slower projectiles.
			inline constexpr float HEProjectileSpeed = 5500;

			namespace ProjectilesVfx
			{
				// Defines how large the launch smoke and muzzle flash can grow.
				inline constexpr float MaxLaunchVfxMlt = 1.5f;
				// Basic width for 75mm projectiles.
				inline constexpr float BaseShellWidth = 16.f;
				inline constexpr float APShellWidthMlt = 1.0;
				inline constexpr float APBCShellWidthMlt = 1.0;
				inline constexpr float HEShellWidthMlt = 1.8f;
				inline constexpr float FireShellWidthMlt = 1.8f;
				inline constexpr float HEATShellWidthMlt = 1.2f;
				inline constexpr float APCRShellWidthMlt = 0.8;
				inline const FVector RailGunProjectileScaleBase = FVector(1.0f, 1.0f, 1.0f);
			}
		}

		namespace Construction
		{
			// How many Material Instances the construction preview supports.
			inline constexpr int MaxNumberMaterialsOnPreviewMesh = 30;

			// Amount of incline degrees buildings are allowed to be placed on a hill.
			inline constexpr float DegreesAllowedOnHill = 5;

			// The amount of cms added to each trace point on a preview mesh to check if the slope is too steep.
			inline constexpr float AddedHeightToTraceSlopeCheckPoint = 100.f;

			// Amount of cms with which we calculated gird squares to help the player place buildings.
			inline constexpr float GridSnapSize = 50.f;


			// Name part that sockets need to have to be recognized as BXP sockets.
			const FString BxpSocketTrigger = "BXP";
		}

		namespace Training
		{
			// The maximum amount of training options a trainer can have, supported by the widgets.
			static int32 MaxTrainingOptions = 24;

			// The maximum amount of training items a trainer can have in the queue.
			inline constexpr uint32 MaxTrainingQueueSize = 128;

			inline constexpr int32 TimeRemainingStartAsyncLoading = 2;
		}

		namespace ActionUI
		{
			// Defines the maximum lenght for an ability array of a unit.
			inline constexpr int32 MaxAbilitiesForActionUI = 15;
		}

		namespace Resources
		{
			// Used to check if a spot at a resource for harvesting is already occupied by another harvester.
			// The higher this value, the more widely a harvester location will block other harvester locations;
			// for small resources a too large radius may prevent the harvester from finding a spot.
			inline constexpr float HarvesterLocationBlockageRadiusSquared = 2500.f;

			// How often the harvester should check if it is idle and if so it will resume harvesting operations.
			inline constexpr float HarvesterIdleCheckInterval = 5.f;
		}

		namespace HealthCompAndHealthBar
		{
			// Divide the max health by this to obtain the amount of slices.
			constexpr int32 HPBarSliceRatio = 85;
			// Divide the max FireThreshold by this to obtain the amount of slices.
			constexpr int32 FireThresholdBarSliceRatio = 50;
		}


		// If the absolute value of the angular velocity of the tank is below this value, the tank will consider
		// the movement as straight
		inline constexpr float AngleIgnoreLeftRightTrackAnim = 5.f;
	}


	namespace UIUX
	{
		// The amount of UE cms that determines the max distance the mouse can be away from the camera;
		// still checking for collision with the landscape.
		inline constexpr float SightDistanceMouse = 10000;

		inline constexpr float MinZoomLimit = 300.f;
		// corresponds with 8500 range in the game (weapon on left side engaging weapon just visible on right side)
		// inline constexpr float MaxZoomLimit = 4200.f;
		inline constexpr float MaxZoomLimit = 4800.f;
		inline constexpr float ZoomForPlayerStartOverview = 10000.f;
		inline constexpr float DefaultTerrainHeight = 110.f;
		inline constexpr float ZoomSpeed = 150.f;
		inline constexpr float CameraPanSpeed = 5.f;
		// 25 degrees to make skybox not visible.
		inline constexpr float CameraPitchLimit = 18.f;
		inline constexpr float DefaultCameraMovementSpeed = 15.f;
		inline static float ModifierCameraMovementSpeed = 1.f;
		// How long the player needs to hover an actor to get the hover popup.
		inline static float HoverTime = 0.5f;
		// How much the mouse needs to have moved in pixels for the hover popup to disappear.
		inline static float HoverMouseMoveThreshold = 2.f;
		// How many pixels (Screen space) the mouse needs to move for dragging logic to be triggered.
		inline static constexpr float MouseDragThreshold = 2.f;


		namespace TechTree
		{
			inline constexpr float TechTreeWidth = 5000.f;
			inline constexpr float TechTreeHeight = 3000.f;
			// if set to true we invert the mouse navigation on the tech tree.
			inline constexpr bool bUseTechTreeMouseDragNavigation = true;
			inline constexpr float PanSpeed = 1.f;
			inline constexpr float PanSmoothness = 10;
		}


		namespace ClickVfx
		{
			inline constexpr int MaxAmountPlacementEffects = 64;
		}
	}

	namespace Optimization
	{
		// Minimum weapon calibre that can trigger tank hull fire feedback.
		inline constexpr int32 MinCalibreWeaponFeedback = 19;
		// Minimum weapon calibre that can trigger camera shake.
		inline constexpr int32 MinCalibreCameraShake = 87;

		// Below this distance we always consider the unit within FOV.
		inline constexpr float DistanceAlwaysConsiderUnitInFOV = 4000.f;
		inline constexpr float DistanceAlwaysConsiderUnitInFOVSquared = DistanceAlwaysConsiderUnitInFOV *
			DistanceAlwaysConsiderUnitInFOV;
		// The distance outside of FOV we consider the unit to be far away and is optimized the most.
		inline constexpr float OutOfFOVConsideredFarAwayUnit = 8000.f;
		inline constexpr float OutOfFOVConsideredFarAwayUnitSquared = OutOfFOVConsideredFarAwayUnit *
			OutOfFOVConsideredFarAwayUnit;

		// If a component ticks and is out of FOV but close this is the multiplier used to determine the new tick time.
		inline constexpr float OutofFOVCloseTickTimeMlt = 2.f;
		// If a component ticks and is out of FOV and considered far then this is the multiplier we use to determine the new tick time.
		inline constexpr float OutofFOVFarTickTimeMlt = 5.f;

		// If a ticking component has 0.f as tick time then this is the tick time used out of Fov close.
		inline constexpr float ImportantTickCompOutOfFOVCloseTickRate = 0.33;

		// If a ticking component has 0.f as tick time then this is the tick time used out of Fov far.
		inline constexpr float ImportantTickCompOutOfFOVFarTickRate = 1.f;

		namespace Tank
		{
			// For tanks we use the same tick interval in full fov as in close out of fov to ensure smooth rotate towards.
			inline constexpr float OutOfFOVFar_TankTickInterval = 0.5f;
		}

		namespace Turrets
		{
			inline constexpr float OutOfFOVClose_TurretTickInterval = 0.2f;
			inline constexpr float OutOfFOVFar_TurretTickInterval = 0.5f;
		}


		namespace CharacterMovement
		{
			// How often we tick out of fov close character movement components.
			inline constexpr float OutOfFOVClose_CharacterMovementTickInterval = 0.2f;
			inline constexpr int32 OutOfFOVClose_CharacterMovementSimulationSteps = 4;

			// How often we tick out of fov far character movement components.
			inline constexpr float OutOfFOVFar_CharacterMovementTickInterval = 0.5f;
			inline constexpr int32 OutOfFOVFar_CharacterMovementSimulationSteps = 2;
		}


		namespace CleanUp
		{
			// How often the GameUnitManager cleans the unit arrays of invalid pointers, in seconds.
			constexpr float UnitMngrCleanUpInterval = 5.f;
			// How often the GameResourceManager cleans the Resource and DropOff arrays of invalid pointers, in seconds.
			constexpr float ResourceMngrCleanUpInterval = 10.f;
		}


		// Determines the orientation frequency of the HealthBar towards the camera.
		// at 30 fps, this is 1.0f/30.0f = 0.03333f
		inline constexpr float OrientHealthBarInterval = 0.03333f;
		inline constexpr float UpdateIntervalProgressBar = 0.1f;
		// Update at 15 fps: 1/15 = 0.0666666666666667
		inline constexpr float UpdateEngineSoundsInterval = 0.0666666666666667;
		inline constexpr float UpdateSquadAnimInterval = 0.00666666666666667;
		inline constexpr float TurretTickInterval = 0.0167;
		inline constexpr float UpdateChassisVFXInterval = 0.33;
		// How often the projectile traces in front to see if it hits something.
		inline constexpr float ProjectileTraceInterval = 0.15;
		// How often marker widgets in screen space are updated to point to their world space location.
		inline constexpr float UpdateMarkerWidgetInterval = 0.1f;
	}

	namespace Debugging
	{
		constexpr bool GNomadicSkeletalAttachments_Compile_DebugSymbols = false;
		constexpr bool GCampaignConnectionGeneration_Compile_DebugSymbols = false;
		constexpr bool GCampaignBacktracking_Report_Compile_DebugSymbols = true;
		constexpr bool GCampaignBacktracking_Compile_DebugSymbols = true;
		constexpr bool GWorldFow_Compile_DebugSymbols = false;
		// Overlap logic; tanks evasion.
		constexpr bool GTankOverlaps_Compile_DebugSymbols = false;
		constexpr bool GWpoTreeAndFoliage_Compile_DebugSymbols = false;
		// Debug enemy controller
		constexpr bool GEnemyController_Compile_DebugSymbols = false;
		constexpr bool GEnemyController_StrategicAI_Compile_DebugSymbols = false;
		constexpr bool GEnemyController_NavDetector_DebugSymbols = false;
		constexpr bool GEnemyController_DirectControl_Compile_DebugSymbols = false;
		constexpr bool ExplosionsManager_Compile_DebugSymbols = false;
		constexpr bool GPlayerStartLocations_Compile_DebugSymbols = false;
		constexpr bool GCamera_Player_Compile_DebugSymbols = false;
		// Debug info about the units registered at the unit manager.
		constexpr bool GUnitManager_Compile_DebugSymbols = false;
		constexpr bool GAction_UI_Compile_DebugSymbols = false;
		constexpr bool GBuilding_Mode_Compile_DebugSymbols = false;
		constexpr bool GTurret_Master_Compile_DebugSymbols = false;
		constexpr bool GEmbedded_Turret_Compile_DebugSymbols = false;
		// Hull Weapon components.
		constexpr bool GHull_Weapons_Compile_DebugSymbols = false;
		constexpr bool GTargetAimOffsets_Compile_DebugSymbols = false;
		constexpr bool GTargetAcquisition_Compile_DebugSymbols = false;
		constexpr bool GAOELibrary_Compile_DebugSymbols = false;
		// Damage taken on actors
		constexpr bool GDamage_System_Compile_DebugSymbols = false;
		// ICommands.
		constexpr bool GCommands_Compile_DebugSymbols = false;
		constexpr bool GArchProjectile_Compile_DebugSymbols = false;
		// Reinforcement provider and ability debug draws.
		constexpr bool GReinforcementAbility_Compile_DebugSymbols = false;
		constexpr bool GPathFollowing_Compile_DebugSymbols = false;
		constexpr bool GRTSNavAgents_Compile_DebugSymbols = false;
		constexpr bool GPathFindingCosts_Compile_DebugSymbols = false;
		// Track animation
		constexpr bool GVehicle_Track_Animation_Compile_DebugSymbols = false;
		// Wheel Animation
		constexpr bool GVehicle_Wheel_Animation_Compile_DebugSymbols = false;
		constexpr bool GConstruction_Preview_Compile_DebugSymbols = false;
		constexpr bool GPlayerClickAndAction_Compile_DebugSymbols = false;
		constexpr bool GPlayerSelection_Compile_DebugSymbols = false;
		// Harvesting
		constexpr bool GHarvestResources_Compile_DebugSymbols = false;
		constexpr bool ResourcesShowOccupyingHarvesters = false;
		constexpr bool GWeapon_ArmorPen_Compile_DebugSymbols = false;
		constexpr bool GArmorCalculation_Compile_DebugSymbols = false;
		constexpr bool GArmorCalculation_Resistances_Compile_DebugSymbols = false;
		constexpr bool GSquadUnit_Weapons_Compile_DebugSymbols = false;
		constexpr bool GSquadUnit_SwitchPickUp_Weapons_Compile_DebugSymbols = false;
		constexpr bool GScavenging_Compile_DebugSymbols = false;
		constexpr bool GRepair_Compile_DebugSymbols = false;
		constexpr bool GTechTree_Compile_DebugSymbols = false;
		constexpr bool GBuildRadius_Compile_DebugSymbols = false;
		// Card system
		constexpr bool GCardSystem_Compile_DebugSymbols = false;
		constexpr bool GFowSystem_Compile_DebugSymbols = false;
		constexpr bool GFowComponents_Compile_DebugSymbols = false;
		constexpr bool GAsyncTargetFinding_Compile_DebugSymbols = false;
		constexpr bool GControlGroups_Compile_DebugSymbols = false;
		constexpr bool GMouseHover_Compile_DebugSymbols = false;
		// Formations layout calculations.
		constexpr bool GFormations_Compile_DebugSymbols = false;
		// Physical Materials / Surface Types
		constexpr bool GPhysicalMaterialSurfaces_Compile_DebugSymbols = false;
		// Crushable actor debugging messages.
		constexpr bool GCrushableActors_Compile_DebugSymbols = false;
		// Compile debug for destructable actors taking damage.
		constexpr bool GDestructableActors_Compile_DebugSymbols = false;
		// The Small arms projectile pool.
		constexpr bool GProjectilePooling_Compile_DebugSymbols = false;
		// The experience system.
		constexpr bool GExperienceSystem_Compile_DebugSymbols = false;
		// Missions
		constexpr bool GMissions_CompileDebugSymbols = false;
		// Player audio / voiceline controller
		constexpr bool GAudioController_Compile_DebugSymbols = false;
		// Player rotation arrow
		constexpr bool GPlayerRotationArrow_Compile_DebugSymbols = false;
		// Training component
		constexpr bool GTrainingComponent_Compile_DebugSymbols = false;
		// Debug landscape divider system.
		constexpr bool GLandscapeDivider_Compile_DebugSymbols = false;
		// Debug Landscape deform manager.
		constexpr bool GLandscapeDeformManager_Compile_DebugSymbols = false;
		// Debug Healthcomponent, healthbar.
		constexpr bool GHealthComponent_Compile_DebugSymbols = false;
		// Optimization component.
		constexpr bool GOptimComponent_Compile_DebugSymbols = false;
		// For aircraft movement.
		constexpr bool GAircraftMovement_Compile_DebugSymbols = false;
		// For Aircraft bombing.
		constexpr bool GBombBay_Compile_DebugSymbols = false;
		// Ammo tracking system.
		constexpr bool GAmmoTracking_Compile_DebugSymbols = false;
	}
}

// On debugging sounds.
// stat sounds -debug
// au.debug.sounds 1
// for 3d visualization:
// 
