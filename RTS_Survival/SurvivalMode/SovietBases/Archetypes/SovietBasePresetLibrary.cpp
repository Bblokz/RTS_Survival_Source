// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SovietBasePresetLibrary.h"

#include "RTS_Survival/Units/Enums/Enum_UnitType.h"

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
	const TCHAR* const Environment = TEXT("/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/");
	const TCHAR* const WalkableWalls =
		TEXT("/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Military/WalkableWall/");
	const TCHAR* const ConcreteWalls =
		TEXT("/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Military/ConcreteWall/");
	const TCHAR* const Missiles =
		TEXT("/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Military/MissileStacks/");
	const TCHAR* const DestroyedTanks =
		TEXT("/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Military/DestTanks/");
	const TCHAR* const SovietGuns = TEXT("/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/");
	const TCHAR* const GroundPaths = TEXT("/Game/RTS_Survival/Blueprints/Environment/Splines/GroundPath/");
	const TCHAR* const RusWallBuildings = TEXT("/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/Wall/");
	const TCHAR* const RusWalls = TEXT("/Game/RTS_Survival/Blueprints/Buildings/Walls/RusWalls/");

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

	FEnemyBlueprintEntry MakeEntry(const TCHAR* Folder, const TCHAR* AssetName, const float Weight)
	{
		FEnemyBlueprintEntry Entry;
		Entry.BlueprintClass = TSoftClassPtr<AActor>(MakeClassPath(Folder, AssetName));
		Entry.Weight = Weight;
		return Entry;
	}

	FSurvivalSovietStructureSlot MakeSlot(
		const ESurvivalSovietZone Zone,
		const TCHAR* Folder,
		const TCHAR* AssetName,
		const int32 MinStrength,
		const int32 ProductionTier,
		const int32 Count,
		const float CountPerStrength,
		const ERTSGameDifficulty MinDifficulty,
		const float CountPerDifficulty)
	{
		FSurvivalSovietStructureSlot Slot;
		Slot.Building = MakeEntry(Folder, AssetName);
		Slot.Zone = Zone;
		Slot.MinStrength = MinStrength;
		Slot.MinDifficulty = MinDifficulty;
		Slot.ProductionTier = ProductionTier;
		Slot.Count = Count;
		Slot.CountPerStrength = CountPerStrength;
		Slot.CountPerDifficulty = CountPerDifficulty;
		return Slot;
	}

	/** @brief An entry laid in a run; FrontAxis is the local axis across the run (see FSurvivalSovietSiteKit). */
	FEnemyBlueprintEntry MakeSegment(const TCHAR* Folder, const TCHAR* AssetName, const EEnemySandbagFrontAxis FrontAxis,
	                                 const float Weight)
	{
		FEnemyBlueprintEntry Entry = MakeEntry(Folder, AssetName, Weight);
		Entry.SandbagFrontAxis = FrontAxis;
		return Entry;
	}

	FTrainingOption Tank(const ETankSubtype Subtype)
	{
		return FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(Subtype));
	}

	FTrainingOption Squad(const ESquadSubtype Subtype)
	{
		return FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(Subtype));
	}

	FSurvivalSovietThreatStep Emplace(const ESurvivalSovietZone Zone, const TCHAR* Folder, const TCHAR* AssetName,
	                                  const int32 ThreatPoints, const int32 MaxCopies)
	{
		FSurvivalSovietThreatStep Step;
		Step.Kind = ESurvivalSovietStepKind::Emplacement;
		Step.Zone = Zone;
		Step.Building = MakeEntry(Folder, AssetName);
		Step.ThreatPoints = ThreatPoints;
		Step.MaxCopies = MaxCopies;
		return Step;
	}

	FSurvivalSovietThreatStep Bunker(const TCHAR* Folder, const TCHAR* AssetName, const int32 ThreatPoints,
	                                 const int32 MaxCopies)
	{
		FSurvivalSovietThreatStep Step;
		Step.Kind = ESurvivalSovietStepKind::LineBunker;
		Step.Building = MakeEntry(Folder, AssetName);
		Step.ThreatPoints = ThreatPoints;
		Step.MaxCopies = MaxCopies;
		return Step;
	}

	FSurvivalSovietThreatStep Garrison(const ESurvivalSovietStepKind Kind, const TArray<FTrainingOption>& Options,
	                                   const int32 MaxCopies)
	{
		FSurvivalSovietThreatStep Step;
		Step.Kind = Kind;
		Step.UnitOptions = Options;
		Step.MaxCopies = MaxCopies;
		return Step;
	}

	/** @return The step behind the gates of a difficulty, a strength and a production tier. */
	FSurvivalSovietThreatStep Gated(FSurvivalSovietThreatStep Step, const ERTSGameDifficulty MinDifficulty,
	                                const int32 MinStrength, const int32 RequiredProductionTier)
	{
		Step.MinDifficulty = MinDifficulty;
		Step.MinStrength = MinStrength;
		Step.RequiredProductionTier = RequiredProductionTier;
		return Step;
	}

	/** @return The step as its difficulty's mark on this archetype: every base of that difficulty buys one first. */
	FSurvivalSovietThreatStep Signature(FSurvivalSovietThreatStep Step, const int32 MinStrength)
	{
		Step.bSignature = true;
		Step.SignatureMinStrength = MinStrength;
		return Step;
	}

	/** @return The step only bought from this pass down the path on. */
	FSurvivalSovietThreatStep Later(FSurvivalSovietThreatStep Step, const int32 FirstPass)
	{
		Step.FirstPass = FirstPass;
		return Step;
	}

	FSurvivalSovietBaseReward MakeReward(const ERTSResourceType Resource, const float Share)
	{
		FSurvivalSovietBaseReward Reward;
		Reward.Resource = Resource;
		Reward.Share = Share;
		return Reward;
	}

	/** @brief Hedgehog belt in front of threatened runs. */
	FEnemyObstacleBeltSettings MakeDefaultBelt(const EEnemyObstacleFormation Formation)
	{
		FEnemyObstacleBeltSettings Belt;
		Belt.bEnabled = true;
		Belt.Obstacles = {
			MakeEntry(Barricades, TEXT("BP_Military_Dest_Hedgehog"), 2.f),
			MakeEntry(Barricades, TEXT("BP_Military_Dest_Hedgehog4"), 1.f),
			MakeEntry(Barricades, TEXT("BP_Military_Dest_LargeHedgehog"), 0.5f)
		};
		Belt.Formation = Formation;
		Belt.DistanceFromLine = 1100.f;
		Belt.SpacingBetweenObstacles = 520.f;
		Belt.RowSpacing = 340.f;
		return Belt;
	}
}

