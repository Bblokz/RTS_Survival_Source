// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyDivisionArmySpawner.h"

#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyDirectControlComponent/EnemyDirectControlComponent.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"

namespace EnemyDivisionArmySpawnerConstants
{
	// The async spawner is a level actor; on the first ticks of a level it may not be registered yet.
	constexpr float AsyncSpawnerRetryIntervalSeconds = 0.5f;
	constexpr int32 MaxAsyncSpawnerRetries = 20;
	// A request the spawner accepted but never answered must not keep the field map unbuilt (and unwinnable) forever.
	constexpr float SpawnAnswerTimeoutSeconds = 30.f;
}

UEnemyDivisionArmySpawner::UEnemyDivisionArmySpawner()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UEnemyDivisionArmySpawner::StartSpawningArmy(const FWorldMissionEnemyArmy& Army,
                                                  const FEnemyDivisionArmySettings& Settings,
                                                  const int32 EnemyPlayer)
{
	if (M_SpawnState.bHasStarted)
	{
		return false;
	}

	M_SpawnState.bHasStarted = true;
	M_SpawnState.Orders = Settings.GetOrdersForDivision(Army.DivisionType);
	M_SpawnState.UnitSpacingMultiplier = Settings.UnitSpacingMultiplier;
	M_SpawnState.EnemyPlayer = EnemyPlayer;
	const int32 SkippedEntryCount = EnemyItemSiteRules::BuildArmySpawnList(Army, Settings.ConvoyTruck,
	                                                                        M_SpawnState.UnitsToSpawn);
	if (SkippedEntryCount > 0)
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("Division army of %s skipped %d roster entries: a subtype was None, or the convoy truck is not set in ")
			TEXT("the director's ArmySettings."), *GetNameSafe(GetOwner()), SkippedEntryCount));
	}

	M_SpawnState.PendingSpawnCount = M_SpawnState.UnitsToSpawn.Num();
	TryIssueSpawns();
	return true;
}

void UEnemyDivisionArmySpawner::TryIssueSpawns()
{
	if (M_SpawnState.UnitsToSpawn.IsEmpty())
	{
		TryFinishSpawning();
		return;
	}

	ARTSAsyncSpawner* AsyncSpawner = FRTS_Statics::GetAsyncSpawner(this);
	if (IsValid(AsyncSpawner))
	{
		IssueSpawns(*AsyncSpawner);
		return;
	}

	if (M_SpawnState.AsyncSpawnerRetryCount < EnemyDivisionArmySpawnerConstants::MaxAsyncSpawnerRetries)
	{
		ScheduleAsyncSpawnerRetry();
		return;
	}

	RTSFunctionLibrary::ReportError(TEXT("Division army of ") + GetNameSafe(GetOwner()) +
		TEXT(" found no async spawner in the level; no unit was spawned."));
	M_SpawnState.FailedSpawnCount = M_SpawnState.UnitsToSpawn.Num();
	M_SpawnState.PendingSpawnCount = 0;
	TryFinishSpawning();
}

void UEnemyDivisionArmySpawner::ScheduleAsyncSpawnerRetry()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		M_SpawnState.FailedSpawnCount = M_SpawnState.UnitsToSpawn.Num();
		M_SpawnState.PendingSpawnCount = 0;
		TryFinishSpawning();
		return;
	}

	M_SpawnState.AsyncSpawnerRetryCount++;
	TWeakObjectPtr<UEnemyDivisionArmySpawner> WeakThis(this);
	World->GetTimerManager().SetTimer(M_AsyncSpawnerRetryTimerHandle, FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->TryIssueSpawns();
		}
	}), EnemyDivisionArmySpawnerConstants::AsyncSpawnerRetryIntervalSeconds, false);
}

void UEnemyDivisionArmySpawner::IssueSpawns(ARTSAsyncSpawner& AsyncSpawner)
{
	M_SpawnState.bIsIssuingSpawns = true;
	for (int32 UnitIndex = 0; UnitIndex < M_SpawnState.UnitsToSpawn.Num(); ++UnitIndex)
	{
		RequestUnitSpawn(AsyncSpawner, UnitIndex);
	}

	M_SpawnState.bIsIssuingSpawns = false;
	TryFinishSpawning();
	if (not M_SpawnState.bIsSpawned)
	{
		StartSpawnAnswerTimeout();
	}
}

