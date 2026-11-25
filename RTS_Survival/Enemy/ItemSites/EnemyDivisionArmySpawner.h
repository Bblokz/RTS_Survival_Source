// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnemyItemSiteTypes.h"
#include "EnemyDivisionArmySpawner.generated.h"

class AEnemyController;
class ARTSAsyncSpawner;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnEnemyDivisionArmySpawned, int32 /*SpawnedUnitCount*/);

/** Progress of one army spawn; the army counts as spawned only once every request has answered. */
USTRUCT()
struct FEnemyDivisionArmySpawnState
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FEnemyPlacementUnit> UnitsToSpawn;

	EEnemyDivisionArmyOrders Orders = EEnemyDivisionArmyOrders::EnemyAIControl;
	float UnitSpacingMultiplier = 1.f;
	int32 EnemyPlayer = INDEX_NONE;
	int32 PendingSpawnCount = 0;
	int32 FailedSpawnCount = 0;
	int32 AsyncSpawnerRetryCount = 0;
	bool bHasStarted = false;
	// Set while the requests go out: a request can answer at once, and the army must not finish mid-loop.
	bool bIsIssuingSpawns = false;
	bool bIsSpawned = false;
};

/**
 * @brief Spawns the attacked world division at its owner (the AEnemyItemSiteDirector of a field map, the enemy start)
 * and hands it to the enemy AI once both the units and the AI are ready. The director reads GetLivingUnits for the
 * victory.
 */
UCLASS(ClassGroup = (Custom))
class RTS_SURVIVAL_API UEnemyDivisionArmySpawner : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyDivisionArmySpawner();

	/**
	 * @brief Spawns every unit of the army on rings around the owner; runs once. OnArmySpawned fires when every
	 * spawn request has answered (at once for an empty army).
	 * @param Army The division's units from the world map.
	 * @param Settings Orders, convoy truck and spacing.
	 * @param EnemyPlayer Player the units are spawned for.
	 * @return false when the army was already started.
	 */
	bool StartSpawningArmy(const FWorldMissionEnemyArmy& Army, const FEnemyDivisionArmySettings& Settings,
	                       int32 EnemyPlayer);

	// Call once the enemy AI is ready; units on EnemyAIControl orders are handed to it when they are all spawned.
	void SetEnemyController(AEnemyController* EnemyController);

	bool GetIsArmySpawned() const { return M_SpawnState.bIsSpawned; }
	int32 GetSpawnedUnitCount() const { return M_ArmyUnits.Num(); }
	int32 GetFailedSpawnCount() const { return M_SpawnState.FailedSpawnCount; }

	// Spawned units that are alive and still hostile to the human player (a captured unit is gone).
	TArray<AActor*> GetLivingUnits() const;

	FOnEnemyDivisionArmySpawned OnArmySpawned;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	FEnemyDivisionArmySpawnState M_SpawnState;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> M_ArmyUnits;

	UPROPERTY()
	TWeakObjectPtr<AEnemyController> M_EnemyController;

	// Set by SetEnemyController; the hand-off joins on this and on the army being spawned.
	bool bM_IsEnemyAIReady = false;
	bool bM_HasHandedOffToEnemyAI = false;
	FTimerHandle M_AsyncSpawnerRetryTimerHandle;
	FTimerHandle M_SpawnAnswerTimeoutHandle;

	bool GetIsValidEnemyController() const;
	void TryIssueSpawns();
	void StartSpawnAnswerTimeout();
	void OnSpawnAnswerTimeout();
	void ScheduleAsyncSpawnerRetry();
	void IssueSpawns(ARTSAsyncSpawner& AsyncSpawner);
	void RequestUnitSpawn(ARTSAsyncSpawner& AsyncSpawner, int32 UnitIndex);
	void HandleUnitSpawned(int32 UnitIndex, AActor* SpawnedUnit);
	void TryFinishSpawning();
	void TryHandOffToEnemyAI();
	FVector GetSpawnLocation(int32 UnitIndex) const;
	bool GetIsLivingArmyUnit(AActor* Unit) const;
};
