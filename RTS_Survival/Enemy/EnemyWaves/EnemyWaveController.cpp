// Copyright (C) Bas Blokzijl - All rights reserved.


#include "EnemyWaveController.h"

#include "Algo/Unique.h"
#include "NavigationSystem.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

#if defined(RTS_WITH_ENEMY_AI_SHIPPING_TESTS) && RTS_WITH_ENEMY_AI_SHIPPING_TESTS
namespace EnemyWaveShippingTestConstants
{
	const int32 MaxFormationHandoffHistory = 512;
}
#endif

UEnemyWaveController::UEnemyWaveController()
{
	PrimaryComponentTick.bCanEverTick = false;

	PrimaryComponentTick.bStartWithTickEnabled = false;
	CacheGenerationSeedFromGameInstance();
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

void UEnemyWaveController::StartSingleRandomPatrolWithAttackMoveWave(
	const EEnemyWaveType WaveType,
	const TArray<FAttackWaveElement>& WaveElements,
	const TArray<FVector>& PatrolPoints,
	const int32 OverrideFirstPatrolPointIndex,
	const int32 AmountIterationsAtPatrolPoint,
	const float GuardTimePerPatrolPointIteration,
	const float GuardSphereRadius,
	const int32 MaxFormationWidth,
	const float TimeTillPatrol,
	AActor* WaveCreator,
	const float FormationOffsetMultiplier,
	const float HelpOffsetRadiusMltMax,
	const float HelpOffsetRadiusMltMin,
	const float MaxAttackTimeBeforeAdvancingToNextWayPoint,
	const int32 MaxTriesFindNavPointForHelpOffset,
	const float ProjectionScale)
{
	if (not GetIsValidSingleAttackWave(WaveType, WaveElements, PatrolPoints, WaveCreator))
	{
		return;
	}
	if (PatrolPoints.Num() < 2)
	{
		RTSFunctionLibrary::ReportError("Random patrol with attack move wave requires at least two patrol points.");
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
			"Invalid async spawner or enemy controller for random patrol with attack move wave.");
		return;
	}

	FAttackMoveWaveSettings AttackMoveSettings;
	AttackMoveSettings.HelpOffsetRadiusMltMax = HelpOffsetRadiusMltMax;
	AttackMoveSettings.HelpOffsetRadiusMltMin = HelpOffsetRadiusMltMin;
	AttackMoveSettings.MaxAttackTimeBeforeAdvancingToNextWayPoint = MaxAttackTimeBeforeAdvancingToNextWayPoint;
	AttackMoveSettings.MaxTriesFindNavPointForHelpOffset = MaxTriesFindNavPointForHelpOffset;
	AttackMoveSettings.ProjectionScale = ProjectionScale;

	FRandomPatrolWithAttackMoveSettings RandomPatrolSettings;
	RandomPatrolSettings.OverrideFirstPatrolPointIndex = OverrideFirstPatrolPointIndex;
	RandomPatrolSettings.AmountIterationsAtPatrolPoint = AmountIterationsAtPatrolPoint;
	RandomPatrolSettings.GuardTimePerPatrolPointIteration = GuardTimePerPatrolPointIteration;
	RandomPatrolSettings.GuardSphereRadius = GuardSphereRadius;
	RandomPatrolSettings.AttackMoveSettings = AttackMoveSettings;

	const int32 UniqueWaveID = GetUniqueWaveID();
	const float ClampedTimeTillPatrol = FMath::Max(TimeTillPatrol, 0.f);
	const bool bIsSingleWave = true;
	constexpr bool bIsAttackMoveWave = false;
	constexpr bool bIsRandomPatrolWithAttackMoveWave = true;
	if (not CreateNewAttackWaveStruct(
		UniqueWaveID,
		WaveType,
		WaveElements,
		ClampedTimeTillPatrol,
		0.f,
		PatrolPoints,
		FRotator::ZeroRotator,
		MaxFormationWidth,
		WaveCreator,
		{},
		0.f,
		bIsSingleWave,
		FormationOffsetMultiplier,
		bIsAttackMoveWave,
		AttackMoveSettings,
		bIsRandomPatrolWithAttackMoveWave,
		RandomPatrolSettings))
	{
		RTSFunctionLibrary::ReportError("failed to create struct for single random patrol with attack move wave!");
		return;
	}

	FAttackWave* AttackWave = GetAttackWaveByID(UniqueWaveID);
	if (not AttackWave)
	{
		return;
	}
	const bool bInstantStart = ClampedTimeTillPatrol <= 0.f;
	if (not CreateAttackWaveTimer(AttackWave, bInstantStart))
	{
		RTSFunctionLibrary::ReportError(
			"Failed to start random patrol with attack move wave timer for single wave, will remove the struct!");
		RemoveAttackWaveByIDAndInvalidateTimer(AttackWave);
	}
}

