// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GA_AircraftStrafing.h"

#include "TimerManager.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/FStrafeAircraftSettings/FStrafeAircraftSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UGA_AircraftStrafing::ExecuteAbilityAtLocation(const FVector& TargetLocation)
{
	ResetAbilityEndedBroadcast();

	if (not GetIsValidGlobalAbilityManager())
	{
		return;
	}

	M_AircraftClassLoadHandles.Reset();
	M_PendingAircraftLoadCount = 0;
	M_ActiveAircraftCount = 0;
	RequestSpawnAircraftAtStartLocationsAsync(TargetLocation);
	Super::ExecuteAbilityAtLocation(TargetLocation);
}

void UGA_AircraftStrafing::BeginDestroy()
{
	M_AircraftClassLoadHandles.Reset();
	Super::BeginDestroy();
}

void UGA_AircraftStrafing::RequestSpawnAircraftAtStartLocationsAsync(const FVector& TargetLocation)
{
	if (M_AircraftStrafingSettings.AircraftClasses.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(TEXT("UGA_AircraftStrafing has no aircraft classes configured."));
		BroadcastAbilityEnded();
		return;
	}

	int32 ValidAircraftClassCount = 0;
	for (const TSoftClassPtr<AAircraftMaster>& AircraftClassToLoad : M_AircraftStrafingSettings.AircraftClasses)
	{
		if (AircraftClassToLoad.IsNull())
		{
			continue;
		}

		++ValidAircraftClassCount;
		++M_PendingAircraftLoadCount;
		UGlobalAbilitiesManager* GlobalAbilitiesManager = GetGlobalAbilityManager();
		const FVector StartLocation = GlobalAbilitiesManager->GetAircraftBombingSpawnLocation(this, TargetLocation);
		const FVector PostStrafeMoveToLocation = BuildPostStrafeMoveToLocation(TargetLocation);
		RequestSpawnAircraftAtStartLocationAsync(
			AircraftClassToLoad,
			StartLocation,
			TargetLocation,
			PostStrafeMoveToLocation);
	}

	if (ValidAircraftClassCount > 0)
	{
		return;
	}

	RTSFunctionLibrary::ReportError(TEXT("UGA_AircraftStrafing has no valid aircraft classes configured."));
	BroadcastAbilityEnded();
}

void UGA_AircraftStrafing::RequestSpawnAircraftAtStartLocationAsync(
	const TSoftClassPtr<AAircraftMaster>& AircraftClassToLoad,
	const FVector& StartLocation,
	const FVector& TargetLocation,
	const FVector& PostStrafeMoveToLocation)
{
	TWeakObjectPtr<UGA_AircraftStrafing> WeakThis(this);
	TSharedPtr<FStreamableHandle> AircraftClassLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		AircraftClassToLoad.ToSoftObjectPath(),
		FStreamableDelegate::CreateLambda(
			[WeakThis, AircraftClassToLoad, StartLocation, TargetLocation, PostStrafeMoveToLocation]()
			{
				if (not WeakThis.IsValid())
				{
					return;
				}

				UGA_AircraftStrafing* StrongThis = WeakThis.Get();
				StrongThis->OnAircraftClassLoaded(
					AircraftClassToLoad,
					StartLocation,
					TargetLocation,
					PostStrafeMoveToLocation);
			})
	);
	M_AircraftClassLoadHandles.Add(AircraftClassLoadHandle);
}

