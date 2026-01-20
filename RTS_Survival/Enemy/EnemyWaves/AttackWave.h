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
	float HelpOffsetRadiusMltMax = 1.f;

	// Multiplier applied to the unit inner radius for the min help offset.
	float HelpOffsetRadiusMltMin = 1.f;

	// Max time a unit can keep attacking after reaching a waypoint before advancing.
	float MaxAttackTimeBeforeAdvancingToNextWayPoint = 0.f;

	// How many times to try to project a help offset location per tick.
	int32 MaxTriesFindNavPointForHelpOffset = 5;

	// Scale applied to RTSToNavProjectionExtent for help offset projections.
	float ProjectionScale = 1.f;
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

	UPROPERTY()
	TArray<AActor*> SpawnedWaveUnits;
};
