#include "UT_StabilityManager.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "RTS_Survival/Interfaces/Commands.h"

// Sets default values
AUT_StabilityManager::AUT_StabilityManager()
{
	PrimaryActorTick.bCanEverTick = true;

	// Default "normal" spawn settings
	SpawnInterval = 5.0f;
	SpawnAmount = 5;
	RectangleOffset = 200.0f;
	DestroyInterval = 15.0f;

	// Default "movement" spawn settings
	MovementSpawnAmount = 5;
	GarbageCollectMoveUnitsInterval = 30.f;

	PlayerUnitsStartLocation = FVector::ZeroVector;
	EnemyUnitsStartLocation = FVector::ZeroVector;

	MovementStartLocation = FVector::ZeroVector;
	MovementFinishLocation = FVector::ZeroVector;
}

void AUT_StabilityManager::BeginPlay()
{
	Super::BeginPlay();
	// We do nothing until SetupStabilityTest is called
}

void AUT_StabilityManager::BeginDestroy()
{
	// Unbind OnDestroyed for normal spawns
	for (FSpawnedUnitInfo& Info : AllSpawnedUnits)
	{
		if (IsValid(Info.SpawnedActor))
		{
			Info.SpawnedActor->OnDestroyed.RemoveDynamic(this, &AUT_StabilityManager::OnActorBeginDestroyCallback);
		}
	}

	// Also safely destroy movement units if they haven't been GC'd
	for (FMoveSpawnedUnitInfo& MoveInfo : MovementSpawnedUnits)
	{
		if (IsValid(MoveInfo.SpawnedActor))
		{
			MoveInfo.SpawnedActor->Destroy();
		}
	}

	Super::BeginDestroy();
}

void AUT_StabilityManager::SetupStabilityTest(
	const TArray<TSubclassOf<AActor>>& InPlayerUnits,
	const TArray<TSubclassOf<AActor>>& InEnemyUnits,
	const FVector& PlayerStart,
	const FVector& EnemyStart,
	const FVector& StartMovement,
	const FVector& FinishMovement
)
{
	if (InPlayerUnits.Num() == 0 || InEnemyUnits.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("UT_StabilityManager::SetupStabilityTest: No player or enemy classes specified!"));
		return;
	}

	PlayerUnitClasses = InPlayerUnits;
	EnemyUnitClasses = InEnemyUnits;
	PlayerUnitsStartLocation = PlayerStart;
	EnemyUnitsStartLocation = EnemyStart;

	MovementStartLocation = StartMovement;
	MovementFinishLocation = FinishMovement;

	// Clear the normal spawn structures
	PlayerSlots.Empty();
	BPlayerSlotOccupied.Empty();
	EnemySlots.Empty();
	BEnemySlotOccupied.Empty();
	AllSpawnedUnits.Empty();

	// Clear the movement array
	MovementSpawnedUnits.Empty();

	// Create a small initial rectangle of spawn slots for each side
	CreateAdditionalSlots(true, 10);
	CreateAdditionalSlots(false, 10);

	// Start the normal spawn timer
	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
	GetWorldTimerManager().SetTimer(
		SpawnTimerHandle,
		this,
		&AUT_StabilityManager::SpawnIteration,
		SpawnInterval,
		true
	);

	// Start the normal destruction timer
	GetWorldTimerManager().ClearTimer(DestroyTimerHandle);
	GetWorldTimerManager().SetTimer(
		DestroyTimerHandle,
		this,
		&AUT_StabilityManager::DestroyAllSpawnedUnits,
		DestroyInterval,
		true
	);

	GetWorldTimerManager().ClearTimer(MovementSpawnTimerHandle);
	GetWorldTimerManager().SetTimer(
		MovementSpawnTimerHandle,
		this,
		&AUT_StabilityManager::SpawnMovementUnits,
		MovementSpawnInterval,
		true
	);

	// Garbage collect the movement units at intervals
	GetWorldTimerManager().ClearTimer(DestroyMovementUnitsTimerHandle);
	GetWorldTimerManager().SetTimer(
		DestroyMovementUnitsTimerHandle,
		this,
		&AUT_StabilityManager::DestroyAllMovementUnits,
		GarbageCollectMoveUnitsInterval,
		true
	);
}

void AUT_StabilityManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AUT_StabilityManager::SpawnIteration()
{
	// Normal spawns
	SpawnBatch(true);  // Player
	SpawnBatch(false); // Enemy
}