FSovietPresetLibrary::FSovietPresetLibrary()
{
	using namespace SurvivalSovietBaseDefaults;
	Tower = Emplace(EZone::Watch, RusBunkers, TEXT("Rus_BXP_GuardTowerMG"), 6, 6);
	Platform = Emplace(EZone::Watch, RusBunkers, TEXT("Rus_BXP_WeaponPlatform"), 15, 6);
	Pit53K = Emplace(EZone::AntiTank, Bxps, TEXT("Rus53KPit/Rus_BXP_53KPit"), 8, 8);
	PitSector = Emplace(EZone::AntiTank, Bxps, TEXT("Rus53KPit/Rus_BXP_53KSectorPit"), 8, 6);
	PitDShK = Emplace(EZone::AirDefense, Bxps, TEXT("RusDShKPit/Rus_BXP_DShKPit"), 8, 5);
	PitMortar = Gated(Emplace(EZone::Artillery, Bxps, TEXT("RusMortarPit/Rus_BXP_120MortarPit"), 12, 5), EDiff::Normal);
	PitAmpulomet = Gated(Emplace(EZone::AntiTank, Bxps, TEXT("RusAmpulometPit/Rus_BXP_AmpulometPit"), 10, 4), EDiff::Normal);
	PitBofors = Gated(Emplace(EZone::AirDefense, RusBunkers, TEXT("Rus_BXP_Bofors"), 15, 6), EDiff::Hard);
	PitBS3 = Gated(Emplace(EZone::AntiTank, Bxps, TEXT("RusBS3Pit/Rus_BXP_BS3Pit"), 22, 6), EDiff::Hard, 1);
	PitML20 = Gated(Emplace(EZone::Artillery, SovietGuns, TEXT("ML20_152MM/Pit/Rus_BXP_ML20Pit"), 38, 3), EDiff::Brutal, 1);
	PitB4 = Gated(Emplace(EZone::Artillery, SovietGuns, TEXT("B4_203MM/Pit/Rus_BXP_B4HowitzerPit"), 45, 3), EDiff::Ironman, 1);
	MGBunker = Bunker(RusBunkers, TEXT("Rus_BXP_MGBunker"), 8, 8);
	AutoCn = Gated(Bunker(RusBunkers, TEXT("Rus_BXP_AutoCnBunker"), 10, 5), EDiff::Normal);
	LongCamo = Gated(Bunker(RusBunkers, TEXT("Rus_BXP_LongCamoBunker"), 12, 5), EDiff::Normal);
	Dome = Gated(Bunker(RusBunkers, TEXT("Rus_BXP_DomeBunker"), 12, 5), EDiff::Hard);
	ZisBunker = Gated(Bunker(RusBunkers, TEXT("Rus_BXP_Bunker02_ZIS"), 15, 6), EDiff::Hard);
	Bunker03 = Gated(Bunker(RusBunkers, TEXT("Rus_BXP_RusBunker03"), 20, 6), EDiff::Brutal);
	Bunker05 = Gated(Bunker(RusBunkers, TEXT("Rus_BXP_Bunker05"), 28, 5), EDiff::Brutal, 1);
	MegaBunker = Gated(Bunker(RusBunkers, TEXT("Rus_BXP_MegaBunker_1"), 30, 4), EDiff::Ironman, 1);
	Rifles = {
		Squad(ESquadSubtype::Squad_Rus_Mosin), Squad(ESquadSubtype::Squad_Rus_Okhotnik), Squad(ESquadSubtype::Squad_Rus_Dyakonov)
	};
	Hunters = {Squad(ESquadSubtype::Squad_Rus_Okhotnik)};
	TankHunters = {Squad(ESquadSubtype::Squad_Rus_LargePTRS)};
	Assault = {Squad(ESquadSubtype::Squad_Rus_Mosin), Squad(ESquadSubtype::Squad_Rus_KorovinAssault)};
	ShockReserve = {Squad(ESquadSubtype::Squad_Rus_RedHammer)};
	ArmourReserve = {Tank(ETankSubtype::Tank_T34_76)};
	Guards = {
		Squad(ESquadSubtype::Squad_Rus_RedHammer), Squad(ESquadSubtype::Squad_Rus_RedHamerPTRS), Squad(ESquadSubtype::Squad_Rus_ToxicGuard)
	};
	Scouts = {Tank(ETankSubtype::Tank_Ba12)};
	LightTanks = {
		Tank(ETankSubtype::Tank_BT7), Tank(ETankSubtype::Tank_T26), Tank(ETankSubtype::Tank_T70), Tank(ETankSubtype::Tank_SU_76)
	};
	MediumTanks = {Tank(ETankSubtype::Tank_T34_76)};
	LateMediums = {Tank(ETankSubtype::Tank_T34_85), Tank(ETankSubtype::Tank_SU_85)};
	HeavyTanks = {Tank(ETankSubtype::Tank_KV_1)};
	Breakthrough = {Tank(ETankSubtype::Tank_KV_2), Tank(ETankSubtype::Tank_IS_1)};
	AATanks = {Tank(ETankSubtype::Tank_T34_AA), Tank(ETankSubtype::Tank_BTR152A)};

	Barracks = MakeSlot(EZone::Barracks, RusBuildings, TEXT("Rus_Building_Barracks"), 0, 0, 1, 0.f);
	Tent = MakeSlot(EZone::Barracks, RusBuildings, TEXT("Rus_Building_LargeTent"), 0, 0, 1, 0.f);
	Cooling = MakeSlot(EZone::Workshop, RusBuildings, TEXT("Rus_Building_CoolingTowers"), 0, 1, 1, 0.f);
	Factory = MakeSlot(EZone::Workshop, RusBuildings, TEXT("Rus_Building_Factory"), 0, 1, 1, 0.f);
	Research = MakeSlot(EZone::Command, RusBuildings, TEXT("Rus_Building_ResearchCenter"), 2, 2, 1, 0.f);
	Admin = MakeSlot(EZone::Command, RusBuildings, TEXT("Rus_Building_AdminBunker"), 1, 2, 1, 0.f);
	Ammo = MakeSlot(EZone::Depot, RusBuildings, TEXT("Rus_Building_AmmoStorage"), 0, -1, 1, 0.5f, EDiff::NewToRTS, 0.5f);
	Supplies = MakeSlot(EZone::Depot, RusBuildings, TEXT("Rus_Building_Supplies"), 0, -1, 1, 0.f, EDiff::NewToRTS, 0.25f);
	Speaker = MakeSlot(EZone::Depot, RusBuildings, TEXT("Rus_Bxp_Megaphone"), 0, -1, 1, 0.f);
	Fuel = MakeSlot(EZone::Fuel, RusBuildings, TEXT("Rus_Building_FuelStorage"), 0, -1, 1, 0.f, EDiff::NewToRTS, 0.25f);
	FuelTank = MakeSlot(EZone::Fuel, RusBuildings, TEXT("Rus_Building_FuelStorageTank"), 0, -1, 1, 0.5f, EDiff::Normal, 0.34f);
	InfantryReward = {
		MakeReward(ERTSResourceType::Resource_Radixite, 0.55f), MakeReward(ERTSResourceType::Resource_Metal, 0.45f)
	};
	ArmourReward = {
		MakeReward(ERTSResourceType::Resource_Radixite, 0.35f), MakeReward(ERTSResourceType::Resource_Metal, 0.35f),
		MakeReward(ERTSResourceType::Resource_VehicleParts, 0.3f)
	};
	CommandReward = {
		MakeReward(ERTSResourceType::Resource_Radixite, 0.4f), MakeReward(ERTSResourceType::Resource_Metal, 0.4f),
		MakeReward(ERTSResourceType::Resource_VehicleParts, 0.2f)
	};
}

