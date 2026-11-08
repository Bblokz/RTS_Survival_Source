// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GA_AircraftBombing.h"

#include "TimerManager.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UGA_AircraftBombing::ExecuteAbilityAtLocation(const FVector& TargetLocation)
{
	ResetAbilityEndedBroadcast();

	if (not GetIsValidGlobalAbilityManager())
	{
		return;
	}

	UGlobalAbilitiesManager* GlobalAbilitiesManager = GetGlobalAbilityManager();
	const FVector StartLocation = GlobalAbilitiesManager->GetAircraftBombingSpawnLocation(this, TargetLocation);
	const FVector RetreatLocation = GlobalAbilitiesManager->GetAircraftBombingRetreatLocation(this, TargetLocation);
	RequestSpawnAircraftAtStartLocationAsync(StartLocation, TargetLocation, RetreatLocation);
	Super::ExecuteAbilityAtLocation(TargetLocation);
}


void UGA_AircraftBombing::BeginDestroy()
{
	ClearForcedRetreatTimer();
	M_AircraftClassLoadHandles.Reset();
	M_PendingAircraftClassLoads = 0;
	Super::BeginDestroy();
}

void UGA_AircraftBombing::RequestSpawnAircraftAtStartLocationAsync(
	const FVector& StartLocation,
	const FVector& TargetLocation,
	const FVector& RetreatLocation)
{
	const TSoftClassPtr<AAircraftMaster> AircraftClassToLoad = M_AircraftBombingSettings.AircraftClass;
	if (AircraftClassToLoad.IsNull())
	{
		RTSFunctionLibrary::ReportError(TEXT("UGA_AircraftBombing has no aircraft class configured."));
		BroadcastBombingFinishedIfNeeded();
		return;
	}

	TWeakObjectPtr<UGA_AircraftBombing> WeakThis(this);
	// Counted before the request: an already loaded class can complete inside RequestAsyncLoad.
	++M_PendingAircraftClassLoads;
	const TSharedPtr<FStreamableHandle> LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		AircraftClassToLoad.ToSoftObjectPath(),
		FStreamableDelegate::CreateLambda(
			[WeakThis, AircraftClassToLoad, StartLocation, TargetLocation, RetreatLocation]()
			{
				if (not WeakThis.IsValid())
				{
					return;
				}

				UGA_AircraftBombing* StrongThis = WeakThis.Get();
				StrongThis->OnAircraftClassLoaded(
					AircraftClassToLoad,
					StartLocation,
					TargetLocation,
					RetreatLocation);
			})
	);
	if (LoadHandle.IsValid() && not LoadHandle->HasLoadCompleted())
	{
		M_AircraftClassLoadHandles.Add(LoadHandle);
	}
}

void UGA_AircraftBombing::OnAircraftClassLoaded(
	const TSoftClassPtr<AAircraftMaster>& AircraftClassToLoad,
	const FVector& StartLocation,
	const FVector& TargetLocation,
	const FVector& RetreatLocation)
{
	M_PendingAircraftClassLoads = FMath::Max(0, M_PendingAircraftClassLoads - 1);
	M_AircraftClassLoadHandles.RemoveAll([](const TSharedPtr<FStreamableHandle>& Handle)
	{
		return not Handle.IsValid() || Handle->HasLoadCompleted() || Handle->WasCanceled();
	});
	UClass* AircraftClass = AircraftClassToLoad.Get();
	if (not IsValid(AircraftClass))
	{
		RTSFunctionLibrary::ReportError(TEXT("UGA_AircraftBombing failed to async load a valid aircraft class."));
		BroadcastBombingFinishedIfNeeded();
		return;
	}

	AAircraftMaster* SpawnedAircraft = GlobalAbilityHelpers::SpawnAircraftAtLocation(this, AircraftClass, StartLocation);
	if (not IsValid(SpawnedAircraft))
	{
		BroadcastBombingFinishedIfNeeded();
		return;
	}

	M_SpawnedAircraft = SpawnedAircraft;
	FBombingRun& Run = M_Runs.AddDefaulted_GetRef();
	Run.Aircraft = SpawnedAircraft;
	SpawnedAircraft->OnDestroyed.AddUniqueDynamic(this, &UGA_AircraftBombing::OnSpawnedAircraftDestroyed);
	SpawnedAircraft->SetUnitSelectable(false);
	const FVector CarpetEndLocation = BuildCarpetEndLocation(StartLocation, TargetLocation);
	QueueCarpetBombingOrderForNextFrame(SpawnedAircraft, TargetLocation, CarpetEndLocation, RetreatLocation);
}

FVector UGA_AircraftBombing::BuildCarpetEndLocation(const FVector& StartLocation, const FVector& TargetLocation) const
{
	// The carpet starts at the target and runs on for its length in the direction the bomber flies in from its spawn.
	// (Measured from the spawn location, the end lay short of the target: the bomber turned over the target and bombed
	// its way back toward its spawn edge.)
	return GlobalAbilityHelpers::BuildAircraftRunEndLocationFromDirection(
		TargetLocation,
		StartLocation,
		TargetLocation,
		M_AircraftBombingSettings.CarpetBombingLength);
}