void UEnemyWaveController::InitWaveController(AEnemyController* EnemyController)
{
	M_EnemyController = EnemyController;
	CacheGenerationSeedFromGameInstance();
}


void UEnemyWaveController::BeginPlay()
{
	Super::BeginPlay();

	// Re-bind the owning controller at runtime; constructor-time InitWaveController(this) captures the
	// class default object for a placed controller, which would route completed-wave formations to the CDO.
	M_EnemyController = Cast<AEnemyController>(GetOwner());

	M_NavigationSystem = UNavigationSystemV1::GetCurrent(GetWorld());
	CacheGenerationSeedFromGameInstance();
}

bool UEnemyWaveController::EnsureEnemyControllerIsValid()
{
	// Always re-bind to the actual owner so a stale reference (e.g. the class default object captured at
	// construction) can never be used; fall back to the cached value only if the owner is momentarily
	// unavailable (e.g. during teardown).
	if (AEnemyController* OwningController = Cast<AEnemyController>(GetOwner()))
	{
		M_EnemyController = OwningController;
		return true;
	}
	if (M_EnemyController.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_EnemyController"),
		TEXT("UEnemyWaveController::EnsureEnemyControllerIsValid"),
		this);
	return false;
}

void UEnemyWaveController::CacheGenerationSeedFromGameInstance()
{
	UWorld* const World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	UGameInstance* const GameInstance = World->GetGameInstance();
	URTSGameInstance* const RTSGameInstance = Cast<URTSGameInstance>(GameInstance);
	if (not IsValid(RTSGameInstance))
	{
		RTSFunctionLibrary::ReportError("Enemy wave controller could not cache generation seed from game instance.");
		return;
	}

	const FCampaignGenerationSettings CampaignGenerationSettings = RTSGameInstance->GetCampaignGenerationSettings();
	M_CachedGenerationSeed = CampaignGenerationSettings.GenerationSeed;
}

int32 UEnemyWaveController::GetSeededIndex(const int32 OptionCount, const int32 DecisionSalt) const
{
	if (OptionCount <= 0)
	{
		return INDEX_NONE;
	}

	const uint32 CombinedSeed = static_cast<uint32>(M_CachedGenerationSeed)
		+ static_cast<uint32>(DecisionSalt)
		+ static_cast<uint32>(M_SeedDecisionCounter);
	FRandomStream SeededRandomStream(static_cast<int32>(CombinedSeed));
	++M_SeedDecisionCounter;
	return SeededRandomStream.RandRange(0, OptionCount - 1);
}

float UEnemyWaveController::GetSeededFloatInRange(const float MinValue, const float MaxValue, const int32 DecisionSalt) const
{
	const uint32 CombinedSeed = static_cast<uint32>(M_CachedGenerationSeed)
		+ static_cast<uint32>(DecisionSalt)
		+ static_cast<uint32>(M_SeedDecisionCounter);
	FRandomStream SeededRandomStream(static_cast<int32>(CombinedSeed));
	++M_SeedDecisionCounter;
	return SeededRandomStream.FRandRange(MinValue, MaxValue);
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
	FAttackWave* AttackWave = FindAttackWaveByID(UniqueID);
	if (AttackWave)
	{
		return AttackWave;
	}
	RTSFunctionLibrary::ReportError("Could not find attack wave with ID: " + FString::FromInt(UniqueID));
	return nullptr;
}

