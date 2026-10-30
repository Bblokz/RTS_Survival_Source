// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalSovietBaseSettings.h"

#include "RTS_Survival/Units/Enums/Enum_UnitType.h"

#define LOCTEXT_NAMESPACE "SurvivalSovietBaseSettings"

namespace SurvivalSovietBaseDefaults
{
	// Verified on disk 2026-10-29 (see Docs/50_SovietBases.md for the full list and building types).
	const TCHAR* const RusBuildings = TEXT("/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/");
	const TCHAR* const RusBunkers = TEXT("/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/");
	const TCHAR* const Bxps = TEXT("/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/");
	const TCHAR* const Barricades =
		TEXT("/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Military/Barricades/");
	const TCHAR* const Sandbags =
		TEXT("/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/NaturalCover/SandBagWalls/");
	const TCHAR* const Airport = TEXT("/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Airport/");
	const TCHAR* const Military = TEXT("/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Military/");

	/** @return A soft class path "<Folder><SubPath>/<Name>.<Name>_C"; AssetPath may contain sub folders. */
	FSoftObjectPath MakeClassPath(const TCHAR* Folder, const TCHAR* AssetPath)
	{
		const FString RelativePath(AssetPath);
		FString AssetName = RelativePath;
		int32 LastSlash = INDEX_NONE;
		if (RelativePath.FindLastChar(TEXT('/'), LastSlash))
		{
			AssetName = RelativePath.RightChop(LastSlash + 1);
		}
		return FSoftObjectPath(FString::Printf(TEXT("%s%s.%s_C"), Folder, *RelativePath, *AssetName));
	}

	FEnemyBlueprintEntry MakeEntry(const TCHAR* Folder, const TCHAR* AssetName, const float Weight = 1.f)
	{
		FEnemyBlueprintEntry Entry;
		Entry.BlueprintClass = TSoftClassPtr<AActor>(MakeClassPath(Folder, AssetName));
		Entry.Weight = Weight;
		return Entry;
	}

	FSurvivalSovietStructureSlot MakeSlot(
		const TCHAR* Folder,
		const TCHAR* AssetName,
		const int32 MinStrength,
		const int32 ProductionTier,
		const int32 Count,
		const float CountPerStrength)
	{
		FSurvivalSovietStructureSlot Slot;
		Slot.Building = MakeEntry(Folder, AssetName);
		Slot.MinStrength = MinStrength;
		Slot.ProductionTier = ProductionTier;
		Slot.Count = Count;
		Slot.CountPerStrength = CountPerStrength;
		return Slot;
	}

