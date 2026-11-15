#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"


#include "AttackWave.generated.h"

USTRUCT(Blueprintable)
struct FAttackWaveElement
{
	GENERATED_BODY()
	// Where this element spawns.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector SpawnLocation = FVector::ZeroVector;

	// the types of units that can be spawned for this wave element.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FTrainingOption> UnitOptions = {};
};

UENUM(Blueprintable)
enum class EEnemyWaveType :uint8
{
	Wave_None,
	// The wave will spawn units and does not depend on some building.
	Wave_NoOwningBuilding,
	// The wave will spawn units and depend on a building that is owned by the enemy.
	Wave_OwningBuilding,
	// The wave will spawn units and depend on a building that is owned by the enemy and has power generators
	// Which when destroyed slow down the wave interval.
	Wave_OwningBuildingAndPowerGenerators
};

USTRUCT(Blueprintable)
struct FAttackMoveWaveSettings
{
	GENERATED_BODY()

	// Multiplier applied to the unit inner radius for the max help offset.
	// Higher pushes units that help an ally in combat further away from that ally.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Move", meta = (ClampMin = "0.01"))
	float HelpOffsetRadiusMltMax = 1.f;

	// Multiplier applied to the unit inner radius for the min help offset.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Move", meta = (ClampMin = "0.01"))
	float HelpOffsetRadiusMltMin = 1.f;

	// Max time a unit can keep attacking after reaching a waypoint before advancing.
	// <= 0: the formation never advances while any of its units is in combat.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Move")
	float MaxAttackTimeBeforeAdvancingToNextWayPoint = 0.f;

	// How many times to try to project a help offset location per tick.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Move", meta = (ClampMin = "1"))
	int32 MaxTriesFindNavPointForHelpOffset = 5;

	// Scale applied to RTSToNavProjectionExtent for help offset projections.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Move", meta = (ClampMin = "0.01"))
	float ProjectionScale = 1.f;
};

USTRUCT(Blueprintable)
struct FRandomPatrolWithAttackMoveSettings
{
	GENERATED_BODY()

	// To pick the first patrol point directly instead of randomizing the first one (-1 = random).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol", meta = (ClampMin = "-1"))
	int32 OverrideFirstPatrolPointIndex = INDEX_NONE;

	// Amount of guard iterations that are executed once the patrol destination is reached.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol", meta = (ClampMin = "0"))
	int32 AmountIterationsAtPatrolPoint = 0;

	// Time in seconds between guard iterations while the patrol is holding position.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol", meta = (ClampMin = "0.0"))
	float GuardTimePerPatrolPointIteration = 0.f;

	// Radius around the current patrol point for guard iteration movement projections.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol", meta = (ClampMin = "0.0"))
	float GuardSphereRadius = 0.f;

	// Attack move logic that keeps patrol units supporting units in combat before advancing.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol")
	FAttackMoveWaveSettings AttackMoveSettings = {};
};

// Units of one completed wave iteration, after they were handed to their formation.
DECLARE_DELEGATE_TwoParams(FOnAttackWaveUnitsSpawned, const int32 /*WaveID*/, const TArray<AActor*>& /*Units*/);
// The wave was removed: its iterations ran out, its owning building died, or it was stopped.
DECLARE_DELEGATE_OneParam(FOnAttackWaveEnded, const int32 /*WaveID*/);

// How the units of a wave move once an iteration has spawned.
enum class EAttackWaveMovement : uint8
{
	// Plain formation move along the waypoints.
	Formation,
	// Formation move along the waypoints that stops to help allies in combat.
	AttackMove,
	// Formation that patrols random waypoints, guarding each, and helps allies in combat.
	RandomPatrolWithAttackMove
};

/**
 * @brief Every setting of one wave in one value, for callers that configure waves from data (AEnemyWaveSpawner)
 * instead of through the fixed CreateXWave signatures. Started with AEnemyController::StartWaveFromRequest.
 */