FAttackWave* UEnemyWaveController::FindAttackWaveByID(const int32 UniqueID)
{
	return M_AttackWaves.FindByPredicate([UniqueID](const FAttackWave& Wave)
	{
		return Wave.UniqueWaveID == UniqueID;
	});
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
                                                     const FAttackMoveWaveSettings& AttackMoveSettings,
                                                     const bool bIsRandomPatrolWithAttackMoveWave,
                                                     const FRandomPatrolWithAttackMoveSettings& RandomPatrolWithAttackMoveSettings)
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
	NewAttackWave.bIsRandomPatrolWithAttackMoveWave = bIsRandomPatrolWithAttackMoveWave;
	NewAttackWave.RandomPatrolWithAttackMoveSettings = RandomPatrolWithAttackMoveSettings;
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
		if (WeakThis->SpawnUnitsForAttackWave(Wave))
		{
			return;
		}

		RTSFunctionLibrary::PrintString(
			"Wave iteration could not submit any spawn requests, possibly due "
			"to no wave supply left. Skipping this iteration.");
		WeakThis->FinalizeWaveIterationWithoutSpawnedUnits(ID);
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
	return GetSeededFloatInRange(MinInterval, MaxInterval, AttackWave->UniqueWaveID + 701)
		* GetWaveTimeMltDependingOnGenerators(AttackWave);
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
	if (not AttackWave || not GetIsValidAsyncSpawner() || not EnsureEnemyControllerIsValid())
	{
		return false;
	}

	const int32 WaveID = AttackWave->UniqueWaveID;
	const TArray<FAttackWaveElement> WaveElements = AttackWave->WaveElements;
	AttackWave->ResetForSpawningNewWave();

	struct FPendingSpawn
	{
		FTrainingOption TrainingOption;
		FVector SpawnLocation;
	};

	TArray<FPendingSpawn> PendingSpawns;
	PendingSpawns.Reserve(WaveElements.Num());

	for (const FAttackWaveElement& Element : WaveElements)
	{
		FTrainingOption PickedOption;
		if (not TryReserveSupplyForWaveElement(Element, PickedOption))
		{
			continue;
		}

		PendingSpawns.Add({PickedOption, Element.SpawnLocation});
	}

	FAttackWave* CurrentWave = FindAttackWaveByID(WaveID);
	if (not CurrentWave)
	{
		for (const FPendingSpawn& PendingSpawn : PendingSpawns)
		{
			OnWaveUnitFailedToSpawn(PendingSpawn.TrainingOption, WaveID);
		}
		return not PendingSpawns.IsEmpty();
	}

	CurrentWave->AwaitingSpawnsTillStartMoving = PendingSpawns.Num();
	if (PendingSpawns.IsEmpty())
	{
		return false;
	}

	for (const FPendingSpawn& PendingSpawn : PendingSpawns)
	{
		SubmitSpawnRequestForWave(
			PendingSpawn.TrainingOption,
			PendingSpawn.SpawnLocation,
			WaveID);
	}

	return true;
}

bool UEnemyWaveController::TryReserveSupplyForWaveElement(
	const FAttackWaveElement& Element,
	FTrainingOption& OutPickedOption)
{
	if (not GetRandomAttackWaveElementOption(Element, OutPickedOption))
	{
		return false;
	}
	if (M_EnemyController->GetEnemyWaveSupply() <= 0)
	{
		RTSFunctionLibrary::PrintString("Enemy has no wave supply left; skipping this element.");
		return false;
	}

	M_EnemyController->AddToWaveSupply(-1);
	return true;
}

void UEnemyWaveController::SubmitSpawnRequestForWave(
	const FTrainingOption& TrainingOption,
	const FVector& SpawnLocation,
	const int32 WaveID)
{
	if (not FindAttackWaveByID(WaveID) || not GetIsValidAsyncSpawner())
	{
		OnWaveUnitFailedToSpawn(TrainingOption, WaveID);
		return;
	}

	ARTSAsyncSpawner* AsyncSpawner = M_AsyncSpawner.Get();
	if (not IsValid(AsyncSpawner))
	{
		OnWaveUnitFailedToSpawn(TrainingOption, WaveID);
		return;
	}

	const TSharedRef<bool> RequestResolved = MakeShared<bool>(false);
	const TWeakObjectPtr<UEnemyWaveController> WeakThis(this);
	auto OnSpawned = [WeakThis, RequestResolved, WaveID](
		const FTrainingOption& SpawnedTrainingOption,
		AActor* SpawnedActor,
		const int32)
	{
		if (*RequestResolved)
		{
			return;
		}

		*RequestResolved = true;
		if (WeakThis.IsValid())
		{
			WeakThis->OnUnitSpawnedForWave(SpawnedTrainingOption, SpawnedActor, WaveID);
		}
	};

	const bool bRequestStarted = AsyncSpawner->AsyncSpawnOptionAtLocation(
		TrainingOption,
		SpawnLocation,
		this,
		WaveID,
		OnSpawned,
		FRotator::ZeroRotator);
	if (bRequestStarted || *RequestResolved)
	{
		return;
	}

	*RequestResolved = true;
	OnWaveUnitFailedToSpawn(TrainingOption, WaveID);
}

