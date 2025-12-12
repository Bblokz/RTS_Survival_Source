// Copyright (C) Bas Blokzijl - All rights reserved.

#include "UT_VFX_Manager.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

AUT_VFX_Manager::AUT_VFX_Manager()
{
	PrimaryActorTick.bCanEverTick = true;

	SpawnTime = 5.0f;
	CurrentWaveIndex = 0;
}

void AUT_VFX_Manager::BeginPlay()
{
	Super::BeginPlay();
	ChangeArrayOrderDependingOnStartIndex();

	if (UnitClasses.Num() == 0 || SpawnTime <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("AUT_VFX_Manager::BeginPlay - No unit classes or invalid SpawnTime."));
		return;
	}

	// Start the main repeating timer
	GetWorldTimerManager().SetTimer(
		SpawnWaveTimerHandle,
		this,
		&AUT_VFX_Manager::HandleSpawnWave,
		SpawnTime,
		true
	);

	// Immediately call once to spawn wave #0
	HandleSpawnWave();
}

void AUT_VFX_Manager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

/** Clears all currently spawned units. */
void AUT_VFX_Manager::DestroyCurrentWaveUnits()
{
	for (AActor* OldUnit : CurrentSpawnedUnits)
	{
		if (IsValid(OldUnit))
		{
			OldUnit->Destroy();
		}
	}
	CurrentSpawnedUnits.Empty();
}

/** Spawns wave at the specified index, optionally increments CurrentWaveIndex after. */
void AUT_VFX_Manager::SpawnWaveAtIndex(int32 WaveIndex, bool bIncrementIndex)
{
	if (WaveIndex >= UnitClasses.Num())
	{
		// No wave to spawn or we've run out of waves
		GetWorldTimerManager().ClearTimer(SpawnWaveTimerHandle);
		return;
	}
	TSubclassOf<AActor> WaveClass = UnitClasses[WaveIndex];
	if (!WaveClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Wave class is null at index %d"), WaveIndex);
		return;
	}

	// For each spawn position, spawn an actor of that class
	for (const FVector& SpawnPos : SpawnPositions)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AActor* NewUnit = GetWorld()->SpawnActor<AActor>(
			WaveClass,
			SpawnPos,
			FRotator::ZeroRotator,
			SpawnParams
		);
		if (NewUnit)
		{
			if (APawn* AsPawn = Cast<APawn>(NewUnit))
			{
				AsPawn->SpawnDefaultController();
			}
			CurrentSpawnedUnits.Add(NewUnit);
		}
	}

	// Call blueprint event
	int32 WaveNumber = WaveIndex + 1;
	int32 MaxWaves = UnitClasses.Num();
	OnWaveSpawned(WaveNumber, MaxWaves);

	if (bIncrementIndex)
	{
		CurrentWaveIndex++;
		bWasPreviousWaveIncrementingIndex = true;
	}
	else
	{
		bWasPreviousWaveIncrementingIndex = false;
	}
}

/** The main wave spawn function used by the repeating timer. */
void AUT_VFX_Manager::HandleSpawnWave()
{
	// Destroy old wave
	DestroyCurrentWaveUnits();

	// Spawn the new wave at the current wave index, then increment
	SpawnWaveAtIndex(CurrentWaveIndex, /*bIncrementIndex=*/true);
}

void AUT_VFX_Manager::ReSpawnPreviousWave()
{
	// If we're at the first wave, there's no previous wave. Just restart current wave.
	if (CurrentWaveIndex <= 1)
	{
		// i.e., if CurrentWaveIndex == 0, definitely no wave behind us
		RestartCurrentWave();
		return;
	}

	// We have a previous wave to re-spawn
	// 1) Stop the normal wave timer
	GetWorldTimerManager().ClearTimer(SpawnWaveTimerHandle);

	// 2) Destroy current wave 
	DestroyCurrentWaveUnits();

	if(bWasPreviousWaveIncrementingIndex)
	{
		// If the previous wave incremented the index, we need to decrement it back to obtain our wave.
		CurrentWaveIndex--;
	}

	// 3) Decrement wave index to get the previous wave
	CurrentWaveIndex--;

	// 4) Spawn that previous wave (do not increment wave index!)
	SpawnWaveAtIndex(CurrentWaveIndex, /*bIncrementIndex=*/false);

	// 5) After SpawnTime seconds, we destroy that wave, move wave index forward, re-spawn that wave, and resume
	GetWorldTimerManager().ClearTimer(OneShotTimerHandle);
	GetWorldTimerManager().SetTimer(
		OneShotTimerHandle,
		this,
		&AUT_VFX_Manager::OnReSpawnPreviousWaveExpired,
		SpawnTime,
		false
	);
}

