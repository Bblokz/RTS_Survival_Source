// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/SurvivalMode/SovietBases/SurvivalSovietBaseTypes.h"

namespace SurvivalSovietBaseDefaults
{
	// The build radius the plans are drawn in since the map shrink (start discs flat to 5 700).
	constexpr float BaseBuildAreaRadius = 5300.f;
	extern const TCHAR* const RusBuildings;
	extern const TCHAR* const RusBunkers;
	extern const TCHAR* const Bxps;
	extern const TCHAR* const Barricades;
	extern const TCHAR* const Sandbags;
	extern const TCHAR* const Airport;
	extern const TCHAR* const Military;
	extern const TCHAR* const Environment;
	extern const TCHAR* const WalkableWalls;
	extern const TCHAR* const ConcreteWalls;
	extern const TCHAR* const Missiles;
	extern const TCHAR* const DestroyedTanks;
	extern const TCHAR* const SovietGuns;
	extern const TCHAR* const GroundPaths;
	extern const TCHAR* const RusWallBuildings;
	extern const TCHAR* const RusWalls;

	FSoftObjectPath MakeClassPath(const TCHAR* Folder, const TCHAR* AssetPath);
	FEnemyBlueprintEntry MakeEntry(const TCHAR* Folder, const TCHAR* AssetName, float Weight = 1.f);
	FSurvivalSovietStructureSlot MakeSlot(ESurvivalSovietZone Zone, const TCHAR* Folder, const TCHAR* AssetName, int32 MinStrength,
	                                      int32 ProductionTier, int32 Count, float CountPerStrength,
	                                      ERTSGameDifficulty MinDifficulty = ERTSGameDifficulty::NewToRTS, float CountPerDifficulty = 0.f);
	FEnemyBlueprintEntry MakeSegment(const TCHAR* Folder, const TCHAR* AssetName, EEnemySandbagFrontAxis FrontAxis, float Weight = 1.f);
	FTrainingOption Tank(ETankSubtype Subtype);
	FTrainingOption Squad(ESquadSubtype Subtype);
	FSurvivalSovietThreatStep Emplace(ESurvivalSovietZone Zone, const TCHAR* Folder, const TCHAR* AssetName, int32 ThreatPoints, int32 MaxCopies);
	FSurvivalSovietThreatStep Bunker(const TCHAR* Folder, const TCHAR* AssetName, int32 ThreatPoints, int32 MaxCopies);
	FSurvivalSovietThreatStep Garrison(ESurvivalSovietStepKind Kind, const TArray<FTrainingOption>& Options, int32 MaxCopies);
	/** @return The step behind the gates of a difficulty, a strength and a production tier. */
	FSurvivalSovietThreatStep Gated(FSurvivalSovietThreatStep Step, ERTSGameDifficulty MinDifficulty, int32 MinStrength = 0, int32 RequiredProductionTier = 0);
	/** @return The step as its difficulty's mark on this archetype: every base of that difficulty buys one first. */
	FSurvivalSovietThreatStep Signature(FSurvivalSovietThreatStep Step, int32 MinStrength = 1);
	/** @return The step only bought from this pass down the path on. */
	FSurvivalSovietThreatStep Later(FSurvivalSovietThreatStep Step, int32 FirstPass);
	FSurvivalSovietBaseReward MakeReward(ERTSResourceType Resource, float Share);
	/** @brief Hedgehog belt in front of threatened runs. */
	FEnemyObstacleBeltSettings MakeDefaultBelt(EEnemyObstacleFormation Formation);
}

/**
 * @brief The shared price list, unit groups, building slots and rewards the base types build their presets from.
 * Every base type writes its own preset as an Add<Type> member in its Archetypes/SovietBaseArchetype_<Type>.cpp, so
 * nobody edits a shared file or the binary data asset to tune one type. A type's own one-off slot is a local copy.
 */
struct FSovietPresetLibrary
{
	using EZone = ESurvivalSovietZone;
	using EArch = ESurvivalSovietBaseArchetype;
	using EDiff = ERTSGameDifficulty;