bool UEnemyWaveController::GetRandomAttackWaveElementOption(const FAttackWaveElement& Element,
                                                            FTrainingOption& OutPickedOption) const
{
	const int32 Index = GetSeededIndex(Element.UnitOptions.Num(), 811);
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
	if (not IsValid(SpawnedActor))
	{
		OnWaveUnitFailedToSpawn(TrainingOption, ID);
		return;
	}

	FAttackWave* AttackWave = FindAttackWaveByID(ID);
	if (not AttackWave)
	{
		RTSFunctionLibrary::ReportError(
			"Spawned a wave unit after its wave was removed. Destroying the orphaned unit.");
		SpawnedActor->Destroy();
		OnWaveUnitFailedToSpawn(TrainingOption, ID);
		return;
	}

	AttackWave->SpawnedWaveUnits.Add(SpawnedActor);
	CompletePendingSpawnForWave(ID);
}

void UEnemyWaveController::OnWaveUnitFailedToSpawn(
	const FTrainingOption& TrainingOption,
	const int32 WaveID)
{
#if defined(RTS_WITH_ENEMY_AI_SHIPPING_TESTS) && RTS_WITH_ENEMY_AI_SHIPPING_TESTS
	M_ShippingTest_FailedSpawnCount++;
#endif

	const bool bWaveExists = FindAttackWaveByID(WaveID) != nullptr;
	const FString OptionName = TrainingOption.GetDisplayName();
	RTSFunctionLibrary::ReportError(
		"Wave unit failed to spawn: " + OptionName +
		" (wave " + FString::FromInt(WaveID) + "). Refunding supply.");
	RefundSupplyForFailedWaveSpawn();

	if (bWaveExists)
	{
		CompletePendingSpawnForWave(WaveID);
	}
}

void UEnemyWaveController::CompletePendingSpawnForWave(const int32 WaveID)
{
	FAttackWave* AttackWave = FindAttackWaveByID(WaveID);
	if (not AttackWave)
	{
		return;
	}
	if (AttackWave->AwaitingSpawnsTillStartMoving <= 0)
	{
		RTSFunctionLibrary::ReportError(
			"Received an extra wave spawn completion for wave " + FString::FromInt(WaveID) + ".");
		return;
	}

	AttackWave->AwaitingSpawnsTillStartMoving--;
	if (AttackWave->AwaitingSpawnsTillStartMoving > 0)
	{
		return;
	}
	if (AttackWave->SpawnedWaveUnits.IsEmpty())
	{
		FinalizeWaveIterationWithoutSpawnedUnits(WaveID);
		return;
	}

	OnWaveCompletedSpawn(AttackWave);
}

void UEnemyWaveController::FinalizeWaveIterationWithoutSpawnedUnits(const int32 WaveID)
{
	FAttackWave* AttackWave = FindAttackWaveByID(WaveID);
	if (not AttackWave)
	{
		return;
	}
	if (AttackWave->bIsSingleWave)
	{
		RemoveAttackWaveByIDAndInvalidateTimer(AttackWave);
		return;
	}

	constexpr bool bInstantStart = false;
	if (not CreateAttackWaveTimer(AttackWave, bInstantStart))
	{
		RTSFunctionLibrary::ReportError(
			"Failed to schedule the next iteration after a wave spawned no valid units.");
		FAttackWave* FailedWave = FindAttackWaveByID(WaveID);
		RemoveAttackWaveByIDAndInvalidateTimer(FailedWave);
	}
}

