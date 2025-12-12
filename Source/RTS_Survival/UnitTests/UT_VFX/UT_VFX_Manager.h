// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UT_VFX_Manager.generated.h"

/**
 * Manages the spawning of VFX “units” (actors) in waves at specific intervals.
 * 
 * - Spawns a new wave every SpawnTime seconds.
 * - Each wave is associated with one entry in the UnitClasses array (the # of entries determines # of waves).
 * - The new wave is spawned at the locations in SpawnPositions.
 * - The previous wave is destroyed before spawning the new wave.
 * - If the spawned unit is a Pawn, we spawn a default controller.
 * - We save references to all spawned units in CurrentSpawnedUnits.
 * - After each wave spawns, we call the blueprint event OnWaveSpawned with wave info.
 */
UCLASS()
class RTS_SURVIVAL_API AUT_VFX_Manager : public AActor
{
	GENERATED_BODY()

public:
	/**
	 * Default constructor setting default values.
	 */
	AUT_VFX_Manager();

	virtual void Tick(float DeltaTime) override;

	/**
	 * Respawns the previous wave for SpawnTime seconds, then continues with the wave 
	 * that was originally going to be next.
	 * - If CurrentWaveIndex == -1, we have no previous wave, so we call RestartCurrentWave instead.
	 * - If we are wave #i, we go back to wave #(i - 0), wait SpawnTime seconds, 
	 *   then destroy that wave, set wave index back to #i, spawn wave i again, and 
	 *   resume the normal wave timer.
	 */
	UFUNCTION(BlueprintCallable, Category="VFX|WaveControl")
	void ReSpawnPreviousWave();

	/**
	 * Restarts the current wave immediately: 
	 * - We destroy the current wave,
	 * - re-spawn the same wave index, 
	 * - wait SpawnTime seconds, 
	 * - then proceed to the next wave and resume normal wave timer.
	 */
	UFUNCTION(BlueprintCallable, Category="VFX|WaveControl")
	void RestartCurrentWave();

	
	UFUNCTION(BlueprintCallable, Category="VFX|WaveControl")
	void NextWave();
	

protected:
	/**
	 * Called when the game starts or when spawned.
	 * Sets up the timer to spawn waves.
	 */
	virtual void BeginPlay() override;

	/** 
	 * Called each time we need to spawn the next wave of units in the normal cycle.
	 * Involves destroying the previous wave units, spawning new ones,
	 * and calling the OnWaveSpawned blueprint event.
	 */
	UFUNCTION()
	void HandleSpawnWave();

	/** 
	 * Spawns the wave at the specified wave index, storing the newly spawned units,
	 * calling OnWaveSpawned, etc. 
	 * If bIncrementIndex is true, it will increment the CurrentWaveIndex afterwards. 
	 */
	void SpawnWaveAtIndex(int32 WaveIndex, bool bIncrementIndex);

	/**
	 * Timer handle for wave spawning (the normal wave schedule).
	 */
	FTimerHandle SpawnWaveTimerHandle;

	/**
	 * A one-shot timer handle used for either respawning a previous wave or restarting the current wave.
	 */
	FTimerHandle OneShotTimerHandle;

	/**
	 * The index of the wave we are about to spawn next.
	 * Ranges from 0 to UnitClasses.Num() - 1.
	 */
	int32 CurrentWaveIndex;

	/**
	 * The array of currently spawned units (the “previous wave”).
	 * We destroy these units before spawning the next wave.
	 */
	UPROPERTY()
	TArray<AActor*> CurrentSpawnedUnits;

	/**
	 * Blueprint-implementable event called right after we spawn a wave.
	 * @param WaveNumber The index (1-based) of the current wave being spawned.
	 * @param MaxWaves The total number of waves (size of UnitClasses).
	 */
	UFUNCTION(BlueprintImplementableEvent, Category="VFX")
	void OnWaveSpawned(int32 WaveNumber, int32 MaxWaves);

	/**
	 * The time interval (in seconds) between each wave spawn in the normal cycle.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="VFX|Setup")
	float SpawnTime;

		UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="VFX|Setup")
	int32 StartIndex = 0;


	/**
	 * An array of spawn positions used to place the units for each wave.
	 * Typically set in the Blueprint or at runtime. 
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="VFX|Setup")
	TArray<FVector> SpawnPositions;

	/**
	 * The list of classes representing each wave to spawn.
	 * Each entry in this array corresponds to one wave.
	 * If we have N entries, we have N waves total.
	 */
	UPROPERTY(BlueprintReadWrite, Category="VFX|Setup")
	TArray<TSubclassOf<AActor>> UnitClasses;

	/**
	 * Destroys all currently spawned units and clears CurrentSpawnedUnits.
	 */
	void DestroyCurrentWaveUnits();

	/**
	 * Internal callback used for finishing the "ReSpawnPreviousWave" flow
	 * after the wave has been displayed for SpawnTime seconds.
	 * We destroy the wave, increment wave index, spawn that wave, and restart the main timer.
	 */
	void OnReSpawnPreviousWaveExpired();

	/**
	 * Internal callback used for finishing the "RestartCurrentWave" flow
	 * after showing the re-spawned wave for SpawnTime seconds.
	 * We then move on to the next wave and restart the main timer.
	 */
	void OnRestartCurrentWaveExpired();

	void ChangeArrayOrderDependingOnStartIndex();

	bool bWasPreviousWaveIncrementingIndex = false;
};
