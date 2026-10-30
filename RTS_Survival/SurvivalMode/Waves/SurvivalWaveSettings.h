// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RTS_Survival/Enemy/EnemyAIBehaviour/EnemyAIBehaviour.h"
#include "RTS_Survival/SurvivalMode/Waves/SurvivalWaveTypes.h"

#include "SurvivalWaveSettings.generated.h"

class UStochasticDecisionTree;

// Set to 1 once AEnemyController::OverrideStrategicDecisionTree (see Docs/40_WavesAndAI.md, Integration requests)
// exists; until then the survival map's AEnemyController keeps its map-authored decision tree.
#ifndef RTS_SURVIVAL_WAVES_WITH_TREE_OVERRIDE
#define RTS_SURVIVAL_WAVES_WITH_TREE_OVERRIDE 1
#endif

/**
 * @brief When the strategic AI of the survival map starts thinking.
 */
UENUM(BlueprintType)
enum class ESurvivalStrategicAIStartMoment : uint8
{
	// Soviet bases react (train, defend) from the moment the grace countdown starts.
	GracePeriodStart,
	// Nothing happens on the Soviet side until the first wave.
	WavesStart
};

/**
 * @brief How the regular enemy strategic AI is configured on a survival map so it coexists with the waves.
 * Ownership rule: units in a wave formation belong to the wave director; units on the idle blackboard
 * (wave survivors after arrival, Soviet base garrisons, drops) belong to the strategic AI.
 */
USTRUCT(BlueprintType)
struct FSurvivalStrategicAISettings
{
	GENERATED_BODY()

	// Off: the wave director never touches the enemy controller's strategic AI.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Strategic AI")
	bool bConfigureStrategicAI = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Strategic AI")
	ESurvivalStrategicAIStartMoment StartMoment = ESurvivalStrategicAIStartMoment::GracePeriodStart;

	// While false during the grace period, the decision tree does not send idle units at the freshly landed player.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Strategic AI")
	bool bAllowDirectControlDuringGrace = false;

	// Soviet bases train reinforcements from their trainers with the profile's training income.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Strategic AI")
	bool bAllowUnitTraining = true;

	// Training points set when the strategic AI is configured (10 = one light tank).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Strategic AI", meta = (ClampMin = "0"))
	int32 TrainingPointsAtStart = 20;

	// How often the training income and unit-pick limits follow the difficulty profile curves.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Strategic AI", meta = (ClampMin = "1.0"))
	float ProfileUpdateIntervalSeconds = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Strategic AI")
	EEnemyStrategicAIBehaviour Behaviour = EEnemyStrategicAIBehaviour::PreferAggressive;

	// On defeat the decision tree and training stop so the result screen is not shown over a still-running AI.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Strategic AI")
	bool bStopStrategicAIOnDefeat = true;

	// Replaces the map-authored tree on every survival map (needs RTS_SURVIVAL_WAVES_WITH_TREE_OVERRIDE).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Strategic AI|Decision Tree")
	bool bOverrideDecisionTree = true;

	// The survival decision tree; see Docs/40_WavesAndAI.md for the action/sub-action layout and scores.
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Strategic AI|Decision Tree",
		meta = (EditCondition = "bOverrideDecisionTree"))
	TObjectPtr<UStochasticDecisionTree> DecisionTreeTemplate = nullptr;
};

/**
 * @brief How long a wave (round) lasts and when it counts as survived.
 */
USTRUCT(BlueprintType)
struct FSurvivalRoundSettings
{
	GENERATED_BODY()

	// Seconds the first round keeps sending groups.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rounds", meta = (ClampMin = "10.0"))
	float FirstRoundActiveSeconds = 60.f;

	// Every later round sends groups this much longer: waves get longer as the game goes on.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rounds", meta = (ClampMin = "0.0"))
	float ActiveSecondsPerRound = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rounds", meta = (ClampMin = "10.0"))
	float MaxRoundActiveSeconds = 420.f;

	// Scales the difficulty's wave interval curve to the time between two groups of one round.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rounds", meta = (ClampMin = "0.05"))
	float GroupIntervalScale = 0.45f;

	// After the active time the round still counts as survived once this much time passed, even if a few of
	// its units are stuck somewhere; they keep fighting.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rounds", meta = (ClampMin = "10.0"))
	float ClearTimeoutSeconds = 180.f;

	// Units that appear this close to a just-launched group's spawn points belong to that round.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rounds", meta = (ClampMin = "100.0"))
	float AttributionRadius = 2500.f;

	// How long after launch a group's units can still appear (the warning lead plus async spawning).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rounds", meta = (ClampMin = "1.0"))
	float AttributionWindowSeconds = 45.f;
};