void UEnemyWaveController::RefundSupplyForFailedWaveSpawn()
{
	if (not EnsureEnemyControllerIsValid())
	{
		return;
	}

	M_EnemyController->AddToWaveSupply(1);
}

void UEnemyWaveController::FinishWaveAfterFormationStarted(const int32 WaveID, const bool bIsSingleWave)
{
	FAttackWave* CurrentWave = FindAttackWaveByID(WaveID);
	if (not CurrentWave)
	{
		return;
	}
	if (bIsSingleWave)
	{
		RemoveAttackWaveByIDAndInvalidateTimer(CurrentWave);
		return;
	}

	constexpr bool bInstantStartWave = false;
	if (not CreateAttackWaveTimer(CurrentWave, bInstantStartWave))
	{
		RTSFunctionLibrary::ReportError("Failed to schedule the next completed attack wave iteration.");
		FAttackWave* FailedWave = FindAttackWaveByID(WaveID);
		RemoveAttackWaveByIDAndInvalidateTimer(FailedWave);
	}
}


void UEnemyWaveController::OnWaveCompletedSpawn(FAttackWave* Wave)
{
	if (not Wave || not EnsureEnemyControllerIsValid())
	{
		return;
	}

	const FAttackWave CompletedWave = *Wave;
	const int32 WaveID = CompletedWave.UniqueWaveID;
	TArray<ATankMaster*> WaveTanks;
	TArray<ASquadController*> WaveSquads;
	const int32 InvalidSpawnedActorCount = ExtractTanksAndSquadsFromWaveActors(
		WaveTanks,
		WaveSquads,
		CompletedWave.SpawnedWaveUnits);

#if defined(RTS_WITH_ENEMY_AI_SHIPPING_TESTS) && RTS_WITH_ENEMY_AI_SHIPPING_TESTS
	M_ShippingTest_FailedSpawnCount += InvalidSpawnedActorCount;
#else
	(void)InvalidSpawnedActorCount;
#endif

	if (WaveTanks.IsEmpty() && WaveSquads.IsEmpty())
	{
		FinalizeWaveIterationWithoutSpawnedUnits(WaveID);
		return;
	}

#if defined(RTS_WITH_ENEMY_AI_SHIPPING_TESTS) && RTS_WITH_ENEMY_AI_SHIPPING_TESTS
	if (CompletedWave.bIsAttackMoveWave && not CompletedWave.bIsRandomPatrolWithAttackMoveWave)
	{
		M_ShippingTest_CompletedAttackMoveWaveCount++;
	}
	if (CompletedWave.bIsRandomPatrolWithAttackMoveWave)
	{
		M_ShippingTest_CompletedPatrolWaveCount++;
	}
#endif

	const FVector AverageSpawnLocation = GetAverageSpawnLocation(WaveSquads, WaveTanks);
#if defined(RTS_WITH_ENEMY_AI_SHIPPING_TESTS) && RTS_WITH_ENEMY_AI_SHIPPING_TESTS
	const TArray<int32> FormationHistoryIDsBefore = ShippingTest_GetFormationHistoryIDs();
#endif
	StartMovementForCompletedWave(CompletedWave, WaveSquads, WaveTanks, AverageSpawnLocation);
#if defined(RTS_WITH_ENEMY_AI_SHIPPING_TESTS) && RTS_WITH_ENEMY_AI_SHIPPING_TESTS
	ShippingTest_RecordFormationHandoff(
		CompletedWave,
		WaveSquads,
		WaveTanks,
		FormationHistoryIDsBefore);
#endif
	FinishWaveAfterFormationStarted(WaveID, CompletedWave.bIsSingleWave);
}