	FTrainingOption Tank(const ETankSubtype Subtype)
	{
		return FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(Subtype));
	}

	FTrainingOption Squad(const ESquadSubtype Subtype)
	{
		return FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(Subtype));
	}

	FSurvivalSovietGarrisonGroup MakeGarrison(
		const TArray<FTrainingOption>& Options,
		const int32 Count,
		const float CountPerStrength,
		const int32 MinStrength,
		const ESurvivalSovietGarrisonBehaviour Behaviour)
	{
		FSurvivalSovietGarrisonGroup Group;
		Group.UnitOptions = Options;
		Group.Count = Count;
		Group.CountPerStrength = CountPerStrength;
		Group.MinStrength = MinStrength;
		Group.Behaviour = Behaviour;
		return Group;
	}

	FSurvivalSovietBaseReward MakeReward(const ERTSResourceType Resource, const int32 Amount)
	{
		FSurvivalSovietBaseReward Reward;
		Reward.Resource = Resource;
		Reward.Amount = Amount;
		return Reward;
	}

	/** @brief Hedgehog belt, sandbag parapet and wire shared by every default preset. */
	void FillDefaultLineWorks(FSurvivalSovietPerimeterSettings& Perimeter)
	{
		Perimeter.ObstacleBelt.bEnabled = true;
		Perimeter.ObstacleBelt.Obstacles = {
			MakeEntry(Barricades, TEXT("BP_Military_Dest_Hedgehog"), 2.f),
			MakeEntry(Barricades, TEXT("BP_Military_Dest_Hedgehog4"), 1.f),
			MakeEntry(Barricades, TEXT("BP_Military_Dest_LargeHedgehog"), 0.5f)
		};
		Perimeter.ObstacleBelt.DistanceFromLine = 900.f;
		Perimeter.ObstacleBelt.SpacingBetweenObstacles = 450.f;

		Perimeter.BarbedWire.bEnabled = true;
		Perimeter.BarbedWire.WireSegments = {
			MakeEntry(Barricades, TEXT("BP_Military_Dest_BarbedWire"), 1.f),
			MakeEntry(Barricades, TEXT("BP_Military_Dest_BarbedWireLong"), 1.f)
		};
		Perimeter.BarbedWire.DistanceFromLine = 500.f;

		Perimeter.Sandbags.bEnabled = true;
		Perimeter.Sandbags.SandbagUnits = {
			MakeEntry(Sandbags, TEXT("BP_Cover_RusSandbags_SingleStraight"), 2.f),
			MakeEntry(Sandbags, TEXT("BP_Cover_RusSandbags_DoubleStraight"), 1.f)
		};
		Perimeter.Sandbags.Coverage = EEnemySandbagCoverage::FrontAndFlanks;
	}

	FEnemyDecoratorProfile MakeSupplyDecorators()
	{
		FEnemyDecoratorProfile Profile;
		Profile.Decorators = {
			MakeEntry(Airport, TEXT("BP_Airport_Dest_CrateStack01")),
			MakeEntry(Airport, TEXT("BP_Airport_Dest_CrateStack02")),
			MakeEntry(Airport, TEXT("BP_Airport_Dest_CrateStack03")),
			MakeEntry(Airport, TEXT("BP_Airport_Dest_BarrelStack")),
			MakeEntry(Airport, TEXT("BP_Airport_Dest_BarrelStack2")),
			MakeEntry(Military, TEXT("BP_Military_Dest_Camonet"), 0.5f)
		};
		Profile.CountPerSetup = 2.f;
		Profile.MinDistanceFromBounds = 150.f;
		Profile.MaxDistanceFromBounds = 700.f;
		Profile.ChancePerSetup = 0.75f;
		return Profile;
	}
}

USurvivalSovietBaseSettings::USurvivalSovietBaseSettings()
{
	BaseDestroyedNotification = LOCTEXT("BaseDestroyed", "Soviet base destroyed: {0}");
	FillDefaultPresets();
	FillDefaultDifficultyScaling();
	FillDefaultStrategicAITypes();
}

FSurvivalSovietBaseDifficultyScaling USurvivalSovietBaseSettings::GetScalingForDifficulty(
	const ERTSGameDifficulty Difficulty) const
{
	const FSurvivalSovietBaseDifficultyScaling* NormalRow = nullptr;
	for (const FSurvivalSovietBaseDifficultyScaling& Row : DifficultyScaling)
	{
		if (Row.Difficulty == Difficulty)
		{
			return Row;
		}
		if (Row.Difficulty == ERTSGameDifficulty::Normal)
		{
			NormalRow = &Row;
		}
	}
	return NormalRow != nullptr ? *NormalRow : FSurvivalSovietBaseDifficultyScaling();
}

#if WITH_EDITOR
void USurvivalSovietBaseSettings::ResetToDefaults()
{
	Modify();
	FillDefaultPresets();
	FillDefaultDifficultyScaling();
	FillDefaultStrategicAITypes();
}
#endif