FSurvivalSovietBasePreset& FSovietPresetLibrary::AddPreset(
	TArray<FSurvivalSovietBasePreset>& Presets,
	const FName Name,
	const FText& DisplayName,
	const ESurvivalSovietBaseArchetype Archetype,
	const int32 MinStrength,
	const int32 MaxStrength,
	const int32 MinTier) const
{
	FSurvivalSovietBasePreset& Preset = Presets.AddDefaulted_GetRef();
	Preset.PresetName = Name;
	Preset.DisplayName = DisplayName;
	Preset.Archetype = Archetype;
	Preset.MinStrength = MinStrength;
	Preset.MaxStrength = MaxStrength;
	Preset.MinProductionTier = MinTier;
	return Preset;
}

FSurvivalSovietThreatStep FSovietPresetLibrary::Hold(const TArray<FTrainingOption>& Options, const int32 MaxCopies) const
{
	return SurvivalSovietBaseDefaults::Garrison(ESurvivalSovietStepKind::HoldGarrison, Options, MaxCopies);
}

FSurvivalSovietThreatStep FSovietPresetLibrary::Patrol(const TArray<FTrainingOption>& Options, const int32 MaxCopies) const
{
	return SurvivalSovietBaseDefaults::Garrison(ESurvivalSovietStepKind::PatrolGarrison, Options, MaxCopies);
}

TArray<FSurvivalSovietThreatStep> FSovietPresetLibrary::Reserves() const
{
	using namespace SurvivalSovietBaseDefaults;
	return {
		Garrison(ESurvivalSovietStepKind::Reserve, Assault, 3),
		Gated(Garrison(ESurvivalSovietStepKind::Reserve, ShockReserve, 2), ERTSGameDifficulty::Brutal),
		Gated(Garrison(ESurvivalSovietStepKind::Reserve, ArmourReserve, 1), ERTSGameDifficulty::Ironman, 2, 2)
	};
}