void UEnemyWaveController::StartMovementForCompletedWave(
	const FAttackWave& CompletedWave,
	const TArray<ASquadController*>& WaveSquads,
	const TArray<ATankMaster*>& WaveTanks,
	const FVector& AverageSpawnLocation) const
{
	// Start the formation movement using the enemy formation controller; note that at this point
	// all the wave resources are paid for and that if a wave unit dies during the formation or after
	// the enemy formation controller will callback on the enemy controller to adjust the wave supply accordingly.
	if (CompletedWave.bIsAttackMoveWave)
	{
		M_EnemyController->MoveAttackMoveFormationToLocation(
			WaveSquads,
			WaveTanks,
			CompletedWave.Waypoints,
			CompletedWave.FinalWaypointDirection,
			CompletedWave.MaxFormationWidth,
			CompletedWave.FormationOffsetMlt,
			CompletedWave.AttackMoveSettings,
			AverageSpawnLocation);
	}
	else if (CompletedWave.bIsRandomPatrolWithAttackMoveWave)
	{
		M_EnemyController->MoveRandomPatrolWithAttackMoveFormation(
			WaveSquads,
			WaveTanks,
			CompletedWave.Waypoints,
			CompletedWave.RandomPatrolWithAttackMoveSettings.OverrideFirstPatrolPointIndex,
			CompletedWave.RandomPatrolWithAttackMoveSettings.AmountIterationsAtPatrolPoint,
			CompletedWave.RandomPatrolWithAttackMoveSettings.GuardTimePerPatrolPointIteration,
			CompletedWave.RandomPatrolWithAttackMoveSettings.GuardSphereRadius,
			CompletedWave.MaxFormationWidth,
			CompletedWave.FormationOffsetMlt,
			CompletedWave.RandomPatrolWithAttackMoveSettings.AttackMoveSettings,
			AverageSpawnLocation);
	}
	else
	{
		M_EnemyController->MoveFormationToLocation(
			WaveSquads,
			WaveTanks,
			CompletedWave.Waypoints,
			CompletedWave.FinalWaypointDirection,
			CompletedWave.MaxFormationWidth,
			CompletedWave.FormationOffsetMlt,
			AverageSpawnLocation);
	}
}

#if defined(RTS_WITH_ENEMY_AI_SHIPPING_TESTS) && RTS_WITH_ENEMY_AI_SHIPPING_TESTS
TArray<int32> UEnemyWaveController::ShippingTest_GetFormationHistoryIDs()
{
	if (not EnsureEnemyControllerIsValid())
	{
		return {};
	}

	UEnemyFormationController* FormationController = M_EnemyController->GetEnemyFormationController();
	if (not IsValid(FormationController))
	{
		return {};
	}

	const TArray<FEnemyAIShippingFormationProgress> FormationHistory =
		FormationController->ShippingTest_GetFormationProgressHistory();
	TArray<int32> FormationIDs;
	FormationIDs.Reserve(FormationHistory.Num());
	for (const FEnemyAIShippingFormationProgress& Progress : FormationHistory)
	{
		FormationIDs.Add(Progress.FormationID);
	}
	return FormationIDs;
}

int32 UEnemyWaveController::ShippingTest_GetNextWaveIteration(const int32 WaveID) const
{
	for (int32 HandoffIndex = M_ShippingTest_FormationHandoffHistory.Num() - 1;
		HandoffIndex >= 0;
		--HandoffIndex)
	{
		const FEnemyAIShippingWaveFormationHandoff& PreviousHandoff =
			M_ShippingTest_FormationHandoffHistory[HandoffIndex];
		if (PreviousHandoff.WaveID == WaveID)
		{
			return PreviousHandoff.WaveIteration + 1;
		}
	}
	return 1;
}

void UEnemyWaveController::ShippingTest_RecordFormationHandoff(
	const FAttackWave& CompletedWave,
	const TArray<ASquadController*>& WaveSquads,
	const TArray<ATankMaster*>& WaveTanks,
	const TArray<int32>& FormationHistoryIDsBefore)
{
	FEnemyAIShippingWaveFormationHandoff Handoff;
	Handoff.WaveID = CompletedWave.UniqueWaveID;
	Handoff.WaveIteration = ShippingTest_GetNextWaveIteration(CompletedWave.UniqueWaveID);
	Handoff.ResolvedSpawnActorCount = CompletedWave.SpawnedWaveUnits.Num();
	Handoff.AcceptedSquadCount = WaveSquads.Num();
	Handoff.AcceptedTankCount = WaveTanks.Num();
	Handoff.AcceptedUnitCount = WaveSquads.Num() + WaveTanks.Num();
	Handoff.RequestedWaypointCount = CompletedWave.Waypoints.Num();
	Handoff.ExpectedFormationKind = ShippingTest_GetExpectedFormationKind(CompletedWave);
	if (not CompletedWave.Waypoints.IsEmpty())
	{
		Handoff.RequestedFirstDestination = CompletedWave.Waypoints[0];
		Handoff.RequestedFinalDestination = CompletedWave.Waypoints.Last();
	}

	ShippingTest_ResolveFormationHandoff(Handoff, FormationHistoryIDsBefore);
	if (M_ShippingTest_FormationHandoffHistory.Num() >=
		EnemyWaveShippingTestConstants::MaxFormationHandoffHistory)
	{
		M_ShippingTest_FormationHandoffHistory.RemoveAt(0, 1, false);
	}
	M_ShippingTest_FormationHandoffHistory.Add(Handoff);
}

