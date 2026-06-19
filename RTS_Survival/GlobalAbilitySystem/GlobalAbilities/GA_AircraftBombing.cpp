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
	if (not GetIsValidGlobalAbilityManager())
	{
		return;
	}

	UGlobalAbilitiesManager* GlobalAbilitiesManager = GetGlobalAbilityManager();
	const FVector StartLocation = GlobalAbilitiesManager->GetAircraftBombingSpawnLocation(this, TargetLocation);
	const FVector RetreatLocation = GlobalAbilitiesManager->GetAircraftBombingRetreatLocation(this, TargetLocation);
	RequestSpawnAircraftAtStartLocationAsync(StartLocation, TargetLocation, RetreatLocation);
}

void UGA_AircraftBombing::BeginDestroy()
{
	ClearForcedRetreatTimer();
	M_AircraftClassLoadHandle.Reset();
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
		return;
	}

	TWeakObjectPtr<UGA_AircraftBombing> WeakThis(this);
	M_AircraftClassLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
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
}

void UGA_AircraftBombing::OnAircraftClassLoaded(
	const TSoftClassPtr<AAircraftMaster>& AircraftClassToLoad,
	const FVector& StartLocation,
	const FVector& TargetLocation,
	const FVector& RetreatLocation)
{
	M_AircraftClassLoadHandle.Reset();
	UClass* AircraftClass = AircraftClassToLoad.Get();
	if (not IsValid(AircraftClass))
	{
		RTSFunctionLibrary::ReportError(TEXT("UGA_AircraftBombing failed to async load a valid aircraft class."));
		return;
	}

	AAircraftMaster* SpawnedAircraft = GlobalAbilityHelpers::SpawnAircraftAtLocation(this, AircraftClass, StartLocation);
	if (not IsValid(SpawnedAircraft))
	{
		return;
	}

	M_SpawnedAircraft = SpawnedAircraft;
	const FVector CarpetEndLocation = BuildCarpetEndLocation(StartLocation, TargetLocation);
	QueueCarpetBombingOrderForNextFrame(SpawnedAircraft, TargetLocation, CarpetEndLocation, RetreatLocation);
}

FVector UGA_AircraftBombing::BuildCarpetEndLocation(const FVector& StartLocation, const FVector& TargetLocation) const
{
	return GlobalAbilityHelpers::BuildAircraftRunEndLocation(
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
		return;
	}

	SpawnedAircraft->CarpetBombing(StartCarpetLocation, CarpetEndLocation, RetreatLocation);
	StartForcedRetreatTimer();
}

void UGA_AircraftBombing::StartForcedRetreatTimer()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	const float SafeRetreatDelay = FMath::Max(0.0f, M_AircraftBombingSettings.TimeUntilForcedRetreat);
	TWeakObjectPtr<UGA_AircraftBombing> WeakThis(this);
	World->GetTimerManager().SetTimer(
		M_ForcedRetreatTimerHandle,
		[WeakThis]()
		{
			if (not WeakThis.IsValid())
			{
				return;
			}

			UGA_AircraftBombing* StrongThis = WeakThis.Get();
			StrongThis->OnForcedRetreatTimerFinished();
		},
		SafeRetreatDelay,
		false);
}

void UGA_AircraftBombing::OnForcedRetreatTimerFinished()
{
	M_ForcedRetreatTimerHandle.Invalidate();
	if (not GetIsValidSpawnedAircraft() || not GetIsValidGlobalAbilityManager())
	{
		return;
	}

	const FVector TargetLocation = M_SpawnedAircraft->GetActorLocation();
	const FVector RetreatLocation = GetGlobalAbilityManager()->GetAircraftBombingRetreatLocation(this, TargetLocation);
	M_SpawnedAircraft->CarpetBombing_RetreatAndDestroy(RetreatLocation);
}

bool UGA_AircraftBombing::GetIsValidSpawnedAircraft() const
{
	if (M_SpawnedAircraft.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_SpawnedAircraft",
		"GetIsValidSpawnedAircraft",
		this
	);

	return false;
}

void UGA_AircraftBombing::ClearForcedRetreatTimer()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	World->GetTimerManager().ClearTimer(M_ForcedRetreatTimerHandle);
	M_ForcedRetreatTimerHandle.Invalidate();
}
