// Copyright (C) Bas Blokzijl - All rights reserved.


#include "EnemyWaveController.h"

#include "Algo/Unique.h"
#include "NavigationSystem.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"


UEnemyWaveController::UEnemyWaveController()
{
	PrimaryComponentTick.bCanEverTick = false;

	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UEnemyWaveController::StartNewAttackWave(
	const EEnemyWaveType WaveType,
	const TArray<FAttackWaveElement>& WaveElements,
	const float WaveInterval,
	const float IntervalVarianceFraction,
	const TArray<FVector>& Waypoints,
	const FRotator& FinalWaypointDirection,
	const int32 MaxFormationWidth,
	const bool bInstantStart,
	AActor* WaveCreator,
	const TArray<TWeakObjectPtr<AActor>>& WaveTimerAffectingBuildings,
	const float PerAffectingBuildingTimerFraction, const float FormationOffsetMultiplier)
{
	if (not GetIsValidWave(WaveType,
	                       WaveElements,
	                       WaveInterval,
	                       IntervalVarianceFraction,
	                       Waypoints,
	                       WaveCreator,
	                       WaveTimerAffectingBuildings,
	                       PerAffectingBuildingTimerFraction))
	{
		return;
	}
	if (not GetIsValidAsyncSpawner() || not EnsureEnemyControllerIsValid())
	{
		RTSFunctionLibrary::ReportError(
			"Invalid async spawner or enemy controller for enemy wave controller. Cannot start new attack wave.");
		return;
	}
	const int32 UniqueWaveID = GetUniqueWaveID();
	constexpr bool bIsSingleTimerSpawnWave = false;
	if (not CreateNewAttackWaveStruct(
		UniqueWaveID,
		WaveType,
		WaveElements, WaveInterval, IntervalVarianceFraction, Waypoints, FinalWaypointDirection,
		MaxFormationWidth, WaveCreator, WaveTimerAffectingBuildings, PerAffectingBuildingTimerFraction,
		bIsSingleTimerSpawnWave,
		FormationOffsetMultiplier))
	{
		RTSFunctionLibrary::ReportError("failed to create struct for attack wave!");
		return;
	}
	FAttackWave* AttackWave = GetAttackWaveByID(UniqueWaveID);
	if (not AttackWave)
	{
		return;
	}
	if (not CreateAttackWaveTimer(AttackWave, bInstantStart))
	{
		RTSFunctionLibrary::ReportError("Failed to start attack wave timer for wave, will remove the struct!");
		RemoveAttackWaveByIDAndInvalidateTimer(AttackWave);
	}
}

void UEnemyWaveController::StartNewAttackMoveWave(
	const EEnemyWaveType WaveType,
	const TArray<FAttackWaveElement>& WaveElements,
	const float WaveInterval,
	const float IntervalVarianceFraction,
	const TArray<FVector>& Waypoints,
	const FRotator& FinalWaypointDirection,
	const int32 MaxFormationWidth,
	const bool bInstantStart,
	AActor* WaveCreator,
	const TArray<TWeakObjectPtr<AActor>>& WaveTimerAffectingBuildings,
	const float PerAffectingBuildingTimerFraction,
	const float FormationOffsetMultiplier,
	const float HelpOffsetRadiusMltMax,
	const float HelpOffsetRadiusMltMin,
	const float MaxAttackTimeBeforeAdvancingToNextWayPoint,
	const int32 MaxTriesFindNavPointForHelpOffset,
	const float ProjectionScale)
{
	if (not GetIsValidWave(
		WaveType,
		WaveElements,
		WaveInterval,
		IntervalVarianceFraction,
		Waypoints,
		WaveCreator,
		WaveTimerAffectingBuildings,
		PerAffectingBuildingTimerFraction))
	{
		return;
	}
	if (not GetIsValidAttackMoveWaveSettings(
		HelpOffsetRadiusMltMax,
		HelpOffsetRadiusMltMin,
		MaxTriesFindNavPointForHelpOffset,
		ProjectionScale))
	{
		return;
	}
	if (not GetIsValidAsyncSpawner() || not EnsureEnemyControllerIsValid())
	{
		RTSFunctionLibrary::ReportError(
			"Invalid async spawner or enemy controller for enemy wave controller. Cannot start new attack move wave.");
		return;
	}

	FAttackMoveWaveSettings AttackMoveSettings;
	AttackMoveSettings.HelpOffsetRadiusMltMax = HelpOffsetRadiusMltMax;
	AttackMoveSettings.HelpOffsetRadiusMltMin = HelpOffsetRadiusMltMin;
	AttackMoveSettings.MaxAttackTimeBeforeAdvancingToNextWayPoint = MaxAttackTimeBeforeAdvancingToNextWayPoint;
	AttackMoveSettings.MaxTriesFindNavPointForHelpOffset = MaxTriesFindNavPointForHelpOffset;
	AttackMoveSettings.ProjectionScale = ProjectionScale;

	const int32 UniqueWaveID = GetUniqueWaveID();
	constexpr bool bIsSingleTimerSpawnWave = false;
	constexpr bool bIsAttackMoveWave = true;
	if (not CreateNewAttackWaveStruct(
		UniqueWaveID,
		WaveType,
		WaveElements,
		WaveInterval,
		IntervalVarianceFraction,
		Waypoints,
		FinalWaypointDirection,
		MaxFormationWidth,
		WaveCreator,
		WaveTimerAffectingBuildings,
		PerAffectingBuildingTimerFraction,
		bIsSingleTimerSpawnWave,
		FormationOffsetMultiplier,
		bIsAttackMoveWave,
		AttackMoveSettings))
	{
		RTSFunctionLibrary::ReportError("failed to create struct for attack move wave!");
		return;
	}

	FAttackWave* AttackWave = GetAttackWaveByID(UniqueWaveID);
	if (not AttackWave)
	{
		return;
	}
	if (not CreateAttackWaveTimer(AttackWave, bInstantStart))
	{
		RTSFunctionLibrary::ReportError("Failed to start attack move wave timer for wave, will remove the struct!");
		RemoveAttackWaveByIDAndInvalidateTimer(AttackWave);
	}
}

void UEnemyWaveController::StartSingleAttackWave(
	const EEnemyWaveType WaveType,
	const TArray<FAttackWaveElement>& WaveElements,
	const TArray<FVector>& Waypoints,
	const FRotator& FinalWaypointDirection,
	const int32 MaxFormationWidth,
	const float TimeTillWave,
	AActor* WaveCreator,
	const float FormationOffsetMultiplier)
{
	if (not GetIsValidSingleAttackWave(WaveType, WaveElements, Waypoints, WaveCreator))
	{
		return;
	}
	if (not GetIsValidAsyncSpawner() || not EnsureEnemyControllerIsValid())
	{
		RTSFunctionLibrary::ReportError(
			"Invalid async spawner or enemy controller for enemy wave controller. Cannot start single attack wave.");
		return;
	}
	const int32 UniqueWaveID = GetUniqueWaveID();
	const float ClampedTimeTillWave = FMath::Max(TimeTillWave, 0.f);
	const bool bIsSingleWave = true;
	if (not CreateNewAttackWaveStruct(
		UniqueWaveID,
		WaveType,
		WaveElements, ClampedTimeTillWave, 0.f, Waypoints, FinalWaypointDirection,
		MaxFormationWidth, WaveCreator, {}, 0.f, bIsSingleWave,
		FormationOffsetMultiplier))
	{
		RTSFunctionLibrary::ReportError("failed to create struct for single attack wave!");
		return;
	}
	FAttackWave* AttackWave = GetAttackWaveByID(UniqueWaveID);
	if (not AttackWave)
	{
		return;
	}
	const bool bInstantStart = ClampedTimeTillWave <= 0.f;
	if (not CreateAttackWaveTimer(AttackWave, bInstantStart))
	{
		RTSFunctionLibrary::ReportError("Failed to start attack wave timer for single wave, will remove the struct!");
		RemoveAttackWaveByIDAndInvalidateTimer(AttackWave);
	}
}

void UEnemyWaveController::StartSingleAttackMoveWave(
	const EEnemyWaveType WaveType,
	const TArray<FAttackWaveElement>& WaveElements,
	const TArray<FVector>& Waypoints,
	const FRotator& FinalWaypointDirection,
	const int32 MaxFormationWidth,
	const float TimeTillWave,
	AActor* WaveCreator,
	const float FormationOffsetMultiplier,
	const float HelpOffsetRadiusMltMax,
	const float HelpOffsetRadiusMltMin,
	const float MaxAttackTimeBeforeAdvancingToNextWayPoint,
	const int32 MaxTriesFindNavPointForHelpOffset,
	const float ProjectionScale)
{
	if (not GetIsValidSingleAttackWave(WaveType, WaveElements, Waypoints, WaveCreator))
	{
		return;
	}
	if (not GetIsValidAttackMoveWaveSettings(
		HelpOffsetRadiusMltMax,
		HelpOffsetRadiusMltMin,
		MaxTriesFindNavPointForHelpOffset,
		ProjectionScale))
	{
		return;
	}
	if (not GetIsValidAsyncSpawner() || not EnsureEnemyControllerIsValid())
	{
		RTSFunctionLibrary::ReportError(
			"Invalid async spawner or enemy controller for enemy wave controller. Cannot start single attack move wave.");
		return;
	}

	FAttackMoveWaveSettings AttackMoveSettings;
	AttackMoveSettings.HelpOffsetRadiusMltMax = HelpOffsetRadiusMltMax;
	AttackMoveSettings.HelpOffsetRadiusMltMin = HelpOffsetRadiusMltMin;
	AttackMoveSettings.MaxAttackTimeBeforeAdvancingToNextWayPoint = MaxAttackTimeBeforeAdvancingToNextWayPoint;
	AttackMoveSettings.MaxTriesFindNavPointForHelpOffset = MaxTriesFindNavPointForHelpOffset;
	AttackMoveSettings.ProjectionScale = ProjectionScale;

	const int32 UniqueWaveID = GetUniqueWaveID();
	const float ClampedTimeTillWave = FMath::Max(TimeTillWave, 0.f);
	const bool bIsSingleWave = true;
	constexpr bool bIsAttackMoveWave = true;
	if (not CreateNewAttackWaveStruct(
		UniqueWaveID,
		WaveType,
		WaveElements,
		ClampedTimeTillWave,
		0.f,
		Waypoints,
		FinalWaypointDirection,
		MaxFormationWidth,
		WaveCreator,
		{},
		0.f,
		bIsSingleWave,
		FormationOffsetMultiplier,
		bIsAttackMoveWave,
		AttackMoveSettings))
	{
		RTSFunctionLibrary::ReportError("failed to create struct for single attack move wave!");
		return;
	}

	FAttackWave* AttackWave = GetAttackWaveByID(UniqueWaveID);
	if (not AttackWave)
	{
		return;
	}
	const bool bInstantStart = ClampedTimeTillWave <= 0.f;
	if (not CreateAttackWaveTimer(AttackWave, bInstantStart))
	{
		RTSFunctionLibrary::ReportError(
			"Failed to start attack move wave timer for single wave, will remove the struct!");
		RemoveAttackWaveByIDAndInvalidateTimer(AttackWave);
	}
}

void UEnemyWaveController::InitWaveController(AEnemyController* EnemyController)
{
	M_EnemyController = EnemyController;
}


void UEnemyWaveController::BeginPlay()
{
	Super::BeginPlay();

	M_NavigationSystem = UNavigationSystemV1::GetCurrent(GetWorld());
}

bool UEnemyWaveController::EnsureEnemyControllerIsValid()
{
	if (M_EnemyController.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(TEXT("EnemyWaveController has no valid enemy controller! "
		"\n see UEnemyWaveController::EnsureEnemyControllerIsValid"));
	M_EnemyController = Cast<AEnemyController>(GetOwner());
	return M_EnemyController.IsValid();
}

void UEnemyWaveController::RemoveAttackWaveByIDAndInvalidateTimer(FAttackWave* AttackWave)
{
	if (not AttackWave)
	{
		return;
	}
	FTimerHandle TimerHandle = AttackWave->WaveTimerHandle;
	const int32 UniqueID = AttackWave->UniqueWaveID;
	M_AttackWaves.RemoveAll([UniqueID](const FAttackWave& Wave) { return Wave.UniqueWaveID == UniqueID; });
	const UWorld* World = GetWorld();
	if (World && TimerHandle.IsValid())
	{
		World->GetTimerManager().ClearTimer(TimerHandle);
	}
}

FAttackWave* UEnemyWaveController::GetAttackWaveByID(const int32 UniqueID)
{
	for (auto& EachWave : M_AttackWaves)
	{
		if (EachWave.UniqueWaveID == UniqueID)
		{
			return &EachWave;
		}
	}
	RTSFunctionLibrary::ReportError("Could not find attack wave with ID: " + FString::FromInt(UniqueID));
	return nullptr;
}

bool UEnemyWaveController::CreateNewAttackWaveStruct(const int32 UniqueID, const EEnemyWaveType WaveType,
                                                     const TArray<FAttackWaveElement>& WaveElements,
                                                     const float WaveInterval,
                                                     const float IntervalVarianceFraction,
                                                     const TArray<FVector>& Waypoints,
                                                     const FRotator& FinalWaypointDirection,
                                                     const int32 MaxFormationWidth,
                                                     AActor* WaveCreator,
                                                     const TArray<TWeakObjectPtr<AActor>>& WaveTimerAffectingBuildings,
                                                     const float PerAffectingBuildingTimerFraction,
                                                     const bool bIsSingleWave,
                                                     const float FormationOffsetMultiplier,
                                                     const bool bIsAttackMoveWave,
                                                     const FAttackMoveWaveSettings& AttackMoveSettings)
{
	// Sanity check for unique ID
	if (not EnsureIDIsUnique(UniqueID))
	{
		RTSFunctionLibrary::ReportError("Cannot create new attack wave struct; ID is not unique: " +
			FString::FromInt(UniqueID));
		return false;
	}
	const FTimerHandle NewWaveTimerHandle;
	const FTimerDelegate NewWaveTimerDelegate;
	FAttackWave NewAttackWave;
	NewAttackWave.WaveType = WaveType;
	NewAttackWave.WaveElements = WaveElements;
	NewAttackWave.WaveCreator = WaveCreator;
	NewAttackWave.WaveTimerAffectingBuildings = WaveTimerAffectingBuildings;
	NewAttackWave.PerAffectingBuildingTimerFraction = PerAffectingBuildingTimerFraction;
	NewAttackWave.WaveTimerHandle = NewWaveTimerHandle;
	NewAttackWave.WaveDelegate = NewWaveTimerDelegate;
	NewAttackWave.WaveInterval = WaveInterval;
	NewAttackWave.IntervalVarianceFraction = IntervalVarianceFraction;
	NewAttackWave.Waypoints = Waypoints;
	NewAttackWave.FinalWaypointDirection = FinalWaypointDirection;
	NewAttackWave.MaxFormationWidth = MaxFormationWidth;
	NewAttackWave.UniqueWaveID = UniqueID;
	NewAttackWave.bIsSingleWave = bIsSingleWave;
	NewAttackWave.FormationOffsetMlt = FormationOffsetMultiplier;
	NewAttackWave.AttackMoveSettings = AttackMoveSettings;
	NewAttackWave.bIsAttackMoveWave = bIsAttackMoveWave;
	M_AttackWaves.Add(NewAttackWave);
	return true;
}

bool UEnemyWaveController::CreateAttackWaveTimer(FAttackWave* AttackWave, const bool bInstantStart)
{
	UWorld* World = GetWorld();
	if (not World || not AttackWave)
	{
		return false;
	}
	TWeakObjectPtr<UEnemyWaveController> WeakThis(this);
	const int32 ID = AttackWave->UniqueWaveID;
	auto OnWaveIteration = [WeakThis, ID ]()-> void
	{
		if (not WeakThis.IsValid())
		{
			return;
		}
		UEnemyWaveController* This = WeakThis.Get();
		FAttackWave* Wave = WeakThis->GetAttackWaveByID(ID);
		if (not Wave)
		{
			return;
		}
		if (This->DidAttackWaveCreatorDie(Wave))
		{
			This->OnAttackWaveCreatorDied(Wave);
			// The wave is removed; nothing to do.
			return;
		}
		// If the spawning fails then there is no call to on OnWaveCompletedSpawn which means we need to
		// set the next timer iteration here as this is normally done automatically after a wave spawned all untis
		// and started formation movement.
		if (not WeakThis->SpawnUnitsForAttackWave(Wave))
		{
			RTSFunctionLibrary::PrintString(
				"Wave iteration could not spawn any units possibly due "
				"to no wave supply left. skipping this iteration.");
			// Set timer again for next iteration.
			(void)WeakThis->CreateAttackWaveTimer(Wave, false);
		}
	};
	AttackWave->WaveDelegate.BindLambda(OnWaveIteration);
	if (bInstantStart)
	{
		// If we want to start the wave immediately then we call the delegate right away.
		OnWaveIteration();
		return true;
	}
	// Unbind previous timer.
	if (AttackWave->WaveTimerHandle.IsValid())
	{
		World->GetTimerManager().ClearTimer(AttackWave->WaveTimerHandle);
	}
	// If we do not want to start the wave immediately then we set a timer for the first iteration.
	World->GetTimerManager().SetTimer(
		AttackWave->WaveTimerHandle,
		AttackWave->WaveDelegate,
		GetIterationTimeForWave(AttackWave),
		false // Do not loop, we will set the next iteration time in the delegate.
	);
	return true;
}

float UEnemyWaveController::GetIterationTimeForWave(FAttackWave* AttackWave) const
{
	if (not AttackWave)
	{
		return 0.f;
	}
	const float MinInterval = AttackWave->WaveInterval * (1.f - AttackWave->IntervalVarianceFraction);
	const float MaxInterval = AttackWave->WaveInterval * (1.f + AttackWave->IntervalVarianceFraction);
	return FMath::RandRange(MinInterval, MaxInterval) * GetWaveTimeMltDependingOnGenerators(AttackWave);
}

float UEnemyWaveController::GetWaveTimeMltDependingOnGenerators(FAttackWave* AttackWave) const
{
	if (not AttackWave || AttackWave->WaveType != EEnemyWaveType::Wave_OwningBuildingAndPowerGenerators)
	{
		// Does not depend on power generators.
		return 1.f;
	}
	int32 AmountBuildingsNotValid = 0;
	for (auto EachBuilding : AttackWave->WaveTimerAffectingBuildings)
	{
		if (not EachBuilding.IsValid() || not RTSFunctionLibrary::RTSIsValid(EachBuilding.Get()))
		{
			AmountBuildingsNotValid++;
		}
	}
	if constexpr (DeveloperSettings::Debugging::GEnemyController_Compile_DebugSymbols)
	{
		if (AmountBuildingsNotValid > 0)
		{
			RTSFunctionLibrary::DisplayNotification(FText::FromString("The wave is depended on now"
				"destroyed buildings, mlt:" + FString::SanitizeFloat(
					AmountBuildingsNotValid * AttackWave->PerAffectingBuildingTimerFraction + 1)));
		}
	}
	return (AmountBuildingsNotValid * AttackWave->PerAffectingBuildingTimerFraction) + 1.f;
}

bool UEnemyWaveController::DidAttackWaveCreatorDie(const FAttackWave* AttackWave) const
{
	if (not AttackWave || AttackWave->WaveType == EEnemyWaveType::Wave_NoOwningBuilding)
	{
		return false;
	}
	if (not AttackWave->WaveCreator.IsValid() || not RTSFunctionLibrary::RTSIsValid(AttackWave->WaveCreator.Get()))
	{
		RTSFunctionLibrary::ReportError("Attack wave creator is not valid! Will invalidate attackWave.");
		return true;
	}
	return false;
}

void UEnemyWaveController::OnAttackWaveCreatorDied(FAttackWave* AttackWave)
{
	RemoveAttackWaveByIDAndInvalidateTimer(AttackWave);
}

bool UEnemyWaveController::SpawnUnitsForAttackWave(FAttackWave* AttackWave)
{
	if (!AttackWave || !GetIsValidAsyncSpawner() || !EnsureEnemyControllerIsValid())
		return false;

	// Reset the counter and clear previous actors.
	AttackWave->ResetForSpawningNewWave();

	struct FPendingSpawn
	{
		FTrainingOption Option;
		FVector Location;
	};
	TArray<FPendingSpawn> PendingSpawns;
	PendingSpawns.Reserve(AttackWave->WaveElements.Num());

	// Pick the units we will try to spawn, and reserve their supply.
	for (auto& Element : AttackWave->WaveElements)
	{
		FTrainingOption PickedOption;
		if (!GetRandomAttackWaveElementOption(Element, PickedOption))
			continue;

		if (M_EnemyController->GetEnemyWaveSupply() <= 0)
		{
			RTSFunctionLibrary::PrintString(
				"Enemy has no wave supply left; skipping this element.");
			continue;
		}

		// We consume one supply for this unit so that we don't spawn more than we can afford in this loop.
		M_EnemyController->AddToWaveSupply(-1);
		PendingSpawns.Add({PickedOption, Element.SpawnLocation});
	}

	// Tell the wave how many callbacks we expect
	const int32 NumToSpawn = PendingSpawns.Num();
	AttackWave->AwaitingSpawnsTillStartMoving = NumToSpawn;
	if (NumToSpawn == 0)
	{
		return false;
	}

	TWeakObjectPtr<UEnemyWaveController> WeakThis(this);
	for (auto& SpawnInfo : PendingSpawns)
	{
		auto OnSpawned = [WeakThis](const FTrainingOption& Opt, AActor* Actor, int32 WaveID)
		{
			if (WeakThis.IsValid())
			{
				// Callback to determine when the wave is fully spawned.
				WeakThis->OnUnitSpawnedForWave(Opt, Actor, WaveID);
			}
		};

		//If this ever fails, give the supply back immediately,
		// and decrement our awaiting counter to not hang forever.
		if (!M_AsyncSpawner->AsyncSpawnOptionAtLocation(
			SpawnInfo.Option,
			SpawnInfo.Location,
			this,
			AttackWave->UniqueWaveID,
			OnSpawned))
		{
			M_EnemyController->AddToWaveSupply(1);
			AttackWave->AwaitingSpawnsTillStartMoving--;
		}
	}

	return true;
}

bool UEnemyWaveController::GetRandomAttackWaveElementOption(const FAttackWaveElement& Element,
                                                            FTrainingOption& OutPickedOption) const
{
	const int32 Index = FMath::RandRange(0, Element.UnitOptions.Num() - 1);
	if (Element.UnitOptions.IsValidIndex(Index))
	{
		OutPickedOption = Element.UnitOptions[Index];
		return true;
	}
	RTSFunctionLibrary::ReportError(
		"Could not get random attack wave element option! Index: " + FString::FromInt(Index) +
		"\n At UEnemyWaveController::GetRandomAttackWaveElementOption()");
	return false;
}

void UEnemyWaveController::OnUnitSpawnedForWave(const FTrainingOption& TrainingOption, AActor* SpawnedActor,
                                                const int32 ID)
{
	FAttackWave* AttackWave = GetAttackWaveByID(ID);
	if (not AttackWave)
	{
		// We subtracted one when we enqueued this spawn—give it back.
		if (EnsureEnemyControllerIsValid())
		{
			M_EnemyController->AddToWaveSupply(1);
		}
		return;
	}
	if (not IsValid(SpawnedActor))
	{
		OnWaveUnitFailedToSpawn(TrainingOption, ID);
	}
	else
	{
		AttackWave->SpawnedWaveUnits.Add(SpawnedActor);
	}

	AttackWave->AwaitingSpawnsTillStartMoving--;
	// Did we just spawn the last wave unit?
	if (AttackWave->AwaitingSpawnsTillStartMoving <= 0)
	{
		OnWaveCompletedSpawn(AttackWave);
	}
}

void UEnemyWaveController::OnWaveUnitFailedToSpawn(
	const FTrainingOption& TrainingOption,
	const int32 WaveID)
{
	// If the wave is gone, we need to refund here too.
	FAttackWave* AttackWave = GetAttackWaveByID(WaveID);
	if (!AttackWave)
	{
		if (EnsureEnemyControllerIsValid())
		{
			M_EnemyController->AddToWaveSupply(1);
		}
		return;
	}
	// Make sure we do not hang waiting for this unit that never spawned!
	AttackWave->AwaitingSpawnsTillStartMoving--;
	const FString OptName = TrainingOption.GetDisplayName();
	RTSFunctionLibrary::ReportError(
		"Wave unit failed to spawn: " + OptName +
		" (wave " + FString::FromInt(WaveID) + "). Refunding supply.");

	// Refund supply for this one unit
	if (EnsureEnemyControllerIsValid())
	{
		M_EnemyController->AddToWaveSupply(1);
	}
}


void UEnemyWaveController::OnWaveCompletedSpawn(FAttackWave* Wave)
{
	if (not Wave || not EnsureEnemyControllerIsValid())
	{
		return;
	}

	TArray<ATankMaster*> WaveTanks;
	TArray<ASquadController*> WaveSquads;
	ExtractTanksAndSquadsFromWaveActors(WaveTanks, WaveSquads, Wave->SpawnedWaveUnits);
	const FVector AverageSpawnLocation = GetAverageSpawnLocation(WaveSquads, WaveTanks);
	// Start the formation movement using the enemy formation controller; note that at this point
	// all the wave resources are paid for and that if a wave unit dies during the formation or after
	// the enemy formation controller will callback on the enemy controller to adjust the wave supply accordingly.
	if (Wave->bIsAttackMoveWave)
	{
		M_EnemyController->MoveAttackMoveFormationToLocation(
			WaveSquads,
			WaveTanks,
			Wave->Waypoints,
			Wave->FinalWaypointDirection,
			Wave->MaxFormationWidth,
			Wave->FormationOffsetMlt,
			Wave->AttackMoveSettings,
			AverageSpawnLocation);
	}
	else
	{
		M_EnemyController->MoveFormationToLocation(
			WaveSquads,
			WaveTanks,
			Wave->Waypoints,
			Wave->FinalWaypointDirection,
			Wave->MaxFormationWidth,
			Wave->FormationOffsetMlt,
			AverageSpawnLocation);
	}

	if (Wave->bIsSingleWave)
	{
		RemoveAttackWaveByIDAndInvalidateTimer(Wave);
		return;
	}

	const bool bInstantStartWave = false;
	CreateAttackWaveTimer(Wave, bInstantStartWave);
}

FVector UEnemyWaveController::GetAverageSpawnLocation(
	const TArray<ASquadController*>& SquadControllers,
	const TArray<ATankMaster*>& TankMasters) const
{
	FVector AccumulatedLocation = FVector::ZeroVector;
	int32 ValidUnitCount = 0;

	for (const ASquadController* SquadController : SquadControllers)
	{
		if (not IsValid(SquadController))
		{
			continue;
		}
		AccumulatedLocation += SquadController->GetActorLocation();
		ValidUnitCount++;
	}

	for (const ATankMaster* TankMaster : TankMasters)
	{
		if (not IsValid(TankMaster))
		{
			continue;
		}
		AccumulatedLocation += TankMaster->GetActorLocation();
		ValidUnitCount++;
	}

	if (ValidUnitCount <= 0)
	{
		RTSFunctionLibrary::ReportError(
			"No valid units found when calculating average spawn location. "
			"\nAt UEnemyWaveController::GetAverageSpawnLocation()");
		return FVector::ZeroVector;
	}

	return AccumulatedLocation / static_cast<float>(ValidUnitCount);
}

void UEnemyWaveController::ExtractTanksAndSquadsFromWaveActors(TArray<ATankMaster*>& OutTankMasters,
                                                               TArray<ASquadController*>& OutSquadControllers,
                                                               const TArray<AActor*>& InWaveActors) const
{
	for (const auto EachWaveActor : InWaveActors)
	{
		if (not EachWaveActor)
		{
			M_EnemyController->AddToWaveSupply(1);
			continue;
		}
		if (EachWaveActor->IsA<ATankMaster>())
		{
			OutTankMasters.Add(Cast<ATankMaster>(EachWaveActor));
			continue;
		}
		if (EachWaveActor->IsA<ASquadController>())
		{
			OutSquadControllers.Add(Cast<ASquadController>(EachWaveActor));
			continue;
		}
		RTSFunctionLibrary::ReportError(
			"Invalid wave actor type! Expected TankMaster or SquadController, got: " + EachWaveActor->GetClass()->
			GetName());
		M_EnemyController->AddToWaveSupply(1);
		EachWaveActor->Destroy();
	}
}


bool UEnemyWaveController::GetIsValidAsyncSpawner()
{
	if (M_AsyncSpawner.IsValid())
	{
		return true;
	}
	M_AsyncSpawner = FRTS_Statics::GetAsyncSpawner(this);
	return M_AsyncSpawner.IsValid();
}

bool UEnemyWaveController::GetIsValidWave(const EEnemyWaveType WaveType,
                                          const TArray<FAttackWaveElement>& WaveElements,
                                          const float WaveInterval,
                                          const float IntervalVarianceFactor, const TArray<FVector>& Waypoints,
                                          AActor* WaveCreator,
                                          const TArray<TWeakObjectPtr<AActor>>& WaveTimerAffectingBuildings,
                                          const float PerAffectingBuildingTimerFraction
) const
{
	if (WaveType == EEnemyWaveType::Wave_None)
	{
		RTSFunctionLibrary::ReportError("Invalid enemy wave: Wave type is set to None.");
		return false;
	}
	if (not GetAreWaveElementsValid(WaveElements))
	{
		return false;
	}

	if (WaveInterval <= 0.f || IntervalVarianceFactor >= 1.f || IntervalVarianceFactor < 0.f)
	{
		RTSFunctionLibrary::ReportError("Invalid enemy wave: interval or variance factor not set correctly:"
			"\n wave inteval: " + FString::SanitizeFloat(WaveInterval) +
			"\n interval variance factor: " + FString::SanitizeFloat(IntervalVarianceFactor));
		return false;
	}
	if (not GetAreWavePointsValid(Waypoints))
	{
		return false;
	}
	if (not GetIsValidWaveType(WaveType, WaveCreator, WaveTimerAffectingBuildings, PerAffectingBuildingTimerFraction))
	{
		return false;
	}
	return true;
}

bool UEnemyWaveController::GetIsValidSingleAttackWave(
	const EEnemyWaveType WaveType,
	const TArray<FAttackWaveElement>& WaveElements,
	const TArray<FVector>& Waypoints,
	AActor* WaveCreator) const
{
	if (WaveType == EEnemyWaveType::Wave_OwningBuildingAndPowerGenerators)
	{
		RTSFunctionLibrary::ReportError(
			"Invalid enemy wave: Single attack waves do not support Wave_OwningBuildingAndPowerGenerators.");
		return false;
	}
	if (WaveType == EEnemyWaveType::Wave_None)
	{
		RTSFunctionLibrary::ReportError("Invalid enemy wave: Wave type is set to None.");
		return false;
	}
	if (not GetAreWaveElementsValid(WaveElements))
	{
		return false;
	}
	if (not GetAreWavePointsValid(Waypoints))
	{
		return false;
	}
	if (not GetIsValidWaveType(WaveType, WaveCreator))
	{
		return false;
	}
	return true;
}

bool UEnemyWaveController::GetIsValidAttackMoveWaveSettings(
	const float HelpOffsetRadiusMltMax,
	const float HelpOffsetRadiusMltMin,
	const int32 MaxTriesFindNavPointForHelpOffset,
	const float ProjectionScale) const
{
	if (HelpOffsetRadiusMltMin <= 0.f || HelpOffsetRadiusMltMax <= 0.f)
	{
		RTSFunctionLibrary::ReportError(
			"Invalid attack move wave settings: help offset multipliers must be greater than zero.");
		return false;
	}
	if (HelpOffsetRadiusMltMax < HelpOffsetRadiusMltMin)
	{
		RTSFunctionLibrary::ReportError(
			"Invalid attack move wave settings: max help offset multiplier is smaller than min.");
		return false;
	}
	if (MaxTriesFindNavPointForHelpOffset <= 0)
	{
		RTSFunctionLibrary::ReportError(
			"Invalid attack move wave settings: MaxTriesFindNavPointForHelpOffset must be greater than zero.");
		return false;
	}
	if (ProjectionScale <= 0.f)
	{
		RTSFunctionLibrary::ReportError(
			"Invalid attack move wave settings: ProjectionScale must be greater than zero.");
		return false;
	}
	return true;
}

bool UEnemyWaveController::GetAreWaveElementsValid(TArray<FAttackWaveElement> WaveElements) const
{
	if (WaveElements.Num() == 0)
	{
		RTSFunctionLibrary::ReportError("Invalid enemy wave: No wave elements set for the wave.");
		return false;
	}

	for (auto EachWaveElm : WaveElements)
	{
		if (EachWaveElm.SpawnLocation == FVector::ZeroVector)
		{
			RTSFunctionLibrary::ReportError("Invalid enemy wave: One of the wave elements has a zero spawn location.");
			return false;
		}
		if (EachWaveElm.UnitOptions.Num() == 0)
		{
			RTSFunctionLibrary::ReportError("Invalid enemy wave: One of the wave elements has no unit options set.");
			return false;
		}
		for (auto EachOption : EachWaveElm.UnitOptions)
		{
			if (not GetIsValidTrainingOption(EachOption))
			{
				RTSFunctionLibrary::ReportError(
					"Invalid enemy wave: One of the wave elements has an invalid training option.");
				return false;
			}
		}
	}
	return true;
}

bool UEnemyWaveController::GetAreWavePointsValid(const TArray<FVector>& Waypoints) const
{
	if (Waypoints.Num() == 0)
	{
		RTSFunctionLibrary::ReportError("Invalid enemy wave: No waypoints set for the wave.");
		return false;
	}
	// check if none of the waypoints are zero vectors.
	for (const auto& EachWaypoint : Waypoints)
	{
		if (EachWaypoint.IsZero())
		{
			RTSFunctionLibrary::ReportError("Invalid enemy wave: One of the waypoints is a zero vector.");
			return false;
		}
	}
	return true;
}

bool UEnemyWaveController::GetIsValidTrainingOption(const FTrainingOption& TrainingOption) const
{
	if (TrainingOption.UnitType == EAllUnitType::UNType_None || TrainingOption.SubtypeValue == 0)
	{
		RTSFunctionLibrary::ReportError("Invalid training option: Unit type is set to None or zero subtype value.");
		return false;
	}
	return true;
}

bool UEnemyWaveController::GetIsValidWaveType(const EEnemyWaveType WaveType, AActor* WaveCreator,
                                              const TArray<TWeakObjectPtr<AActor>>& WaveTimerAffectingBuildings,
                                              const float PerAffectingBuildingTimerFraction) const
{
	const bool bNeedsOwningBuildingValid = (WaveType == EEnemyWaveType::Wave_OwningBuilding ||
		WaveType == EEnemyWaveType::Wave_OwningBuildingAndPowerGenerators);
	if (not RTSFunctionLibrary::RTSIsValid(WaveCreator) && bNeedsOwningBuildingValid)
	{
		RTSFunctionLibrary::ReportError(
			"Invalid enemy wave: Set to owning building for wave but the provided building is not valid!");
		return false;
	}
	if (WaveType == EEnemyWaveType::Wave_OwningBuildingAndPowerGenerators)
	{
		bool bHasInvalidGenerator = false;
		for (auto EachGenerator : WaveTimerAffectingBuildings)
		{
			if (not EachGenerator.IsValid())
			{
				bHasInvalidGenerator = true;
				break;
			}
			if (not RTSFunctionLibrary::RTSIsValid(EachGenerator.Get()))
			{
				bHasInvalidGenerator = true;
				break;
			}
		}
		if (bHasInvalidGenerator)
		{
			RTSFunctionLibrary::ReportError(
				"Invalid enemy wave: Set to owning building and power generators for wave but one of the provided generators is not valid!");
			return false;
		}
		if (PerAffectingBuildingTimerFraction <= 0.f)
		{
			RTSFunctionLibrary::ReportError(
				"Invalid enemy wave: Per affecting building timer fraction is set to zero or less for wave type Wave_OwningBuildingAndPowerGenerators!");
			return false;
		}
	}
	return true;
}

void UEnemyWaveController::Debug(const FString& Message, const FColor& Color) const
{
	if constexpr (DeveloperSettings::Debugging::GEnemyController_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString(Message, Color);
	}
}

bool UEnemyWaveController::EnsureIDIsUnique(const int32 ID)
{
	for (const auto EachWave : M_AttackWaves)
	{
		if (EachWave.UniqueWaveID == ID)
		{
			RTSFunctionLibrary::ReportError(
				"Enemy wave ID is not unique! ID: " + FString::FromInt(ID) +
				"\n At UEnemyWaveController::EnsureIDIsUnique()");
			return false;
		}
	}
	return true;
}