void UEnemyDivisionArmySpawner::StartSpawnAnswerTimeout()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	TWeakObjectPtr<UEnemyDivisionArmySpawner> WeakThis(this);
	World->GetTimerManager().SetTimer(M_SpawnAnswerTimeoutHandle, FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->OnSpawnAnswerTimeout();
		}
	}), EnemyDivisionArmySpawnerConstants::SpawnAnswerTimeoutSeconds, false);
}

void UEnemyDivisionArmySpawner::OnSpawnAnswerTimeout()
{
	if (M_SpawnState.bIsSpawned)
	{
		return;
	}

	RTSFunctionLibrary::ReportError(FString::Printf(
		TEXT("Division army of %s: %d spawn requests got no answer after %.0f s; the army goes on with the %d units ")
		TEXT("that arrived."), *GetNameSafe(GetOwner()), M_SpawnState.PendingSpawnCount,
		EnemyDivisionArmySpawnerConstants::SpawnAnswerTimeoutSeconds, M_ArmyUnits.Num()));
	M_SpawnState.FailedSpawnCount += M_SpawnState.PendingSpawnCount;
	M_SpawnState.PendingSpawnCount = 0;
	TryFinishSpawning();
}

void UEnemyDivisionArmySpawner::RequestUnitSpawn(ARTSAsyncSpawner& AsyncSpawner, const int32 UnitIndex)
{
	const FTrainingOption TrainingOption = M_SpawnState.UnitsToSpawn[UnitIndex].ToTrainingOption();
	const AActor* Owner = GetOwner();
	const FRotator Rotation(0.f, IsValid(Owner) ? Owner->GetActorRotation().Yaw : 0.f, 0.f);
	TWeakObjectPtr<UEnemyDivisionArmySpawner> WeakThis(this);
	const TSharedRef<bool> bResolved = MakeShared<bool>(false);
	auto OnSpawned = [WeakThis, bResolved](const FTrainingOption&, AActor* SpawnedUnit, const int32 SpawnedUnitIndex)
	{
		// One answer per request, as the strongpoint guards the same spawner call.
		if (*bResolved)
		{
			return;
		}

		*bResolved = true;
		if (WeakThis.IsValid())
		{
			WeakThis->HandleUnitSpawned(SpawnedUnitIndex, SpawnedUnit);
		}
	};

	const int32 OwningPlayerOverride = FRTSUnitOwnerSpawning::GetOwnerOverrideForAIPlayer(M_SpawnState.EnemyPlayer);
	const bool bStarted = AsyncSpawner.AsyncSpawnOptionAtLocation(TrainingOption, GetSpawnLocation(UnitIndex), this,
	                                                              UnitIndex, OnSpawned, Rotation,
	                                                              OwningPlayerOverride);
	// A request that failed before loading still counts as started when its callback already ran.
	if (not bStarted && not *bResolved)
	{
		*bResolved = true;
		HandleUnitSpawned(UnitIndex, nullptr);
	}
}

void UEnemyDivisionArmySpawner::HandleUnitSpawned(const int32 UnitIndex, AActor* SpawnedUnit)
{
	// An answer after the timeout gave up on it: the army is already counted.
	if (M_SpawnState.bIsSpawned)
	{
		return;
	}

	M_SpawnState.PendingSpawnCount = FMath::Max(M_SpawnState.PendingSpawnCount - 1, 0);
	const int32 SpawnedOwner = FRTSPlayers::GetOwningPlayerOfActor(SpawnedUnit);
	if (IsValid(SpawnedUnit) && GetIsLivingArmyUnit(SpawnedUnit))
	{
		M_ArmyUnits.Add(SpawnedUnit);
	}
	else
	{
		M_SpawnState.FailedSpawnCount++;
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("Division army of %s: unit %d %s; it does not count toward the army."),
			*GetNameSafe(GetOwner()), UnitIndex,
			IsValid(SpawnedUnit)
				? *FString::Printf(TEXT("spawned for player %d, who is not hostile to the player"), SpawnedOwner)
				: TEXT("could not be spawned (no class for its training option?)")));
	}

	TryFinishSpawning();
}

