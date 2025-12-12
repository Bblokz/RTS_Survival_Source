// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UT_TurretManager.generated.h"

// Forward declarations
class AUT_TurretTarget;

/**
 * A small struct to track info about each spawned turret:
 */
USTRUCT()
struct FSpawnedTurretInfo
{
	GENERATED_BODY()

	// The actual spawned turret actor
	UPROPERTY()
	AActor* TurretActor = nullptr;

	// The array of targets spawned around this turret
	UPROPERTY()
	TArray<AUT_TurretTarget*> SpawnedTargets;

	// The index in ActorsDenotingTurretLocations that we used for this turret
	int32 LocationIndex = INDEX_NONE;
};

/**
 * Turret Manager spawns turret units each second, and spawns 4 targets
 * around each turret. Tracks which are destroyed, etc.
 */
UCLASS()
class RTS_SURVIVAL_API AUT_TurretManager : public AActor
{
	GENERATED_BODY()
	
public:
	AUT_TurretManager();

protected:
	virtual void BeginPlay() override;

	/**
	 * Called every frame.
	 */
	virtual void Tick(float DeltaTime) override;

public:
	/**
	 * Called from Blueprints to initialize the turret test:
	 * @param UnitsWithTurrets: array of possible turret classes to spawn (AActor children).
	 * @param Targets: array of possible turret-target classes to spawn (AUT_TurretTarget).
	 * @param ActorsDenotingTurretLocations: array of actors in the world whose transform 
	 *     indicates where we can place a turret.
	 */
	UFUNCTION(BlueprintCallable)
	void SetupTurretTest(
		TArray<TSubclassOf<AActor>> UnitsWithTurrets,
		TArray<TSubclassOf<AUT_TurretTarget>> Targets,
		TArray<AActor*> ActorsDenotingTurretLocations
	);

	/**
	 * Called by a target when it is destroyed, so we can update 
	 * our tracking data structures. 
	 */
	void OnTargetDestroyed(AUT_TurretTarget* DestroyedTarget);

protected:
	/** 
	 * Timer callback that attempts to spawn turrets (and targets) each second.
	 * If we have more turrets to spawn and there's a vacant location, spawn next turret.
	 */
	void TurretSpawnTick();

	/** Finds the first vacant location index, or INDEX_NONE if none free. */
	int32 FindVacantLocationIndex() const;

	/** Spawns a turret using the next turret class, plus spawns 4 targets around it. */
	void SpawnNextTurretAtLocation(int32 LocationIndex);

	/** Notifies Blueprint that a single target was killed (not necessarily last target). */
	UFUNCTION(BlueprintImplementableEvent, Category = "TurretTest")
	void OnTurretKilledTarget(AUT_TurretTarget* KilledTarget);

	/** Notifies Blueprint that a turret has finished all its targets. */
	UFUNCTION(BlueprintImplementableEvent, Category = "TurretTest")
	void OnTurretFinishedTargets(AActor* FinishedTurret);

	UFUNCTION(BlueprintImplementableEvent)
	void OnTurretSpawned(AActor* SpawnedTurret);

private:

	/** The list of turret classes we want to spawn, once each. */
	UPROPERTY()
	TArray<TSubclassOf<AActor>> TurretClasses;

	/** The list of possible target classes we can spawn around each turret. */
	UPROPERTY()
	TArray<TSubclassOf<AUT_TurretTarget>> TargetClasses;

	/**
	 * The list of actors in the level used to denote potential turret spawn locations.
	 * We'll place exactly one turret at each location at a time (if available).
	 */
	UPROPERTY()
	TArray<AActor*> TurretLocations;

	/**
	 * Tracks which location is "occupied" by a spawned turret. 
	 * We'll mark them vacant again once that turret is done.
	 */
	UPROPERTY()
	TArray<bool> bLocationVacant;

	/**
	 * The next index into TurretClasses that we need to spawn. 
	 * Once we surpass TurretClasses.Num(), we have spawned them all.
	 */
	int32 NextTurretIndex;

	/** Timer handle for our "spawn tick." */
	FTimerHandle SpawnTimerHandle;

	/**
	 * All the turrets we've spawned so far, along with their associated targets
	 * and location index. We can remove them once they are destroyed.
	 */
	UPROPERTY()
	TArray<FSpawnedTurretInfo> SpawnedTurrets;
};
