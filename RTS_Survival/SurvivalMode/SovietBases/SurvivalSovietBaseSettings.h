// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansionEnums.h"
#include "RTS_Survival/SurvivalMode/SovietBases/SurvivalSovietBaseTypes.h"

#include "SurvivalSovietBaseSettings.generated.h"

/**
 * @brief How Soviet bases are generated on the start locations the player did not pick.
 *
 * Every unpicked start location gets one base. Its strength (0..MaxStrength) grows with its distance rank from
 * the player's start location (nearest = weakest) plus the difficulty's StrengthBonus. A preset allowed for that
 * strength is picked (seeded, weighted, avoiding repeats); the strength then gates the preset's slots.
 * @note The constructor fills working defaults (three presets, five difficulty rows) with verified asset paths,
 * so a freshly created asset is usable; ResetToDefaults restores them in the editor.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API USurvivalSovietBaseSettings : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	USurvivalSovietBaseSettings();

	/** @return The scaling row for the difficulty; the Normal row (or a default row) when it is missing. */
	FSurvivalSovietBaseDifficultyScaling GetScalingForDifficulty(ERTSGameDifficulty Difficulty) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soviet Bases|Presets")
	TArray<FSurvivalSovietBasePreset> Presets;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soviet Bases|Difficulty")
	TArray<FSurvivalSovietBaseDifficultyScaling> DifficultyScaling;

	// Strength levels are 0..MaxStrength.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soviet Bases|Difficulty", meta = (ClampMin = "0", ClampMax = "5"))
	int32 MaxStrength = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soviet Bases|Seed")
	ESurvivalSovietSeedMode SeedMode = ESurvivalSovietSeedMode::RandomPerMatch;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soviet Bases|Seed",
		meta = (EditCondition = "SeedMode == ESurvivalSovietSeedMode::Fixed"))
	int32 FixedSeed = 1941;

	// A preset already used by another base keeps this fraction of its weight (variety between bases).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soviet Bases|Seed", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RepeatedPresetWeightFactor = 0.25f;

	// Structures spawned per spawn step (every SpawnStepSeconds); spreads ~100 spawns per base over frames.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soviet Bases|Spawning", meta = (ClampMin = "1"))
	int32 StructuresPerSpawnStep = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soviet Bases|Spawning", meta = (ClampMin = "0.0"))
	float SpawnStepSeconds = 0.05f;

	// --- Strategic AI ---

	// Make the strategic AI see every Soviet base: merges the types below into the enemy controller's
	// FindEnemyBase_TimerRequest and raises MaxBasesToReturn / CoreClusterDistanceXY when needed.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soviet Bases|Strategic AI")
	bool bConfigureStrategicAIBaseDetection = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soviet Bases|Strategic AI",
		meta = (EditCondition = "bConfigureStrategicAIBaseDetection"))
	TArray<EBuildingExpansionType> StrategicAICoreBuildingTypes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soviet Bases|Strategic AI",
		meta = (EditCondition = "bConfigureStrategicAIBaseDetection"))
	TArray<EBuildingExpansionType> StrategicAISatelliteBuildingTypes;

	// Lower bound for CoreClusterDistanceXY; the generated layout raises it further when production buildings
	// ended up farther apart.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soviet Bases|Strategic AI",
		meta = (ClampMin = "500", EditCondition = "bConfigureStrategicAIBaseDetection"))
	float MinStrategicAICoreClusterDistance = 4000.f;

	// Upper bound for MinBaseScoreToReturn (score = buildings in the cluster; the request default is 100).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soviet Bases|Strategic AI",
		meta = (ClampMin = "1.0", EditCondition = "bConfigureStrategicAIBaseDetection"))
	float StrategicAIMaxMinBaseScore = 3.f;

	// Upper bound for MinTotalBuildingsPerBase (request default 4).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soviet Bases|Strategic AI",
		meta = (ClampMin = "1", EditCondition = "bConfigureStrategicAIBaseDetection"))
	int32 StrategicAIMaxMinBuildingsPerBase = 3;

	// --- Waves interaction (read by the waves director) ---

	// GetWavePressureMultiplier() = 1 + alive bases * this.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soviet Bases|Waves", meta = (ClampMin = "0.0"))
	float WavePressurePerAliveBase = 0.1f;

	// Notification shown when a base is destroyed; {0} = preset display name.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soviet Bases|Reward")
	FText BaseDestroyedNotification;

#if WITH_EDITOR
	/** @brief Restores the default presets, difficulty rows and AI building types. */
	UFUNCTION(CallInEditor, Category = "Soviet Bases")
	void ResetToDefaults();
#endif

private:
	void FillDefaultPresets();
	void FillDefaultDifficultyScaling();
	void FillDefaultStrategicAITypes();
};
