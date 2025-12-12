// Copyright (C) Bas Blokzijl - All rights reserved.

#include "UT_TurretManager.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/UnitTests/TestTurrets/UT_TurretTarget/UT_TurretTarget.h"

AUT_TurretManager::AUT_TurretManager()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AUT_TurretManager::BeginPlay()
{
	Super::BeginPlay();
	// By default, do nothing here until SetupTurretTest is called from blueprint.
}

void AUT_TurretManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// You can do any debugging or checks in here if needed.
}

void AUT_TurretManager::SetupTurretTest(TArray<TSubclassOf<AActor>> UnitsWithTurrets,
	TArray<TSubclassOf<AUT_TurretTarget>> Targets, TArray<AActor*> ActorsDenotingTurretLocations)
{
	if (UnitsWithTurrets.Num() == 0 || ActorsDenotingTurretLocations.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetupTurretTest: No turrets or no locations specified!"));
		return;
	}

	// Store references
	this->TurretClasses = UnitsWithTurrets;
	this->TargetClasses = Targets;
	this->TurretLocations = ActorsDenotingTurretLocations;

	// Mark all location slots as vacant
	bLocationVacant.Init(true, TurretLocations.Num());

	// We'll spawn turrets in the order they appear in TurretClasses
	NextTurretIndex = 0;
	SpawnedTurrets.Empty();

	// Start a 1-second repeating timer to spawn the turrets
	GetWorldTimerManager().SetTimer(
		SpawnTimerHandle,
		this,
		&AUT_TurretManager::TurretSpawnTick,
		1.0f, 
		true  // looping
	);
}

void AUT_TurretManager::TurretSpawnTick()
{
	// 1) If we've already spawned all turrets, do nothing
	if (NextTurretIndex >= TurretClasses.Num())
	{
		return; 
	}

	// 2) Find a vacant location
	int32 LocationIndex = FindVacantLocationIndex();
	if (LocationIndex == INDEX_NONE)
	{
		// No vacant location found; wait until next second
		return;
	}

	// 3) Spawn the next turret
	SpawnNextTurretAtLocation(LocationIndex);

	// 4) Move to the next turret class for next time
	NextTurretIndex++;
}

int32 AUT_TurretManager::FindVacantLocationIndex() const
{
	for (int32 i = 0; i < bLocationVacant.Num(); i++)
	{
		if (bLocationVacant[i])
		{
			return i;
		}
	}
	return INDEX_NONE;
}

void AUT_TurretManager::SpawnNextTurretAtLocation(int32 LocationIndex)
{
	if (!TurretClasses.IsValidIndex(NextTurretIndex) || !TurretLocations.IsValidIndex(LocationIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnNextTurretAtLocation: invalid indices"));
		return;
	}

	TSubclassOf<AActor> TurretClass = TurretClasses[NextTurretIndex];
	AActor* LocationActor = TurretLocations[LocationIndex];

	if (!TurretClass || !IsValid(LocationActor))
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnNextTurretAtLocation: invalid turret class or location actor"));
		return;
	}

	bLocationVacant[LocationIndex] = false;

	// Spawn the turret at the location actor's transform
	FTransform SpawnTransform = LocationActor->GetTransform();
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.bNoFail = true;
	AActor* SpawnedTurret = GetWorld()->SpawnActor<AActor>(TurretClass, SpawnTransform, SpawnParams);
	if (!SpawnedTurret)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to spawn turret at location %d"), LocationIndex);
		bLocationVacant[LocationIndex] = true; // revert
		return;
	}
	if(APawn* TurretPawn = Cast<APawn>(SpawnedTurret))
	{
		TurretPawn->SpawnDefaultController();
	}
	OnTurretSpawned(SpawnedTurret);

	// We'll also spawn 4 targets around it (north, east, south, west) with random TargetClass from our array
	// Each offset is 600 units away, same Z as the turret
	TArray<FVector> Offsets;
	const float OffsetMlt = 800.f;
	Offsets.Add(FVector(OffsetMlt, 0.f, 0.f));   // East
	Offsets.Add(FVector(-OffsetMlt, 0.f, 0.f));  // West
	Offsets.Add(FVector(0.f, OffsetMlt, 0.f));   // North
	Offsets.Add(FVector(0.f, -OffsetMlt, 0.f));  // South

	TArray<AUT_TurretTarget*> SpawnedTargetArray;

	for (const FVector& Offset : Offsets)
	{
		if (TargetClasses.Num() == 0)
		{
			break;
		}
		const int32 RandIdx = FMath::RandRange(0, TargetClasses.Num() - 1);
		TSubclassOf<AUT_TurretTarget> RandomTargetClass = TargetClasses[RandIdx];
		if (!RandomTargetClass)
		{
			continue; // skip if null
		}

		FVector TargetSpawnLoc = SpawnTransform.GetLocation() + Offset;
		FRotator TargetSpawnRot = SpawnTransform.GetRotation().Rotator();
		
		AUT_TurretTarget* SpawnedTarget = GetWorld()->SpawnActor<AUT_TurretTarget>(
			RandomTargetClass, 
			TargetSpawnLoc, 
			TargetSpawnRot,
			SpawnParams
		);
		if (SpawnedTarget)
		{
			// Initialize it with a pointer to our manager
			SpawnedTarget->Init(this);
			SpawnedTargetArray.Add(SpawnedTarget);
		}
	}

	// Keep track of this turret in our SpawnedTurrets array
	FSpawnedTurretInfo Info;
	Info.TurretActor = SpawnedTurret;
	Info.SpawnedTargets = SpawnedTargetArray;
	Info.LocationIndex = LocationIndex;
	SpawnedTurrets.Add(Info);
}

void AUT_TurretManager::OnTargetDestroyed(AUT_TurretTarget* DestroyedTarget)
{
	if (!DestroyedTarget)
	{
		return;
	}

	// We'll find which turret's list this target belongs to
	for (int32 i = 0; i < SpawnedTurrets.Num(); i++)
	{
		FSpawnedTurretInfo& TurretInfo = SpawnedTurrets[i];
		if (TurretInfo.SpawnedTargets.Contains(DestroyedTarget))
		{
			// Let Blueprint know a turret killed a target
			OnTurretKilledTarget(DestroyedTarget);

			// Remove the target from that turret's array
			TurretInfo.SpawnedTargets.Remove(DestroyedTarget);

			// Check if that turret still has targets 
			if (TurretInfo.SpawnedTargets.Num() == 0)
			{
				// All targets are gone => destroy the turret
				AActor* TurretActor = TurretInfo.TurretActor;
				if (IsValid(TurretActor))
				{
					// Let Blueprint know the turret is finished
					OnTurretFinishedTargets(TurretActor);

					// Destroy the turret
					TurretActor->Destroy();
				}

				// Mark the location as vacant
				if (bLocationVacant.IsValidIndex(TurretInfo.LocationIndex))
				{
					bLocationVacant[TurretInfo.LocationIndex] = true;
				}

				// Remove the turret info from the array 
				SpawnedTurrets.RemoveAt(i);
			}

			break; // done
		}
	}
}
