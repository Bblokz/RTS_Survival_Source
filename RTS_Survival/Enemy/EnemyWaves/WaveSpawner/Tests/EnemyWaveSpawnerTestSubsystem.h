// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "EnemyWaveSpawnerTestSubsystem.generated.h"

class AEnemyController;
class AEnemyWaveSpawner;
struct FAttackWaveElement;
struct FFormationData;

/**
 * @brief -EnemyWaveSpawnerValidate on a mission map with an enemy controller (UT_EnemyAI): spawns wave spawners near
 * the camera and checks a fixed count of 3 gives exactly 3 waves, the difficulty unit lists add up, a patrol stops
 * when its owning building dies, a spawner whose building died before it started never spawns, the minimap markers
 * follow the waves until their units die, attack units reach direct control and no spawner part shows in game.
 * Writes Saved/EnemyWaveSpawnerTest/results.txt and quits. Only built with RTS_WITH_MAP_TESTS.
 */
UCLASS()
class RTS_SURVIVAL_API UEnemyWaveSpawnerTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual TStatId GetStatId() const override;
	virtual void Tick(const float DeltaTime) override;
	// The controller pauses mission maps at start; the test unpauses it.
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	enum class EPhase : uint8
	{
		WaitingForStart,
		RunningWaves,
		KillingAttackUnits,
		Finished
	};

	EPhase M_Phase = EPhase::WaitingForStart;
	double M_PhaseStartSeconds = -1.0;

	UPROPERTY()
	TWeakObjectPtr<AEnemyController> M_EnemyController;

	UPROPERTY()
	TObjectPtr<AEnemyWaveSpawner> M_AttackSpawner;

	UPROPERTY()
	TObjectPtr<AEnemyWaveSpawner> M_PatrolSpawner;

	UPROPERTY()
	TObjectPtr<AEnemyWaveSpawner> M_DeadBuildingSpawner;

	UPROPERTY()
	TObjectPtr<AActor> M_PatrolBuilding;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> M_AttackUnits;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> M_PatrolUnits;

	TArray<int32> M_AttackWaveUnitCounts;
	int32 M_AttackFinishedBroadcasts = 0;
	int32 M_PatrolFinishedBroadcasts = 0;
	int32 M_PeakAttackMarkers = 0;
	bool bM_HasDestroyedPatrolBuilding = false;
	bool bM_SawPatrolFormation = false;
	bool bM_SawAttackUnitInDirectControl = false;
	FVector M_Center = FVector::ZeroVector;

	TArray<FString> M_Lines;
	int32 M_Failed = 0;

	UFUNCTION()
	void OnAttackUnitsSpawned(AEnemyWaveSpawner* Spawner, const TArray<AActor*>& Units);

	UFUNCTION()
	void OnPatrolUnitsSpawned(AEnemyWaveSpawner* Spawner, const TArray<AActor*>& Units);

	UFUNCTION()
	void OnAttackFinished(AEnemyWaveSpawner* Spawner);

	UFUNCTION()
	void OnPatrolFinished(AEnemyWaveSpawner* Spawner);

	void SetPhase(const EPhase NewPhase);
	double GetPhaseSeconds() const;
	void StartTest();
	void CheckDifficultyUnitLists();
	void SpawnTestSpawners();
	AEnemyWaveSpawner* SpawnConfiguredSpawner(const FVector& Location, const int32 Variant);
	void ConfigureAttackSpawner(AEnemyWaveSpawner& Spawner) const;
	void ConfigurePatrolSpawner(AEnemyWaveSpawner& Spawner) const;
	void ConfigureDeadBuildingSpawner(AEnemyWaveSpawner& Spawner) const;
	void ConfigureDifficultyListsSpawner(AEnemyWaveSpawner& Spawner) const;
	int32 CountOverlappingSpawnLocations(const TArray<FAttackWaveElement>& Elements) const;
	void TickRunningWaves();
	void CheckRunResults();
	void KillAttackUnits();
	bool GetIsPatrolFormationOfTestUnits(const FFormationData& Formation) const;
	void TickKillingAttackUnits();
	void WatchFormationsAndDirectControl();
	void CheckNothingVisibleInGame(const AEnemyWaveSpawner* Spawner, const FString& Name);
	void Evaluate();
	void Check(const bool bCondition, const FString& Name, const FString& Detail);
	bool FindNavigableCenter(FVector& OutCenter) const;
};