void USurvivalSovietBaseSettings::FillDefaultPresets()
{
	using namespace SurvivalSovietBaseDefaults;
	Presets.Reset();

	// --- Rifle Outpost: barracks camp, corner strongpoints, light AT/AA pits, infantry garrison. ---
	{
		FSurvivalSovietBasePreset& Preset = Presets.AddDefaulted_GetRef();
		Preset.PresetName = TEXT("RifleOutpost");
		Preset.DisplayName = LOCTEXT("RifleOutpost", "Rifle Outpost");
		Preset.Weight = 1.f;
		Preset.MinStrength = 0;
		Preset.MaxStrength = 1;
		Preset.CoreCompoundRadius = 1500.f;
		Preset.ProductionBuildings = {
			MakeSlot(RusBuildings, TEXT("Rus_Building_Barracks"), 0, 0, 1, 0.f),
			MakeSlot(RusBuildings, TEXT("Rus_Building_LargeTent"), 0, 0, 1, 0.5f),
			MakeSlot(RusBuildings, TEXT("Rus_Building_CoolingTowers"), 1, 1, 1, 0.f)
		};
		Preset.SupportBuildings = {
			MakeSlot(RusBuildings, TEXT("Rus_Building_Supplies"), 0, -1, 1, 0.f),
			MakeSlot(RusBuildings, TEXT("Rus_Building_AmmoStorage"), 0, -1, 1, 0.f),
			MakeSlot(RusBuildings, TEXT("Rus_Bxp_Megaphone"), 0, -1, 1, 0.f),
			MakeSlot(RusBunkers, TEXT("Rus_BXP_GuardTowerMG"), 0, -1, 2, 1.f)
		};
		Preset.Perimeter.LayoutPerStrength = {EEnemyPerimeterLayout::Corners, EEnemyPerimeterLayout::FrontLine};
		Preset.Perimeter.Bunkers = {
			MakeEntry(RusBunkers, TEXT("Rus_BXP_MGBunker"), 2.f),
			MakeEntry(RusBunkers, TEXT("Rus_BXP_AutoCnBunker"), 1.f)
		};
		FillDefaultLineWorks(Preset.Perimeter);
		Preset.Perimeter.WireMinStrength = 1;
		Preset.GunPits = {
			MakeSlot(Bxps, TEXT("RusDShKPit/Rus_BXP_DShKPit"), 0, -1, 1, 0.5f),
			MakeSlot(Bxps, TEXT("Rus53KPit/Rus_BXP_53KPit"), 0, -1, 1, 1.f),
			MakeSlot(Bxps, TEXT("RusMortarPit/Rus_BXP_120MortarPit"), 1, -1, 1, 0.f)
		};
		Preset.DecoratorProfiles = {MakeSupplyDecorators()};
		Preset.Garrison = {
			MakeGarrison({
				             Squad(ESquadSubtype::Squad_Rus_Mosin), Squad(ESquadSubtype::Squad_Rus_Dyakonov),
				             Squad(ESquadSubtype::Squad_Rus_Okhotnik)
			             }, 3, 1.f, 0, ESurvivalSovietGarrisonBehaviour::HoldPosition),
			MakeGarrison({Tank(ETankSubtype::Tank_Ba12), Tank(ETankSubtype::Tank_BT7)}, 2, 0.5f, 0,
			             ESurvivalSovietGarrisonBehaviour::Patrol),
			MakeGarrison({Squad(ESquadSubtype::Squad_Rus_HazmatBazooka), Squad(ESquadSubtype::Squad_Rus_KorovinAssault)},
			             1, 1.f, 1, ESurvivalSovietGarrisonBehaviour::StrategicAIReserve)
		};
		Preset.DestroyRewards = {
			MakeReward(ERTSResourceType::Resource_Radixite, 250),
			MakeReward(ERTSResourceType::Resource_Metal, 200)
		};
	}

	// --- Tank Depot: factory + barracks, front line facing the player, heavy AT pits, armour garrison. ---
	{
		FSurvivalSovietBasePreset& Preset = Presets.AddDefaulted_GetRef();
		Preset.PresetName = TEXT("TankDepot");
		Preset.DisplayName = LOCTEXT("TankDepot", "Tank Depot");
		Preset.Weight = 1.f;
		Preset.MinStrength = 1;
		Preset.MaxStrength = 2;
		Preset.CoreCompoundRadius = 1900.f;
		Preset.ProductionBuildings = {
			MakeSlot(RusBuildings, TEXT("Rus_Building_Factory"), 0, 1, 1, 0.f),
			MakeSlot(RusBuildings, TEXT("Rus_Building_Barracks"), 0, 0, 1, 0.f),
			// Second tier-0 building keeps >= 2 production buildings when the difficulty caps the tier at 0.
			MakeSlot(RusBuildings, TEXT("Rus_Building_LargeTent"), 0, 0, 1, 0.f),
			MakeSlot(RusBuildings, TEXT("Rus_Building_CoolingTowers"), 0, 1, 1, 0.f),
			MakeSlot(RusBuildings, TEXT("Rus_Building_ResearchCenter"), 2, 2, 1, 0.f)
		};
		Preset.SupportBuildings = {
			MakeSlot(RusBuildings, TEXT("Rus_Building_FuelStorage"), 0, -1, 1, 0.f),
			MakeSlot(RusBuildings, TEXT("Rus_Building_FuelStorageTank"), 0, -1, 1, 0.5f),
			MakeSlot(RusBuildings, TEXT("Rus_Building_AmmoStorage"), 0, -1, 1, 0.f),
			MakeSlot(RusBuildings, TEXT("Rus_Bxp_ContainerHut"), 0, -1, 1, 0.f),
			MakeSlot(RusBunkers, TEXT("Rus_BXP_GuardTowerMG"), 0, -1, 2, 0.5f)
		};
		Preset.Perimeter.LayoutPerStrength = {
			EEnemyPerimeterLayout::FrontLine, EEnemyPerimeterLayout::FrontLine, EEnemyPerimeterLayout::FullPerimeter
		};
		Preset.Perimeter.Bunkers = {
			MakeEntry(RusBunkers, TEXT("Rus_BXP_Bunker02_ZIS"), 2.f),
			MakeEntry(RusBunkers, TEXT("Rus_BXP_DomeBunker"), 1.f),
			MakeEntry(RusBunkers, TEXT("Rus_BXP_LongCamoBunker"), 1.f)
		};
		FillDefaultLineWorks(Preset.Perimeter);
		Preset.Perimeter.ObstacleBelt.Formation = EEnemyObstacleFormation::DoubleRow;
		Preset.Perimeter.WireMinStrength = 1;
		Preset.GunPits = {
			MakeSlot(Bxps, TEXT("RusBS3Pit/Rus_BXP_BS3Pit"), 0, -1, 1, 1.f),
			MakeSlot(Bxps, TEXT("Rus53KPit/Rus_BXP_53KSectorPit"), 0, -1, 1, 0.5f),
			MakeSlot(Bxps, TEXT("RusDShKPit/Rus_BXP_DShKPit"), 0, -1, 1, 0.f),
			MakeSlot(RusBunkers, TEXT("Rus_BXP_Bofors"), 2, -1, 1, 0.f)
		};
		Preset.DecoratorProfiles = {MakeSupplyDecorators()};
		Preset.Garrison = {
			MakeGarrison({
				             Tank(ETankSubtype::Tank_T34_76), Tank(ETankSubtype::Tank_T34_85),
				             Tank(ETankSubtype::Tank_SU_85)
			             }, 2, 1.f, 0, ESurvivalSovietGarrisonBehaviour::HoldPosition),
			MakeGarrison({Tank(ETankSubtype::Tank_T70), Tank(ETankSubtype::Tank_BT7), Tank(ETankSubtype::Tank_T26)},
			             3, 0.5f, 0, ESurvivalSovietGarrisonBehaviour::Patrol),
			MakeGarrison({Squad(ESquadSubtype::Squad_Rus_Mosin), Squad(ESquadSubtype::Squad_Rus_LargePTRS)},
			             2, 0.5f, 0, ESurvivalSovietGarrisonBehaviour::HoldPosition),
			MakeGarrison({Tank(ETankSubtype::Tank_T34_AA)}, 1, 0.f, 2, ESurvivalSovietGarrisonBehaviour::HoldPosition),
			MakeGarrison({Tank(ETankSubtype::Tank_T34_76), Tank(ETankSubtype::Tank_SU_76)}, 1, 1.f, 1,
			             ESurvivalSovietGarrisonBehaviour::StrategicAIReserve)
		};
		Preset.DestroyRewards = {
			MakeReward(ERTSResourceType::Resource_Metal, 400),
			MakeReward(ERTSResourceType::Resource_VehicleParts, 250),
			MakeReward(ERTSResourceType::Resource_Fuel, 200)
		};
	}

	// --- Command Fortress: research centre + factory + barracks, full ring of heavy bunkers, artillery and AA. ---
	{
		FSurvivalSovietBasePreset& Preset = Presets.AddDefaulted_GetRef();
		Preset.PresetName = TEXT("CommandFortress");
		Preset.DisplayName = LOCTEXT("CommandFortress", "Command Fortress");
		Preset.Weight = 1.f;
		Preset.MinStrength = 2;
		Preset.MaxStrength = 5;
		Preset.CoreCompoundRadius = 2000.f;
		Preset.ProductionBuildings = {
			MakeSlot(RusBuildings, TEXT("Rus_Building_AdminBunker"), 0, 2, 1, 0.f),
			MakeSlot(RusBuildings, TEXT("Rus_Building_Factory"), 0, 1, 1, 0.f),
			MakeSlot(RusBuildings, TEXT("Rus_Building_Barracks"), 0, 0, 1, 0.f),
			MakeSlot(RusBuildings, TEXT("Rus_Building_LargeTent"), 0, 0, 1, 0.f)
		};
		Preset.SupportBuildings = {
			MakeSlot(RusBuildings, TEXT("Rus_Building_Supplies"), 0, -1, 1, 0.f),
			MakeSlot(RusBuildings, TEXT("Rus_Building_AmmoStorage"), 0, -1, 1, 0.5f),
			MakeSlot(RusBuildings, TEXT("Rus_Building_FuelStorageTank"), 0, -1, 1, 0.f),
			MakeSlot(RusBuildings, TEXT("Rus_Bxp_Megaphone"), 0, -1, 1, 0.f),
			MakeSlot(RusBunkers, TEXT("Rus_BXP_WeaponPlatform"), 0, -1, 2, 0.5f)
		};
		Preset.Perimeter.LayoutPerStrength = {EEnemyPerimeterLayout::FullPerimeter};
		Preset.Perimeter.Bunkers = {
			MakeEntry(RusBunkers, TEXT("Rus_BXP_RusBunker03"), 2.f),
			MakeEntry(RusBunkers, TEXT("Rus_BXP_MGBunker"), 2.f),
			MakeEntry(RusBunkers, TEXT("Rus_BXP_Bunker02_ZIS"), 1.f),
			MakeEntry(RusBunkers, TEXT("Rus_BXP_MegaBunker_1"), 0.5f)
		};
		FillDefaultLineWorks(Preset.Perimeter);
		Preset.Perimeter.ObstacleBelt.Formation = EEnemyObstacleFormation::DoubleRow;
		Preset.Perimeter.BarbedWire.Rows = 2;
		Preset.Perimeter.WireMinStrength = 0;
		Preset.GunPits = {
			MakeSlot(Bxps, TEXT("RusBS3Pit/Rus_BXP_BS3Pit"), 0, -1, 2, 0.5f),
			MakeSlot(Bxps, TEXT("RusMortarPit/Rus_BXP_120MortarPit"), 0, -1, 1, 0.5f),
			MakeSlot(Bxps, TEXT("RusAmpulometPit/Rus_BXP_AmpulometPit"), 0, -1, 1, 0.f),
			MakeSlot(Bxps, TEXT("RusDShKPit/Rus_BXP_DShKPit"), 0, -1, 1, 0.f),
			MakeSlot(RusBunkers, TEXT("Rus_BXP_Bofors"), 0, -1, 1, 0.5f)
		};
		Preset.DecoratorProfiles = {MakeSupplyDecorators()};
		Preset.Garrison = {
			MakeGarrison({Tank(ETankSubtype::Tank_KV_1), Tank(ETankSubtype::Tank_T34_85)}, 2, 1.f, 0,
			             ESurvivalSovietGarrisonBehaviour::HoldPosition),
			MakeGarrison({Tank(ETankSubtype::Tank_T34_76), Tank(ETankSubtype::Tank_T34_85)}, 3, 0.5f, 0,
			             ESurvivalSovietGarrisonBehaviour::Patrol),
			MakeGarrison({
				             Squad(ESquadSubtype::Squad_Rus_RedHammer), Squad(ESquadSubtype::Squad_Rus_RedHamerPTRS),
				             Squad(ESquadSubtype::Squad_Rus_ToxicGuard)
			             }, 3, 0.5f, 0, ESurvivalSovietGarrisonBehaviour::HoldPosition),
			MakeGarrison({Tank(ETankSubtype::Tank_T34_AA)}, 1, 0.5f, 0, ESurvivalSovietGarrisonBehaviour::HoldPosition),
			MakeGarrison({Tank(ETankSubtype::Tank_T34_85), Tank(ETankSubtype::Tank_SU_85)}, 2, 1.f, 0,
			             ESurvivalSovietGarrisonBehaviour::StrategicAIReserve)
		};
		Preset.DestroyRewards = {
			MakeReward(ERTSResourceType::Resource_Radixite, 500),
			MakeReward(ERTSResourceType::Resource_Metal, 500),
			MakeReward(ERTSResourceType::Resource_VehicleParts, 300)
		};
	}
}