void UEnemyDivisionArmySpawner::TryFinishSpawning()
{
	if (M_SpawnState.bIsSpawned || M_SpawnState.bIsIssuingSpawns || M_SpawnState.PendingSpawnCount > 0)
	{
		return;
	}

	M_SpawnState.bIsSpawned = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_SpawnAnswerTimeoutHandle);
	}

	UE_LOG(LogTemp, Display, TEXT("RTS_ENEMY_DIVISION_ARMY spawned owner=%s units=%d failed=%d orders=%s"),
	       *GetNameSafe(GetOwner()), M_ArmyUnits.Num(), M_SpawnState.FailedSpawnCount,
	       *UEnum::GetValueAsString(M_SpawnState.Orders));
	TryHandOffToEnemyAI();
	OnArmySpawned.Broadcast(M_ArmyUnits.Num());
}

void UEnemyDivisionArmySpawner::SetEnemyController(AEnemyController* EnemyController)
{
	M_EnemyController = EnemyController;
	bM_IsEnemyAIReady = true;
	TryHandOffToEnemyAI();
}

bool UEnemyDivisionArmySpawner::GetIsValidEnemyController() const
{
	if (M_EnemyController.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_EnemyController",
	                                                             "GetIsValidEnemyController", GetOwner());
	return false;
}

void UEnemyDivisionArmySpawner::TryHandOffToEnemyAI()
{
	// Units spawning and the enemy AI getting ready arrive in either order; this runs after both.
	if (bM_HasHandedOffToEnemyAI || not M_SpawnState.bIsSpawned || not bM_IsEnemyAIReady
		|| M_SpawnState.Orders != EEnemyDivisionArmyOrders::EnemyAIControl)
	{
		return;
	}

	bM_HasHandedOffToEnemyAI = true;
	if (not GetIsValidEnemyController())
	{
		return;
	}

	UEnemyDirectControlComponent* DirectControl = M_EnemyController->GetEnemyDirectControlComponent();
	if (not IsValid(DirectControl))
	{
		RTSFunctionLibrary::ReportError(TEXT("Division army of ") + GetNameSafe(GetOwner()) +
			TEXT(": the enemy controller has no direct control component; the army holds its start instead."));
		return;
	}

	DirectControl->RegisterDirectControlUnits(GetLivingUnits());
}

TArray<AActor*> UEnemyDivisionArmySpawner::GetLivingUnits() const
{
	TArray<AActor*> LivingUnits;
	for (const TWeakObjectPtr<AActor>& ArmyUnit : M_ArmyUnits)
	{
		if (GetIsLivingArmyUnit(ArmyUnit.Get()))
		{
			LivingUnits.Add(ArmyUnit.Get());
		}
	}

	return LivingUnits;
}

bool UEnemyDivisionArmySpawner::GetIsLivingArmyUnit(AActor* Unit) const
{
	// A squad controller destroys itself once its last soldier died, so the weak pointer covers squads too. The first AI
	// player's units keep their Blueprint owner, so hostility to the player is checked, not an exact owner; a unit
	// the player captured stops being hostile and counts as gone.
	return RTSFunctionLibrary::RTSIsValid(Unit)
		&& FRTSPlayers::IsActorHostileToPlayer(Unit, RTSPlayers::HumanPlayer);
}

FVector UEnemyDivisionArmySpawner::GetSpawnLocation(const int32 UnitIndex) const
{
	const AActor* Owner = GetOwner();
	const FVector Centre = IsValid(Owner) ? Owner->GetActorLocation() : FVector::ZeroVector;
	const FVector Offset = EnemyPlacement::GetRingSpreadOffset(UnitIndex) * M_SpawnState.UnitSpacingMultiplier;
	return EnemyPlacement::ProjectToNavigationIfPossible(GetWorld(), Centre + Offset);
}

void UEnemyDivisionArmySpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_AsyncSpawnerRetryTimerHandle);
		World->GetTimerManager().ClearTimer(M_SpawnAnswerTimeoutHandle);
	}

	Super::EndPlay(EndPlayReason);
}