void AUT_VFX_Manager::OnReSpawnPreviousWaveExpired()
{
	// Called after displaying the "previous wave" for SpawnTime seconds

	// 1) Destroy that "previous wave"
	DestroyCurrentWaveUnits();

	// 2) Move wave index forward to the wave we originally had
	CurrentWaveIndex++;

	// 3) Spawn that wave
	SpawnWaveAtIndex(CurrentWaveIndex, /*bIncrementIndex=*/false);

	// 4) Restart the main repeating timer
	GetWorldTimerManager().SetTimer(
		SpawnWaveTimerHandle,
		this,
		&AUT_VFX_Manager::HandleSpawnWave,
		SpawnTime,
		true
	);
}

void AUT_VFX_Manager::RestartCurrentWave()
{
	// 1) Stop the normal wave timer
	GetWorldTimerManager().ClearTimer(SpawnWaveTimerHandle);

	// 2) Destroy the current wave
	DestroyCurrentWaveUnits();

	// If the previous wave incremented the index, we need to decrement it back to obtain our wave.
	if(bWasPreviousWaveIncrementingIndex)
	{
		CurrentWaveIndex--;
	}
	

	// 3) Re-spawn the same wave index (no increment)
	SpawnWaveAtIndex(CurrentWaveIndex, /*bIncrementIndex=*/false);

	// 4) After SpawnTime seconds, we destroy it and continue to the next wave
	GetWorldTimerManager().ClearTimer(OneShotTimerHandle);
	GetWorldTimerManager().SetTimer(
		OneShotTimerHandle,
		this,
		&AUT_VFX_Manager::OnRestartCurrentWaveExpired,
		SpawnTime,
		false
	);
}

void AUT_VFX_Manager::NextWave()
{
	// Stop the normal wave timer
	GetWorldTimerManager().ClearTimer(SpawnWaveTimerHandle);

	// Destroy the current wave
	DestroyCurrentWaveUnits();

	// Move wave index forward so next wave is up
	if(not bWasPreviousWaveIncrementingIndex)
	{
		// If the previous wave was not an increment; we need to increment the index to obtain the next wave.
	CurrentWaveIndex++;
	}

	

	// Spawn wave #CurrentWaveIndex now, and re-start the main timer
	SpawnWaveAtIndex(CurrentWaveIndex, /*bIncrementIndex=*/true);

	// Resume the main repeating timer
	GetWorldTimerManager().SetTimer(
		SpawnWaveTimerHandle,
		this,
		&AUT_VFX_Manager::HandleSpawnWave,
		SpawnTime,
		true
	);
}

void AUT_VFX_Manager::OnRestartCurrentWaveExpired()
{
	// Called after showing the same wave for SpawnTime seconds

	// Destroy that wave
	DestroyCurrentWaveUnits();

	// Move wave index forward so next wave is up
	CurrentWaveIndex++;

	// If we've gone past the last wave, no more waves to spawn. Otherwise, spawn next wave and resume.
	if (CurrentWaveIndex >= UnitClasses.Num())
	{
		UE_LOG(LogTemp, Log, TEXT("RestartCurrentWave ended, but no more waves left."));
		return;
	}

	// Spawn wave #CurrentWaveIndex now, and re-start the main timer
	SpawnWaveAtIndex(CurrentWaveIndex, /*bIncrementIndex=*/true);

	// Resume the main repeating timer
	GetWorldTimerManager().SetTimer(
		SpawnWaveTimerHandle,
		this,
		&AUT_VFX_Manager::HandleSpawnWave,
		SpawnTime,
		true
	);
}

void AUT_VFX_Manager::ChangeArrayOrderDependingOnStartIndex()
{
	if(StartIndex <= 0)
	{
		return;
	}
	// Reshuffle the array so that the StartIndex is the first element of the new array
	TArray<TSubclassOf<AActor>> NewUnitClasses;
	for(int32 i = StartIndex; i < UnitClasses.Num(); i++)
	{
		NewUnitClasses.Add(UnitClasses[i]);
	}
	for(int32 i = 0; i < StartIndex; i++)
	{
		NewUnitClasses.Add(UnitClasses[i]);
	}
	UnitClasses = NewUnitClasses;
}