void USurvivalSovietBaseSettings::FillDefaultDifficultyScaling()
{
	DifficultyScaling.Reset();
	auto AddRow = [this](const ERTSGameDifficulty Difficulty, const int32 StrengthBonus, const int32 MaxProductionTier,
	                     const float DefenseDensity, const float GunPitMultiplier, const float GarrisonMultiplier,
	                     const float RewardMultiplier)
	{
		FSurvivalSovietBaseDifficultyScaling& Row = DifficultyScaling.AddDefaulted_GetRef();
		Row.Difficulty = Difficulty;
		Row.StrengthBonus = StrengthBonus;
		Row.MaxProductionTier = MaxProductionTier;
		Row.DefenseDensity = DefenseDensity;
		Row.GunPitMultiplier = GunPitMultiplier;
		Row.GarrisonMultiplier = GarrisonMultiplier;
		Row.RewardMultiplier = RewardMultiplier;
	};
	AddRow(ERTSGameDifficulty::NewToRTS, -1, 0, 0.6f, 0.5f, 0.5f, 1.5f);
	AddRow(ERTSGameDifficulty::Normal, 0, 1, 1.f, 1.f, 1.f, 1.f);
	AddRow(ERTSGameDifficulty::Hard, 0, 2, 1.2f, 1.25f, 1.35f, 1.f);
	AddRow(ERTSGameDifficulty::Brutal, 1, 2, 1.4f, 1.5f, 1.7f, 0.9f);
	AddRow(ERTSGameDifficulty::Ironman, 1, 2, 1.5f, 1.75f, 2.f, 0.8f);
}