void AUT_StabilityManager::DestroyAllSpawnedUnits()
{
	// Make a copy
	TArray<FSpawnedUnitInfo> UnitsToDestroy = AllSpawnedUnits;

	for (auto& Info : UnitsToDestroy)
	{
		if (IsValid(Info.SpawnedActor))
		{
			Info.SpawnedActor->Destroy();
		}
	}
	AllSpawnedUnits.Empty();

	// Force GC
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

	// Mark all normal spawn slots vacant again
	for (int32 i = 0; i < BPlayerSlotOccupied.Num(); i++)
	{
		BPlayerSlotOccupied[i] = false;
	}
	for (int32 i = 0; i < BEnemySlotOccupied.Num(); i++)
	{
		BEnemySlotOccupied[i] = false;
	}

	// Optionally spawn again immediately
	SpawnIteration();
}



void AUT_StabilityManager::SpawnMovementUnits()
{
	if (PlayerUnitClasses.Num() == 0 || EnemyUnitClasses.Num() == 0)
	{
		return;
	}

	for (int32 i = 0; i < MovementSpawnAmount; i++)
	{
		// We spawn each unit in a small random rectangle. 
		// Or we can do a grid approach. We'll do a quick random approach:
		const float GridWidth = 300.f; // or some custom approach
		const float RandX = FMath::RandRange(-GridWidth, GridWidth);
		const float RandY = FMath::RandRange(-GridWidth, GridWidth);

		// The spawn offset from MovementStartLocation
		FVector SpawnOffset(RandX, RandY, 0.f);
		FVector SpawnLoc = MovementStartLocation + SpawnOffset;

		bool bSpawnAsPlayer = (FMath::RandBool()); 
		const TArray<TSubclassOf<AActor>>* UnitClasses = bSpawnAsPlayer ? &PlayerUnitClasses : &EnemyUnitClasses;

		int32 RandIdx = FMath::RandRange(0, UnitClasses->Num() - 1);
		TSubclassOf<AActor> ClassToSpawn = (*UnitClasses)[RandIdx];
		if (!ClassToSpawn)
		{
			continue;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		FRotator SpawnRot = FRotator::ZeroRotator;
		AActor* NewActor = GetWorld()->SpawnActor<AActor>(ClassToSpawn, SpawnLoc, SpawnRot, SpawnParams);
		if (!NewActor)
		{
			continue;
		}

		// If it's a pawn, spawn a default controller
		if (APawn* Pawn = Cast<APawn>(NewActor))
		{
			Pawn->SpawnDefaultController();
		}

		// Add to our MovementSpawnedUnits array
		FMoveSpawnedUnitInfo MoveInfo;
		MoveInfo.SpawnedActor = NewActor;
		MoveInfo.bIsPlayerUnit = bSpawnAsPlayer;
		MoveInfo.SpawnOffset = SpawnOffset;
		MovementSpawnedUnits.Add(MoveInfo);

		// Now "move" it to the MovementFinishLocation with the same offset
		if (ICommands* CommandsPtr = Cast<ICommands>(NewActor))
		{
			// Formation logic: apply same offset around finish
			FVector Destination = MovementFinishLocation + SpawnOffset;
			CommandsPtr->MoveToLocation(Destination, true /* bSetUnitToIdle */, FRotator::ZeroRotator);
		}
	}
}

void AUT_StabilityManager::DestroyAllMovementUnits()
{
	// Similar to normal spawns, but only for MovementSpawnedUnits
	TArray<FMoveSpawnedUnitInfo> UnitsToDestroy = MovementSpawnedUnits;

	for (auto& MoveInfo : UnitsToDestroy)
	{
		if (IsValid(MoveInfo.SpawnedActor))
		{
			MoveInfo.SpawnedActor->Destroy();
		}
	}
	MovementSpawnedUnits.Empty();

	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
}

void AUT_StabilityManager::SpawnBatch(bool bIsPlayer)
{
	if (bIsPlayer && PlayerUnitClasses.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("No player classes?"));
		return;
	}
	if (!bIsPlayer && EnemyUnitClasses.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("No enemy classes?"));
		return;
	}

	for (int32 i = 0; i < SpawnAmount; i++)
	{
		FTransform SpawnXf = GetNextSpawnTransform(bIsPlayer);

		int32 RandIdx;
		TSubclassOf<AActor> ClassToSpawn;
		if (bIsPlayer)
		{
			RandIdx = FMath::RandRange(0, PlayerUnitClasses.Num() - 1);
			ClassToSpawn = PlayerUnitClasses[RandIdx];
		}
		else
		{
			RandIdx = FMath::RandRange(0, EnemyUnitClasses.Num() - 1);
			ClassToSpawn = EnemyUnitClasses[RandIdx];
		}

		if (ClassToSpawn)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = this;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			AActor* NewActor = GetWorld()->SpawnActor<AActor>(ClassToSpawn, SpawnXf, SpawnParams);
			if (NewActor)
			{
				APawn* Pawn = Cast<APawn>(NewActor);
				if (Pawn)
				{
					Pawn->SpawnDefaultController();
				}

				// Bind to OnDestroyed
				NewActor->OnDestroyed.AddDynamic(this, &AUT_StabilityManager::OnActorBeginDestroyCallback);

				if (bIsPlayer)
				{
					OnSpawnPlayerUnit(NewActor);
				}
				else
				{
					OnSpawnEnemyUnit(NewActor);
				}

				FSpawnedUnitInfo Info;
				Info.SpawnedActor = NewActor;
				Info.bIsPlayerUnit = bIsPlayer;
				AllSpawnedUnits.Add(Info);
			}
		}
	}
}

