// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Enemy/EnemyWaves/AttackWave.h"
#include "RTS_Survival/GameUI/MiniMap/CustomIcons/MinimapIconTypes.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"

#include "EnemyWaveSpawnerTypes.generated.h"

// What the units of a wave spawner do once spawned; also picks the editor colour of the spawner.
UENUM(BlueprintType)
enum class EEnemyWaveSpawnerBehaviour : uint8
{
	// Attack-move along the waypoints in order; at the last one the units go to the enemy AI's direct control.
	AttackWave UMETA(DisplayName = "Attack Wave"),
	// Patrol the waypoints (picked at random), guard each one and help allies in combat; never handed over.
	PatrolWave UMETA(DisplayName = "Patrol Wave")
};

UENUM(BlueprintType)
enum class EEnemyWaveSpawnerRepeat : uint8
{
	Once UMETA(DisplayName = "Once"),
	FixedCount UMETA(DisplayName = "Fixed Count"),
	Infinite UMETA(DisplayName = "Infinite")
};

/** One kind of unit in a wave; each of the Count units picks one of the options at random. */
USTRUCT(BlueprintType)
struct FEnemyWaveSpawnerUnitEntry
{
	GENERATED_BODY()

	// Each spawned unit picks one of these at random (seeded by the campaign).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit")
	TArray<FTrainingOption> UnitOptions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit", meta = (ClampMin = "1", UIMin = "1"))
	int32 Count = 1;

	// Spawn point these units appear at; -1 spreads them over all spawn points in turn.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Unit", meta = (ClampMin = "-1"))
	int32 SpawnPointIndex = INDEX_NONE;
};

/**
 * Units of one wave per difficulty. The lists add up: Hard spawns Base + Normal extras + Hard extras.
 * Ironman spawns the same as Brutal.
 */
USTRUCT(BlueprintType)
struct FEnemyWaveSpawnerUnits
{
	GENERATED_BODY()

	// Spawned on every difficulty (New To RTS and up).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Units", DisplayName = "Base Units (New To RTS +)")
	TArray<FEnemyWaveSpawnerUnitEntry> BaseUnits;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Units", DisplayName = "Extra Units (Normal +)")
	TArray<FEnemyWaveSpawnerUnitEntry> ExtraUnitsNormal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Units", DisplayName = "Extra Units (Hard +)")
	TArray<FEnemyWaveSpawnerUnitEntry> ExtraUnitsHard;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Units", DisplayName = "Extra Units (Brutal + Ironman)")
	TArray<FEnemyWaveSpawnerUnitEntry> ExtraUnitsBrutal;
};

/** When the spawner starts and how often its wave comes. */
USTRUCT(BlueprintType)
struct FEnemyWaveSpawnerTiming
{
	GENERATED_BODY()

	// Off: the spawner waits until a mission calls StartSpawner (find it with FindEnemyWaveSpawner).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
	bool bStartOnBeginPlay = true;

	// Seconds from StartSpawner (or BeginPlay) to the first wave.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "0.0"))
	float FirstWaveDelay = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
	EEnemyWaveSpawnerRepeat Repeat = EEnemyWaveSpawnerRepeat::Once;

	// Total waves, the first one included. A wave skipped for lack of enemy wave supply also counts.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing",
		meta = (ClampMin = "2", UIMin = "2", EditCondition = "Repeat == EEnemyWaveSpawnerRepeat::FixedCount",
			EditConditionHides))
	int32 WaveCount = 3;

	// Seconds between waves.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing",
		meta = (ClampMin = "1.0", EditCondition = "Repeat != EEnemyWaveSpawnerRepeat::Once", EditConditionHides))
	float WaveInterval = 180.f;

	// Each interval is randomised by +/- this fraction (0.2 => 80% - 120% of WaveInterval).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing",
		meta = (ClampMin = "0.0", ClampMax = "0.95", EditCondition = "Repeat != EEnemyWaveSpawnerRepeat::Once",
			EditConditionHides))
	float IntervalVarianceFraction = 0.1f;
};

/**
 * Ties the spawner to enemy buildings, exactly like enemy controller waves: with an owning building the wave stops
 * for good once that building is destroyed; power generators stretch the interval for each destroyed one.
 */