	FSovietPresetLibrary();

	FSurvivalSovietThreatStep Tower;
	FSurvivalSovietThreatStep Platform;
	FSurvivalSovietThreatStep Pit53K;
	FSurvivalSovietThreatStep PitSector;
	FSurvivalSovietThreatStep PitDShK;
	FSurvivalSovietThreatStep PitMortar;
	FSurvivalSovietThreatStep PitAmpulomet;
	FSurvivalSovietThreatStep PitBofors;
	FSurvivalSovietThreatStep PitBS3;
	FSurvivalSovietThreatStep PitML20;
	FSurvivalSovietThreatStep PitB4;
	FSurvivalSovietThreatStep MGBunker;
	FSurvivalSovietThreatStep AutoCn;
	FSurvivalSovietThreatStep LongCamo;
	FSurvivalSovietThreatStep Dome;
	FSurvivalSovietThreatStep ZisBunker;
	FSurvivalSovietThreatStep Bunker03;
	FSurvivalSovietThreatStep Bunker05;
	FSurvivalSovietThreatStep MegaBunker;
	TArray<FTrainingOption> Rifles;
	TArray<FTrainingOption> Hunters;
	TArray<FTrainingOption> TankHunters;
	TArray<FTrainingOption> Assault;
	TArray<FTrainingOption> ShockReserve;
	TArray<FTrainingOption> ArmourReserve;
	TArray<FTrainingOption> Guards;
	TArray<FTrainingOption> Scouts;
	TArray<FTrainingOption> LightTanks;
	TArray<FTrainingOption> MediumTanks;
	TArray<FTrainingOption> LateMediums;
	TArray<FTrainingOption> HeavyTanks;
	TArray<FTrainingOption> Breakthrough;
	TArray<FTrainingOption> AATanks;
	FSurvivalSovietStructureSlot Barracks;
	FSurvivalSovietStructureSlot Tent;
	FSurvivalSovietStructureSlot Cooling;
	FSurvivalSovietStructureSlot Factory;
	FSurvivalSovietStructureSlot Research;
	FSurvivalSovietStructureSlot Admin;
	FSurvivalSovietStructureSlot Ammo;
	FSurvivalSovietStructureSlot Supplies;
	FSurvivalSovietStructureSlot Speaker;
	FSurvivalSovietStructureSlot Fuel;
	FSurvivalSovietStructureSlot FuelTank;
	TArray<FSurvivalSovietBaseReward> InfantryReward;
	TArray<FSurvivalSovietBaseReward> ArmourReward;
	TArray<FSurvivalSovietBaseReward> CommandReward;

	FSurvivalSovietBasePreset& AddPreset(TArray<FSurvivalSovietBasePreset>& Presets, FName Name, const FText& DisplayName,
	                                     ESurvivalSovietBaseArchetype Archetype, int32 MinStrength, int32 MaxStrength, int32 MinTier) const;
	FSurvivalSovietThreatStep Hold(const TArray<FTrainingOption>& Options, int32 MaxCopies) const;
	FSurvivalSovietThreatStep Patrol(const TArray<FTrainingOption>& Options, int32 MaxCopies) const;
	TArray<FSurvivalSovietThreatStep> Reserves() const;

	// One per base type, each in its own file.
	void AddForwardFirebase(TArray<FSurvivalSovietBasePreset>& Presets) const;
	void AddRifleCantonment(TArray<FSurvivalSovietBasePreset>& Presets) const;
	void AddFieldAirstrip(TArray<FSurvivalSovietBasePreset>& Presets) const;
	void AddTankPark(TArray<FSurvivalSovietBasePreset>& Presets) const;
	void AddSupplyDepot(TArray<FSurvivalSovietBasePreset>& Presets) const;
	void AddHillStronghold(TArray<FSurvivalSovietBasePreset>& Presets) const;
	void AddCommandFortress(TArray<FSurvivalSovietBasePreset>& Presets) const;
};