void UGA_AircraftBombing::QueueCarpetBombingOrderForNextFrame(
	AAircraftMaster* SpawnedAircraft,
	const FVector& StartCarpetLocation,
	const FVector& CarpetEndLocation,
	const FVector& RetreatLocation)
{
	UWorld* World = GetWorld();
	if (not IsValid(World) || not IsValid(SpawnedAircraft))
	{
		return;
	}

	TWeakObjectPtr<UGA_AircraftBombing> WeakThis(this);
	TWeakObjectPtr<AAircraftMaster> WeakSpawnedAircraft(SpawnedAircraft);
	World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateLambda(
			[WeakThis, WeakSpawnedAircraft, StartCarpetLocation, CarpetEndLocation, RetreatLocation]()
			{
				if (not WeakThis.IsValid() || not WeakSpawnedAircraft.IsValid())
				{
					return;
				}

				UGA_AircraftBombing* StrongThis = WeakThis.Get();
				AAircraftMaster* StrongSpawnedAircraft = WeakSpawnedAircraft.Get();
				StrongThis->IssueCarpetBombingOrder(
					StrongSpawnedAircraft,
					StartCarpetLocation,
					CarpetEndLocation,
					RetreatLocation);
			}));
}

void UGA_AircraftBombing::IssueCarpetBombingOrder(
	AAircraftMaster* SpawnedAircraft,
	const FVector& StartCarpetLocation,
	const FVector& CarpetEndLocation,
	const FVector& RetreatLocation)
{
	if (not IsValid(SpawnedAircraft))
	{
		BroadcastBombingFinishedIfNeeded();
		return;
	}

	SpawnedAircraft->CarpetBombing(StartCarpetLocation, CarpetEndLocation, RetreatLocation);
	StartForcedRetreatTimer(SpawnedAircraft);
}

void UGA_AircraftBombing::StartForcedRetreatTimer(AAircraftMaster* SpawnedAircraft)
{
	UWorld* World = GetWorld();
	FBombingRun* Run = M_Runs.FindByPredicate([SpawnedAircraft](const FBombingRun& Candidate)
	{
		return Candidate.Aircraft.Get() == SpawnedAircraft;
	});
	if (not IsValid(World) || Run == nullptr)
	{
		return;
	}

	// One timer per bomber: overlapping executions each keep their own fallback retreat.
	const float SafeRetreatDelay = FMath::Max(0.0f, M_AircraftBombingSettings.TimeUntilForcedRetreat);
	TWeakObjectPtr<UGA_AircraftBombing> WeakThis(this);
	TWeakObjectPtr<AAircraftMaster> WeakAircraft(SpawnedAircraft);
	World->GetTimerManager().SetTimer(
		Run->ForcedRetreatTimerHandle,
		[WeakThis, WeakAircraft]()
		{
			if (not WeakThis.IsValid())
			{
				return;
			}

			WeakThis->OnForcedRetreatTimerFinished(WeakAircraft.Get());
		},
		SafeRetreatDelay,
		false);
}

void UGA_AircraftBombing::OnForcedRetreatTimerFinished(AAircraftMaster* SpawnedAircraft)
{
	// The bomber may be gone already (shot down, or retreated on its own); then there is nothing to force.
	if (not IsValid(SpawnedAircraft) || not GetIsValidGlobalAbilityManager())
	{
		return;
	}

	const FVector TargetLocation = SpawnedAircraft->GetActorLocation();
	const FVector RetreatLocation = GetGlobalAbilityManager()->GetAircraftBombingRetreatLocation(this, TargetLocation);
	SpawnedAircraft->CarpetBombing_RetreatAndDestroy(RetreatLocation);
}

void UGA_AircraftBombing::OnSpawnedAircraftDestroyed(AActor* DestroyedActor)
{
	if (M_SpawnedAircraft.Get() == DestroyedActor)
	{
		M_SpawnedAircraft.Reset();
	}
	UWorld* World = GetWorld();
	for (int32 RunIndex = M_Runs.Num() - 1; RunIndex >= 0; --RunIndex)
	{
		FBombingRun& Run = M_Runs[RunIndex];
		// The weak pointer still resolves during OnDestroyed; stale entries are dropped too.
		if (Run.Aircraft.Get() != DestroyedActor && Run.Aircraft.IsValid())
		{
			continue;
		}
		if (IsValid(World))
		{
			World->GetTimerManager().ClearTimer(Run.ForcedRetreatTimerHandle);
		}
		M_Runs.RemoveAt(RunIndex);
	}
	BroadcastBombingFinishedIfNeeded();
}

void UGA_AircraftBombing::BroadcastBombingFinishedIfNeeded()
{
	// Executions can overlap (enemy AI, missions, short cooldowns): the ability ends when the last bomber is gone.
	if (M_PendingAircraftClassLoads > 0)
	{
		return;
	}
	for (const FBombingRun& Run : M_Runs)
	{
		if (Run.Aircraft.IsValid())
		{
			return;
		}
	}
	M_Runs.Reset();
	BroadcastAbilityEnded();
}

void UGA_AircraftBombing::ClearForcedRetreatTimer()
{
	UWorld* World = GetWorld();
	for (FBombingRun& Run : M_Runs)
	{
		if (IsValid(World))
		{
			World->GetTimerManager().ClearTimer(Run.ForcedRetreatTimerHandle);
		}
		Run.ForcedRetreatTimerHandle.Invalidate();
	}
	if (IsValid(World))
	{
		World->GetTimerManager().ClearTimer(M_ForcedRetreatTimerHandle);
	}
	M_ForcedRetreatTimerHandle.Invalidate();
}