USTRUCT(BlueprintType)
struct FEnemyWaveSpawnerOwnership
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ownership")
	EEnemyWaveType WaveType = EEnemyWaveType::Wave_NoOwningBuilding;

	// The wave spawns only while this building stands.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ownership",
		meta = (EditCondition = "WaveType != EEnemyWaveType::Wave_NoOwningBuilding", EditConditionHides))
	TObjectPtr<AActor> OwningBuilding = nullptr;

	// Each destroyed generator adds PerDestroyedGeneratorIntervalFraction x WaveInterval to the next interval.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ownership",
		meta = (EditCondition = "WaveType == EEnemyWaveType::Wave_OwningBuildingAndPowerGenerators",
			EditConditionHides))
	TArray<TObjectPtr<AActor>> PowerGenerators;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ownership",
		meta = (ClampMin = "0.01", EditCondition = "WaveType == EEnemyWaveType::Wave_OwningBuildingAndPowerGenerators",
			EditConditionHides))
	float PerDestroyedGeneratorIntervalFraction = 0.1f;
};

USTRUCT(BlueprintType)
struct FEnemyWaveSpawnerFormation
{
	GENERATED_BODY()

	// Most units next to each other in one formation row.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxFormationWidth = 3;

	// Scales the spacing between units in the formation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation", meta = (ClampMin = "0.1"))
	float FormationOffsetMultiplier = 1.f;
};

/** Settings only an attack wave uses. */
USTRUCT(BlueprintType)
struct FEnemyWaveSpawnerAttackSettings
{
	GENERATED_BODY()

	FEnemyWaveSpawnerAttackSettings()
	{
		CombatSupport.HelpOffsetRadiusMltMax = 1.2f;
		CombatSupport.HelpOffsetRadiusMltMin = 1.f;
		CombatSupport.MaxAttackTimeBeforeAdvancingToNextWayPoint = 30.f;
		CombatSupport.MaxTriesFindNavPointForHelpOffset = 3;
		CombatSupport.ProjectionScale = 1.f;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	FEnemyWaveSpawnerFormation Formation;

	// On: the formation faces along the last leg of the path at the final waypoint.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	bool bFaceAlongLastLeg = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack",
		meta = (EditCondition = "!bFaceAlongLastLeg", EditConditionHides))
	FRotator FinalWaypointDirection = FRotator::ZeroRotator;

	// How the formation helps its units in combat and how long it lingers at a waypoint while fighting.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	FAttackMoveWaveSettings CombatSupport;
};

/** Settings only a patrol wave uses. */
USTRUCT(BlueprintType)
struct FEnemyWaveSpawnerPatrolSettings
{
	GENERATED_BODY()

	FEnemyWaveSpawnerPatrolSettings()
	{
		Patrol.AmountIterationsAtPatrolPoint = 2;
		Patrol.GuardTimePerPatrolPointIteration = 12.f;
		Patrol.GuardSphereRadius = 800.f;
		Patrol.AttackMoveSettings.HelpOffsetRadiusMltMax = 1.2f;
		Patrol.AttackMoveSettings.HelpOffsetRadiusMltMin = 1.f;
		Patrol.AttackMoveSettings.MaxAttackTimeBeforeAdvancingToNextWayPoint = 0.f;
		Patrol.AttackMoveSettings.MaxTriesFindNavPointForHelpOffset = 3;
		Patrol.AttackMoveSettings.ProjectionScale = 1.f;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol")
	FEnemyWaveSpawnerFormation Formation;

	// Guarding at each patrol point and how the patrol helps its units in combat.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol")
	FRandomPatrolWithAttackMoveSettings Patrol;
};

/** Shows each spawned wave on the minimap as one group icon that follows its living units. */
USTRUCT(BlueprintType)
struct FEnemyWaveSpawnerMinimapSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap")
	bool bShowOnMinimap = false;

	// Turns with the group's heading (the survival wave chevron by default).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap",
		meta = (EditCondition = "bShowOnMinimap", EditConditionHides))
	EMinimapIconType HeadingIcon = EMinimapIconType::IncomingWave;

	// Drawn upright on top of the heading icon; None draws no badge.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap",
		meta = (EditCondition = "bShowOnMinimap", EditConditionHides))
	EMinimapIconType BadgeIcon = EMinimapIconType::IncomingWaveBadge;
};

/** One spawned wave the spawner shows on the minimap until its last unit is gone. */
USTRUCT()
struct FEnemyWaveSpawnerMinimapMarker
{
	GENERATED_BODY()

	FName IconId = NAME_None;
	FVector Location = FVector::ZeroVector;
	// Where the heading was last measured from; the heading follows the group's progress, not its jitter.
	FVector HeadingAnchor = FVector::ZeroVector;
	float HeadingYaw = 0.f;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> Units;
};