void UGA_AircraftStrafing::OnAircraftClassLoaded(
	const TSoftClassPtr<AAircraftMaster>& AircraftClassToLoad,
	const FVector& StartLocation,
	const FVector& TargetLocation,
	const FVector& PostStrafeMoveToLocation)
{
	--M_PendingAircraftLoadCount;

	UClass* AircraftClass = AircraftClassToLoad.Get();
	if (not IsValid(AircraftClass))
	{
		RTSFunctionLibrary::ReportError(TEXT("UGA_AircraftStrafing failed to async load a valid aircraft class."));
		BroadcastStrafingFinishedIfNeeded();
		return;
	}

	AAircraftMaster* SpawnedAircraft = GlobalAbilityHelpers::SpawnAircraftAtLocation(this, AircraftClass, StartLocation);
	if (not IsValid(SpawnedAircraft))
	{
		BroadcastStrafingFinishedIfNeeded();
		return;
	}

	++M_ActiveAircraftCount;
	SpawnedAircraft->OnDestroyed.AddUniqueDynamic(this, &UGA_AircraftStrafing::OnSpawnedAircraftDestroyed);
	const FVector StrafeEndLocation = BuildStrafeEndLocation(StartLocation, TargetLocation);
	SpawnedAircraft->SetUnitSelectable(false);
	QueueStrafeOrderForNextFrame(SpawnedAircraft, TargetLocation, StrafeEndLocation, PostStrafeMoveToLocation);
}

FVector UGA_AircraftStrafing::BuildStrafeEndLocation(const FVector& StartLocation, const FVector& TargetLocation) const
{
	return GlobalAbilityHelpers::BuildAircraftRunEndLocationFromDirection(
		TargetLocation,
		StartLocation,
		TargetLocation,
		M_AircraftStrafingSettings.StrafeLength);
}

FVector UGA_AircraftStrafing::BuildPostStrafeMoveToLocation(const FVector& TargetLocation) const
{
	if (not GetIsValidGlobalAbilityManager())
	{
		return TargetLocation;
	}

	return GetGlobalAbilityManager()->GetAircraftBombingRetreatLocation(this, TargetLocation);
}

void UGA_AircraftStrafing::QueueStrafeOrderForNextFrame(
	AAircraftMaster* SpawnedAircraft,
	const FVector& StartLocation,
	const FVector& StrafeEndLocation,
	const FVector& PostStrafeMoveToLocation)
{
	UWorld* World = GetWorld();
	if (not IsValid(World) || not IsValid(SpawnedAircraft))
	{
		return;
	}

	TWeakObjectPtr<UGA_AircraftStrafing> WeakThis(this);
	TWeakObjectPtr<AAircraftMaster> WeakSpawnedAircraft(SpawnedAircraft);
	World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateLambda(
			[WeakThis, WeakSpawnedAircraft, StartLocation, StrafeEndLocation, PostStrafeMoveToLocation]()
			{
				if (not WeakThis.IsValid() || not WeakSpawnedAircraft.IsValid())
				{
					return;
				}

				UGA_AircraftStrafing* StrongThis = WeakThis.Get();
				AAircraftMaster* StrongSpawnedAircraft = WeakSpawnedAircraft.Get();
				StrongThis->IssueStrafeOrder(
					StrongSpawnedAircraft,
					StartLocation,
					StrafeEndLocation,
					PostStrafeMoveToLocation);
			}));
}

void UGA_AircraftStrafing::IssueStrafeOrder(
	AAircraftMaster* SpawnedAircraft,
	const FVector& StartLocation,
	const FVector& StrafeEndLocation,
	const FVector& PostStrafeMoveToLocation) const
{
	if (not IsValid(SpawnedAircraft))
	{
		return;
	}

	const FStrafeAircraftSettings StrafeAircraftSettings(
		StartLocation,
		StrafeEndLocation,
		M_AircraftStrafingSettings.StrafePointTotalLerpTime,
		M_AircraftStrafingSettings.TotalStrafeTime,
		PostStrafeMoveToLocation);
	SpawnedAircraft->StrafeLocation(StrafeAircraftSettings, M_AircraftStrafingSettings.AttackMoveSettings);
}

void UGA_AircraftStrafing::OnSpawnedAircraftDestroyed(AActor* DestroyedActor)
{
	M_ActiveAircraftCount = FMath::Max(0, M_ActiveAircraftCount - 1);
	BroadcastStrafingFinishedIfNeeded();
}

void UGA_AircraftStrafing::BroadcastStrafingFinishedIfNeeded()
{
	if (M_PendingAircraftLoadCount > 0)
	{
		return;
	}

	if (M_ActiveAircraftCount > 0)
	{
		return;
	}

	BroadcastAbilityEnded();
}