/**
 * @brief Soviet wave growth per difficulty, compositions per tech stage, special waves, edge selection,
 * retargeting, the alive-unit cap and the strategic AI configuration of a survival map.
 * A freshly created asset already holds the defaults documented in Docs/40_WavesAndAI.md / wave_defaults.json.
 * @note Referenced by USurvivalModeSettings::WaveSettings and read by USurvivalWaveDirectorComponent.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API USurvivalWaveSettings : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	USurvivalWaveSettings();

	/** @return The profile of the difficulty, else the Normal profile, else a built-in Normal default. */
	const FSurvivalWaveDifficultyProfile& GetProfileForDifficulty(const ERTSGameDifficulty Difficulty) const;

	/**
	 * @brief Unlock minute of a tech stage after the difficulty's pace is applied.
	 * @param TechLevel Stage to look up.
	 * @param TechPaceMultiplier Profile pace; larger reaches the stage sooner.
	 * @return Minute from which the stage is unlocked; a stage missing from TechStageUnlocks never unlocks.
	 */
	float GetTechStageUnlockMinute(const EEnemyAITechLevel TechLevel, const float TechPaceMultiplier) const;

	/** @return The text configured for the stage, empty when none. */
	FText GetTechStageAnnouncement(const EEnemyAITechLevel TechLevel) const;

	/** @return Curve value at the minute, Fallback when the curve has no keys. */
	static float EvaluateCurveAtMinute(const FRuntimeFloatCurve& Curve, const float Minute, const float Fallback);

	// ---------------------------------------------------------------- Timing

	// Seconds after StartWaves (end of the grace period) before the first wave is launched.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Timing", meta = (ClampMin = "0.0"))
	float FirstWaveDelaySeconds = 5.f;

	// Waves are rounds: active time that grows per round, then clearing, then an intermission with a reward.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rounds")
	FSurvivalRoundSettings Rounds;

	// The HUD is warned this long before the wave's units appear at the edges.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Timing", meta = (ClampMin = "0.0"))
	float WarningLeadSeconds = 10.f;

	// Each interval is randomised by +- this fraction.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Timing", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float WaveIntervalVarianceFraction = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Timing", meta = (ClampMin = "0.1"))
	float SchedulerTickSeconds = 1.f;

	// ---------------------------------------------------------------- Difficulty

	// One profile per ERTSGameDifficulty; a missing difficulty uses the Normal profile.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Difficulty")
	TArray<FSurvivalWaveDifficultyProfile> DifficultyProfiles;

	// Optionally scale every budget by DifficultyPercentage / ReferenceDifficultyPercentage (DefaultGame.ini levels).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Difficulty")
	bool bScaleBudgetByDifficultyPercentage = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Difficulty",
		meta = (ClampMin = "1", EditCondition = "bScaleBudgetByDifficultyPercentage"))
	int32 ReferenceDifficultyPercentage = 100;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Difficulty",
		meta = (EditCondition = "bScaleBudgetByDifficultyPercentage"))
	FVector2D DifficultyPercentageScaleClamp = FVector2D(0.5, 2.0);

	// ---------------------------------------------------------------- Units

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Units")
	TArray<FSurvivalTechStageUnlock> TechStageUnlocks;

	// Only units verified to spawn through the async spawner (unit roster test) are in the default pool.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Units")
	TArray<FSurvivalWaveUnitEntry> UnitPool;

	// A freshly unlocked stage starts at NewTechStageStartWeightFraction of its weight and ramps to full.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Units", meta = (ClampMin = "0.0"))
	float NewTechStageRampInMinutes = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Units", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float NewTechStageStartWeightFraction = 0.25f;

	// Each wave's budget is randomised by +- this fraction.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Units", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float BudgetVarianceFraction = 0.1f;

	// When the unit cap of a wave is hit, up to this many cheap units are swapped for pricier ones.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Units", meta = (ClampMin = "0"))
	int32 UpgradePassMaxReplacements = 16;

	// ---------------------------------------------------------------- Wave kinds

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Kinds")
	FSurvivalWaveKindDefinition RegularWave;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Kinds")
	FSurvivalWaveKindDefinition MilestoneWave;

	// The first wave launched after every multiple of this many survival minutes is a milestone; 0 disables.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Kinds", meta = (ClampMin = "0.0"))
	float MilestoneEveryMinutes = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Kinds")
	TArray<FSurvivalWaveKindDefinition> SpecialWaves;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Kinds", meta = (ClampMin = "0"))
	int32 MinWavesBetweenSpecials = 2;

	// Once this many waves passed without a special (and one is allowed), the next wave is forced special.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Kinds", meta = (ClampMin = "1"))
	int32 MaxWavesBetweenSpecials = 6;

	// ---------------------------------------------------------------- Edges

	// Edges closer than this to the base are skipped (unless no other edge is eligible).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Edges", meta = (ClampMin = "0.0"))
	float MinEdgeDistanceFromBase = 8000.f;

	// Edges used by the last N waves get RecentEdgeWeightMultiplier.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Edges", meta = (ClampMin = "0"))
	int32 RecentEdgeMemory = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Edges", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RecentEdgeWeightMultiplier = 0.35f;

	// 0: extra edges are independent picks; 1: extra edges strongly prefer directions away from those picked.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Edges", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EdgeAngularSpreadBias = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Edges", meta = (ClampMin = "200.0"))
	float SpawnPointSpacing = 700.f;

	// First waypoint: this far inward from the spawn line, so formations leave the map edge before pathing.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Edges", meta = (ClampMin = "0.0"))
	float StagingDistance = 2500.f;

	// ---------------------------------------------------------------- Movement

	// Larger edge groups are split into several formations of at most this many units.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Movement", meta = (ClampMin = "1"))
	int32 MaxUnitsPerFormation = 12;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Movement", meta = (ClampMin = "1"))
	int32 MinFormationWidth = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Movement", meta = (ClampMin = "1"))
	int32 MaxFormationWidth = 5;

	// Each formation aims at a random point within this radius of the base so groups do not stack.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Movement", meta = (ClampMin = "0.0"))
	float FinalWaypointJitterRadius = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Movement", meta = (ClampMin = "0.1"))
	float FormationOffsetMultiplier = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Movement", meta = (ClampMin = "0.1"))
	float HelpOffsetRadiusMltMax = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Movement", meta = (ClampMin = "0.1"))
	float HelpOffsetRadiusMltMin = 1.f;

	// Must be > 0: an attack-move formation otherwise holds a waypoint forever while any unit fights.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Movement", meta = (ClampMin = "1.0"))
	float MaxAttackTimeBeforeAdvancingToNextWayPoint = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Movement", meta = (ClampMin = "1"))
	int32 MaxTriesFindNavPointForHelpOffset = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Movement", meta = (ClampMin = "0.1"))
	float HelpProjectionScale = 1.f;

	// Nav path from the staging point to the base is resampled to waypoints this far apart.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Movement", meta = (ClampMin = "500.0"))
	float AveragePathPointDistance = 8000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Movement", meta = (ClampMin = "0"))
	int32 MaxIntermediatePathPoints = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Movement", meta = (ClampMin = "0.0"))
	float MinimumFinalSegmentDistance = 2000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Movement", meta = (ClampMin = "0.1"))
	float PathProjectionScale = 3.f;

	// ---------------------------------------------------------------- Retargeting

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Retarget", meta = (ClampMin = "0.5"))
	float RetargetCheckIntervalSeconds = 2.f;

	// A formation is re-aimed once the base moved this far from the point it was sent to.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Retarget", meta = (ClampMin = "500.0"))
	float RetargetDistance = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Retarget", meta = (ClampMin = "0.0"))
	float RetargetCooldownSeconds = 12.f;

	// A launched group whose formation never appeared within this time is dropped from tracking.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Retarget", meta = (ClampMin = "10.0"))
	float FormationBindTimeoutSeconds = 120.f;

	// ---------------------------------------------------------------- Performance

	// Alive enemy load = tanks + soldiers * SoldierLoadWeight (all Soviet units, not only waves).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Performance", meta = (ClampMin = "0.0"))
	float SoldierLoadWeight = 0.25f;

	// Load a planned squad is assumed to add before it exists.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Performance", meta = (ClampMin = "0.0"))
	float PlannedSquadLoad = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Performance", meta = (ClampMin = "0.0"))
	float PlannedTankLoad = 1.f;

	// Global multiplier on every profile's AliveEnemyLoadCap (tune for low-end hardware).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Performance", meta = (ClampMin = "0.1"))
	float PerformanceLoadCapMultiplier = 1.f;

	// A wave that would have fewer units after trimming to the cap waits instead.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Performance", meta = (ClampMin = "1"))
	int32 MinUnitsToLaunch = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Performance", meta = (ClampMin = "1.0"))
	float CapRetrySeconds = 8.f;

	// ---------------------------------------------------------------- Strategic AI

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves|Strategic AI")
	FSurvivalStrategicAISettings StrategicAI;

private:
	void InitDefaultDifficultyProfiles();
	void InitDefaultTechStages();
	void InitDefaultUnitPool();
	void InitDefaultWaveKinds();
};