struct FAttackWaveRequest
{
	EEnemyWaveType WaveType = EEnemyWaveType::Wave_NoOwningBuilding;
	TArray<FAttackWaveElement> WaveElements;

	// INDEX_NONE repeats forever; 1 is a single wave; N runs N iterations (skipped iterations count too).
	int32 IterationCount = 1;
	// Seconds before the first iteration; <= 0 starts it right away.
	float FirstIterationDelay = 0.f;
	// Seconds between iterations (unused for single waves).
	float WaveInterval = 60.f;
	float IntervalVarianceFraction = 0.f;

	EAttackWaveMovement Movement = EAttackWaveMovement::AttackMove;
	TArray<FVector> Waypoints;
	FRotator FinalWaypointDirection = FRotator::ZeroRotator;
	int32 MaxFormationWidth = 2;
	float FormationOffsetMultiplier = 1.f;
	FAttackMoveWaveSettings AttackMoveSettings;
	FRandomPatrolWithAttackMoveSettings PatrolSettings;

	TWeakObjectPtr<AActor> WaveCreator = nullptr;
	TArray<TWeakObjectPtr<AActor>> WaveTimerAffectingBuildings;
	float PerAffectingBuildingTimerFraction = 0.f;

	FOnAttackWaveUnitsSpawned OnUnitsSpawned;
	FOnAttackWaveEnded OnWaveEnded;
};

USTRUCT()
struct FAttackWave
{
	GENERATED_BODY()

	void ResetForSpawningNewWave()
	{
		AwaitingSpawnsTillStartMoving = 0;
		SpawnedWaveUnits.Empty();
	}

	EEnemyWaveType WaveType = EEnemyWaveType::Wave_None;
	bool bIsSingleWave = false;

	// Defines a unit type and spawn point.
	TArray<FAttackWaveElement> WaveElements = {};

	// The actor that needs to be RTS valid for the wave to be able to spawn.
	TWeakObjectPtr<AActor> WaveCreator = nullptr;

	// Other types of buildings/units that influence the wave spawn timing;
	// When destroyed they slow down the wave interval.
	TArray<TWeakObjectPtr<AActor>> WaveTimerAffectingBuildings = {};

	// 1 + sum of affecting buildings fractions will be multiplied with the wave timing.
	float PerAffectingBuildingTimerFraction = 0.1f;

	FTimerHandle WaveTimerHandle;
	FTimerDelegate WaveDelegate;

	float WaveInterval = 10.f;
	// used in 1 + fraction and 1 - fraction to mlt with the wave interval.
	float IntervalVarianceFraction = 0.2;

	TArray<FVector> Waypoints = {};
	FRotator FinalWaypointDirection = FRotator::ZeroRotator;
	// The maximum width of the formation that is spawned.
	int32 MaxFormationWidth = 2;

	int32 UniqueWaveID = -1;

	// How many units still need to be spawned on this iteration to complete the wave and start moving.
	int32 AwaitingSpawnsTillStartMoving = 0;

	// How much the offset is multiplied by when calculating the unit's formation position.
	float FormationOffsetMlt = 1.f;

	// Attack move settings used for attack move waves.
	FAttackMoveWaveSettings AttackMoveSettings = {};

	// When true the wave is handled as an attack move wave.
	bool bIsAttackMoveWave = false;

	// When true the wave starts as a random patrol with attack move combat support.
	bool bIsRandomPatrolWithAttackMoveWave = false;

	// Settings used when bIsRandomPatrolWithAttackMoveWave is true.
	FRandomPatrolWithAttackMoveSettings RandomPatrolWithAttackMoveSettings = {};

	// Iterations left before the wave removes itself; INDEX_NONE repeats forever. Ignored for single waves.
	int32 RemainingIterations = INDEX_NONE;

	// Optional callbacks for callers that need the spawned units or the end of the wave (wave spawner actors).
	FOnAttackWaveUnitsSpawned OnUnitsSpawned;
	FOnAttackWaveEnded OnWaveEnded;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> SpawnedWaveUnits;
};