void UEnemyWaveController::ShippingTest_ResolveFormationHandoff(
	FEnemyAIShippingWaveFormationHandoff& InOutHandoff,
	const TArray<int32>& FormationHistoryIDsBefore)
{
	const TArray<int32> FormationHistoryIDsAfter = ShippingTest_GetFormationHistoryIDs();
	TArray<int32> NewFormationIDs;
	for (const int32 FormationID : FormationHistoryIDsAfter)
	{
		if (not FormationHistoryIDsBefore.Contains(FormationID))
		{
			NewFormationIDs.Add(FormationID);
		}
	}

	InOutHandoff.CreatedFormationCount = NewFormationIDs.Num();
	InOutHandoff.bFormationCreated = NewFormationIDs.Num() == 1;
	if (not InOutHandoff.bFormationCreated || not EnsureEnemyControllerIsValid())
	{
		return;
	}

	UEnemyFormationController* FormationController = M_EnemyController->GetEnemyFormationController();
	if (not IsValid(FormationController))
	{
		return;
	}

	FEnemyAIShippingFormationProgress FormationProgress;
	if (not FormationController->ShippingTest_GetFormationProgressByID(
		NewFormationIDs[0],
		FormationProgress))
	{
		return;
	}

	InOutHandoff.FormationID = FormationProgress.FormationID;
	InOutHandoff.FormationUnitCount = FormationProgress.InitialUnitCount;
	InOutHandoff.FormationWaypointCount = FormationProgress.InitialWaypointCount;
	InOutHandoff.FormationPatrolPointCount = FormationProgress.PatrolPointCount;
	InOutHandoff.ActualFormationKind = FormationProgress.FormationKind;
	InOutHandoff.bFormationKindMatches =
		InOutHandoff.ExpectedFormationKind == InOutHandoff.ActualFormationKind;
	InOutHandoff.bFormationUnitCountMatches =
		InOutHandoff.AcceptedUnitCount == InOutHandoff.FormationUnitCount;
}

EEnemyAIShippingFormationKind UEnemyWaveController::ShippingTest_GetExpectedFormationKind(
	const FAttackWave& CompletedWave) const
{
	if (CompletedWave.bIsRandomPatrolWithAttackMoveWave)
	{
		return EEnemyAIShippingFormationKind::RandomPatrolWithAttackMove;
	}
	if (CompletedWave.bIsAttackMoveWave)
	{
		return EEnemyAIShippingFormationKind::AttackMove;
	}
	return EEnemyAIShippingFormationKind::Move;
}
#endif

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

int32 UEnemyWaveController::ExtractTanksAndSquadsFromWaveActors(
	TArray<ATankMaster*>& OutTankMasters,
	TArray<ASquadController*>& OutSquadControllers,
	const TArray<TObjectPtr<AActor>>& InWaveActors) const
{
	int32 InvalidActorCount = 0;
	for (AActor* EachWaveActor : InWaveActors)
	{
		if (not IsValid(EachWaveActor))
		{
			M_EnemyController->AddToWaveSupply(1);
			InvalidActorCount++;
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
		InvalidActorCount++;
		EachWaveActor->Destroy();
	}

	return InvalidActorCount;
}


bool UEnemyWaveController::GetIsValidAsyncSpawner()
{
	if (M_AsyncSpawner.IsValid())
	{
		return true;
	}

	M_AsyncSpawner = FRTS_Statics::GetAsyncSpawner(this);
	if (M_AsyncSpawner.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_AsyncSpawner"),
		TEXT("UEnemyWaveController::GetIsValidAsyncSpawner"),
		this);
	return false;
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
