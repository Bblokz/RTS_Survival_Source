// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansionEnums.h"
#include "RTS_Survival/SurvivalMode/SovietBases/SurvivalSovietBaseTypes.h"

#include "SurvivalSovietBaseSettings.generated.h"

class UPCGGraphInterface;

/**
 * @brief How Soviet bases are generated on the start locations the player did not pick.
 *
 * Every unpicked start location gets one base. Its strength comes from the difficulty's StrengthByDistanceRank table
 * (nearest first). A preset allowed for that strength and tech cap is picked (seeded, weighted, avoiding repeats), and
 * the base then buys down the preset's ThreatPath until the difficulty's threat budget for that strength is spent.
 * @note All tables live in this asset. An asset that has none yet gets the code defaults at load; ResetToDefaults
 * writes them into the asset, and from then on the asset alone decides.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API USurvivalSovietBaseSettings : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	USurvivalSovietBaseSettings();

	virtual void PostLoad() override;

	/** @return True when this asset holds no tables of its own and the code defaults were filled in at load. */
	bool GetIsUsingCodeDefaults() const { return bM_UsingCodeDefaults; }

	/** @return The scaling row for the difficulty; the Normal row (or a default row) when it is missing. */
	FSurvivalSovietBaseDifficultyScaling GetScalingForDifficulty(ERTSGameDifficulty Difficulty) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soviet Bases|Presets")
	TArray<FSurvivalSovietBasePreset> Presets;

	// The PCG graph that generates a base (Site, then the Stage nodes, then Spawn). One graph, run once per base by a
	// PCG component at runtime; add nodes between a stage's Placed output and Spawn to filter what gets built.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soviet Bases|Presets")
	TSoftObjectPtr<UPCGGraphInterface> BaseGraph = TSoftObjectPtr<UPCGGraphInterface>(FSoftObjectPath(
		TEXT("/Game/RTS_Survival/SurvivalMode/PCG/SovietBases/PCG_SovietBase.PCG_SovietBase")));

	// Fences, gates, revetments, props and road surfaces every master plan builds with.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soviet Bases|Presets")
	FSurvivalSovietSiteKit Kit;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soviet Bases|Difficulty")
	TArray<FSurvivalSovietBaseDifficultyScaling> DifficultyScaling;

	// The build radius every base is planned in (cm); the plans scale with it. 0 uses each start location's own
	// BuildAreaRadius. 5 300 since the owner shrank the map (start discs flat to 5 700).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soviet Bases|Presets", meta = (ClampMin = "0.0"))
	float BuildAreaRadius = 5300.f;

	// Strength levels are 0..MaxStrength.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soviet Bases|Difficulty", meta = (ClampMin = "0", ClampMax = "5"))
	int32 MaxStrength = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soviet Bases|Seed")
	ESurvivalSovietSeedMode SeedMode = ESurvivalSovietSeedMode::RandomPerMatch;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soviet Bases|Seed",
		meta = (EditCondition = "SeedMode == ESurvivalSovietSeedMode::Fixed"))
	int32 FixedSeed = 1941;

	// Once every plan allowed for a strength is in use, a plan already used keeps this fraction of its weight.
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

	// --- Bases are a separate system: nothing outside it reads this ---

	// GetWavePressureMultiplier() = 1 + alive bases * this.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soviet Bases|Waves", meta = (ClampMin = "0.0"))
	float WavePressurePerAliveBase = 0.1f;

	// Share of a base's payout that its production buildings carry; the rest is earned by breaking its defences.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soviet Bases|Reward", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ProductionPayoutShare = 0.3f;

	// Fraction of a base's defence points that must be destroyed to earn the whole defence share of the payout.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soviet Bases|Reward", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float MinDefenseBrokenFraction = 0.6f;

	// Notification shown when a base is destroyed; {0} = preset display name.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soviet Bases|Reward")
	FText BaseDestroyedNotification;

#if WITH_EDITOR
	/** @brief Restores the default presets, difficulty rows and AI building types. */
	UFUNCTION(CallInEditor, Category = "Soviet Bases")
	void ResetToDefaults();
#endif

private:
	bool bM_UsingCodeDefaults = false;

	void FillAllDefaults();
	void FillDefaultPresets();
	void FillDefaultKit();
	void FillDefaultDifficultyScaling();
	void FillDefaultStrategicAITypes();
};