FTransform AUT_StabilityManager::GetNextSpawnTransform(bool bIsPlayer)
{
	TArray<FTransform>& Slots = bIsPlayer ? PlayerSlots : EnemySlots;
	TArray<bool>& OccupiedFlags = bIsPlayer ? BPlayerSlotOccupied : BEnemySlotOccupied;

	int32 VacantIdx = INDEX_NONE;
	for (int32 i = 0; i < OccupiedFlags.Num(); i++)
	{
		if (!OccupiedFlags[i])
		{
			VacantIdx = i;
			break;
		}
	}

	if (VacantIdx == INDEX_NONE)
	{
		CreateAdditionalSlots(bIsPlayer, 10);
		for (int32 i = 0; i < OccupiedFlags.Num(); i++)
		{
			if (!OccupiedFlags[i])
			{
				VacantIdx = i;
				break;
			}
		}
	}

	if (!Slots.IsValidIndex(VacantIdx))
	{
		return FTransform();
	}
	OccupiedFlags[VacantIdx] = true;
	return Slots[VacantIdx];
}

void AUT_StabilityManager::CreateAdditionalSlots(bool bIsPlayer, int32 NumSlotsToCreate)
{
	FVector BaseLocation = bIsPlayer ? PlayerUnitsStartLocation : EnemyUnitsStartLocation;
	TArray<FTransform>& Slots = bIsPlayer ? PlayerSlots : EnemySlots;
	TArray<bool>& OccupiedFlags = bIsPlayer ? BPlayerSlotOccupied : BEnemySlotOccupied;

	int32 CurrentNum = Slots.Num();

	for (int32 i = 0; i < NumSlotsToCreate; i++)
	{
		const int32 GridWidth = 5;

		int32 Row = (CurrentNum + i) / GridWidth;
		int32 Col = (CurrentNum + i) % GridWidth;

		FVector SpawnLoc = BaseLocation + FVector(Col * RectangleOffset, Row * RectangleOffset, 0.0f);

		FTransform Tm;
		Tm.SetLocation(SpawnLoc);
		Tm.SetRotation(FQuat::Identity);
		Tm.SetScale3D(FVector::OneVector);

		Slots.Add(Tm);
		OccupiedFlags.Add(false);
	}
}

void AUT_StabilityManager::OnActorBeginDestroyCallback(AActor* DestroyingActor)
{
	if (!DestroyingActor) return;

	for (int32 i = 0; i < AllSpawnedUnits.Num(); i++)
	{
		if (AllSpawnedUnits[i].SpawnedActor == DestroyingActor)
		{
			bool bPlayer = AllSpawnedUnits[i].bIsPlayerUnit;
			OnUnitBeginDestroy(DestroyingActor, bPlayer);
			AllSpawnedUnits.RemoveAt(i);
			break;
		}
	}
}

void AUT_StabilityManager::OnUnitBeginDestroy(AActor* DestroyingUnit, bool bIsPlayer)
{
	OnUnitDestroyed(DestroyingUnit, bIsPlayer);

	// Free the slot
	TArray<FTransform>& Slots = bIsPlayer ? PlayerSlots : EnemySlots;
	TArray<bool>& Occupied = bIsPlayer ? BPlayerSlotOccupied : BEnemySlotOccupied;

	FVector UnitLoc = DestroyingUnit->GetActorLocation();
	float BestDist = FLT_MAX;
	int32 BestIdx = INDEX_NONE;

	for (int32 i = 0; i < Slots.Num(); i++)
	{
		const float DistSq = FVector::DistSquared(Slots[i].GetLocation(), UnitLoc);
		if (DistSq < BestDist)
		{
			BestDist = DistSq;
			BestIdx = i;
		}
	}

	if (BestIdx != INDEX_NONE && Occupied.IsValidIndex(BestIdx))
	{
		Occupied[BestIdx] = false;
	}
}