void USurvivalSovietBaseSettings::FillDefaultStrategicAITypes()
{
	StrategicAICoreBuildingTypes = {
		EBuildingExpansionType::BTX_RusBarracks,
		EBuildingExpansionType::BTX_RusFactory,
		EBuildingExpansionType::BTX_RusCoolingTowers,
		EBuildingExpansionType::BTX_RusResearchCenter,
		EBuildingExpansionType::BTX_RusPlatformFactory,
		EBuildingExpansionType::BTX_RusAssemblyYard
	};
	StrategicAISatelliteBuildingTypes = {
		EBuildingExpansionType::BTX_RusAmmoStorage,
		EBuildingExpansionType::BTX_RusFuelStorage1,
		EBuildingExpansionType::BTX_Rus_SuppliesStorage,
		EBuildingExpansionType::BTX_RusGuardTower,
		EBuildingExpansionType::BTX_RusBunkerMG,
		EBuildingExpansionType::BTX_RusDomeBunker,
		EBuildingExpansionType::BTX_RusLongCamoBunker,
		EBuildingExpansionType::BTX_RusMegaBunker,
		EBuildingExpansionType::BTX_Bunker02_ZIS,
		EBuildingExpansionType::BTX_Bunker05_WithTurrets,
		EBuildingExpansionType::BTX_Bunker05,
		EBuildingExpansionType::BTX_RusBoforsPosition
	};
}

#undef LOCTEXT_NAMESPACE
