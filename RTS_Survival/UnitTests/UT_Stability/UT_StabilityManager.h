#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UT_StabilityManager.generated.h"

/**
 * Basic struct tracking all normal spawns (the original ones).
 */
USTRUCT()
struct FSpawnedUnitInfo
{
	GENERATED_BODY()

	UPROPERTY()
	AActor* SpawnedActor = nullptr;

	/** If it's a player or enemy.  */
	bool bIsPlayerUnit = false;

	/** The slot index or transform used. */
	int32 SlotIndex = INDEX_NONE;
};

/**
 * Extended struct for movement-based spawns.
 * We keep track of the original spawn offset so we can do formation-based movement.
 */
USTRUCT()
struct FMoveSpawnedUnitInfo
{
	GENERATED_BODY()

	UPROPERTY()
	AActor* SpawnedActor = nullptr;

	/** True if we decided to spawn it as a "player" unit, false if "enemy". */
	bool bIsPlayerUnit = false;

	/** The local offset from StartMovement that we used. */
	FVector SpawnOffset;
};

/**
 * This class periodically spawns normal units in a rectangular formation (SpawnInterval),
 * destroys them at DestroyInterval, and also spawns a separate set of "movement" units 
 * at MovementSpawnInterval rate.
 */
UCLASS()
class RTS_SURVIVAL_API AUT_StabilityManager : public AActor
{
	GENERATED_BODY()

public:
	AUT_StabilityManager();

	/**
	 * Initializes the manager with arrays of classes (player/enemy),
	 * plus start locations for them, and start/finish for the movement-based units.
	 */
	UFUNCTION(BlueprintCallable, Category = "StabilityManager")
	void SetupStabilityTest(
		const TArray<TSubclassOf<AActor>>& PlayerUnits,
		const TArray<TSubclassOf<AActor>>& EnemyUnits,
		const FVector& PlayerStartLocation,
		const FVector& EnemyStartLocation,
		const FVector& StartMovement,
		const FVector& FinishMovement
	);

protected:
	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;

	/** ============= Original Spawn Settings ============= */

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stability|Spawn")
	float SpawnInterval;  // How often we spawn normal units

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stability|Spawn")
	int32 SpawnAmount;    // How many of each side per spawn iteration

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stability|Spawn")
	float RectangleOffset; // spacing in the rectangular formation

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stability|Destroy")
	float DestroyInterval; // how often we destroy all "normal" spawns

	/** ============= Movement Spawn Settings ============= */

	/**
	 * Spawns are at half the original spawn rate. So the period = SpawnInterval / 2,
	 * or you can store it as MovementSpawnInterval if desired.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stability|Movement")
	int32 MovementSpawnAmount; // how many "movement units" to spawn each iteration

	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stability|Movement")
	float MovementSpawnInterval;

	/**
	 * Time interval after which we destroy all movement-based units
	 * that we spawned (so they do not accumulate forever).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stability|Movement")
	float GarbageCollectMoveUnitsInterval;

	// The start movement location: where movement units spawn
	FVector MovementStartLocation;

	// The finish movement location: where movement units attempt to move to (in formation).
	FVector MovementFinishLocation;

	/** Timers for normal spawns and normal destruction. */
	FTimerHandle SpawnTimerHandle;
	FTimerHandle DestroyTimerHandle;

	/** Timer for movement-based spawns and timer to GC them. */
	FTimerHandle MovementSpawnTimerHandle;
	FTimerHandle DestroyMovementUnitsTimerHandle;

	FTimerHandle ClearAllCharactesTimerHandle;

	/**
	 * Classes for each side
	 */
	UPROPERTY()
	TArray<TSubclassOf<AActor>> PlayerUnitClasses;

	UPROPERTY()
	TArray<TSubclassOf<AActor>> EnemyUnitClasses;

	UPROPERTY()
	FVector PlayerUnitsStartLocation;

	UPROPERTY()
	FVector EnemyUnitsStartLocation;

	/**
	 * The normal spawns: we track them here, so `DestroyAllSpawnedUnits()` can destroy them.
	 */
	UPROPERTY()
	TArray<FSpawnedUnitInfo> AllSpawnedUnits;

	/**
	 * The movement-based spawns: we track them here, so we can destroy them separately.
	 */
	UPROPERTY()
	TArray<FMoveSpawnedUnitInfo> MovementSpawnedUnits;

	/** 
	 * Data structures for "normal" spawns. 
	 * We store transform "slots" for each side, so that we do rectangular expansions.
	 */
	UPROPERTY()
	TArray<FTransform> PlayerSlots;

	UPROPERTY()
	TArray<bool> BPlayerSlotOccupied;

	UPROPERTY()
	TArray<FTransform> EnemySlots;

	UPROPERTY()
	TArray<bool> BEnemySlotOccupied;

	/**
	 * Called each SpawnInterval to do the normal spawns for both sides.
	 */
	void SpawnIteration();

	/**
	 * Called each DestroyInterval to destroy all normal spawns, then spawn new.
	 */
	void DestroyAllSpawnedUnits();

	/**
	 * Called each "movement spawn" iteration to spawn a certain number of units 
	 * around MovementStartLocation and tell them to move to MovementFinishLocation.
	 */
	void SpawnMovementUnits();

	/**
	 * Called each GarbageCollectMoveUnitsInterval to destroy the movement-based units.
	 */
	void DestroyAllMovementUnits();


	/**
	 * Spawns a batch of normal units for the given side.
	 */
	void SpawnBatch(bool bIsPlayer);

	/**
	 * Returns the transform for the next vacant slot on the given side (player/enemy).
	 */
	FTransform GetNextSpawnTransform(bool bIsPlayer);

	/**
	 * Expands the array of vacant slots if we run out, for the given side.
	 */
	void CreateAdditionalSlots(bool bIsPlayer, int32 NumSlotsToCreate);

	/**
	 * Bound to OnDestroyed for each normal-spawned actor so we can mark its slot as vacant, etc.
	 */
	UFUNCTION()
	void OnActorBeginDestroyCallback(AActor* DestroyingActor);

	/**
	 * Called from OnActorBeginDestroyCallback to do blueprint event and free the slot.
	 */
	void OnUnitBeginDestroy(AActor* DestroyingUnit, bool bIsPlayer);

	// Blueprint event for spawning/destroying normal spawns
	UFUNCTION(BlueprintImplementableEvent, Category="StabilityManager")
	void OnSpawnPlayerUnit(AActor* SpawnedUnit);

	UFUNCTION(BlueprintImplementableEvent, Category="StabilityManager")
	void OnSpawnEnemyUnit(AActor* SpawnedUnit);

	UFUNCTION(BlueprintImplementableEvent, Category="StabilityManager")
	void OnUnitDestroyed(AActor* DestroyedUnit, bool bIsPlayer);

public:
	virtual void Tick(float DeltaTime) override;
};
