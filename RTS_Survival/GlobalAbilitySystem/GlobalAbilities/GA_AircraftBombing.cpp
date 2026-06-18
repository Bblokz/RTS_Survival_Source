// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GA_AircraftBombing.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
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

	AAircraftMaster* SpawnedAircraft = SpawnAircraftAtStartLocation(AircraftClass, StartLocation);
	if (not IsValid(SpawnedAircraft))
	{
		return;
	}

	M_SpawnedAircraft = SpawnedAircraft;
	const FVector CarpetEndLocation = BuildCarpetEndLocation(StartLocation, TargetLocation);
	SpawnedAircraft->CarpetBombing(StartLocation, CarpetEndLocation, RetreatLocation);
	StartForcedRetreatTimer();
}

AAircraftMaster* UGA_AircraftBombing::SpawnAircraftAtStartLocation(UClass* AircraftClass, const FVector& StartLocation) const
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<AAircraftMaster>(AircraftClass, StartLocation, FRotator::ZeroRotator, SpawnParameters);
}

FVector UGA_AircraftBombing::BuildCarpetEndLocation(const FVector& StartLocation, const FVector& TargetLocation) const
{
	FVector Direction = TargetLocation - StartLocation;
	Direction.Z = 0.0f;
	if (not Direction.Normalize())
	{
		return StartLocation;
	}

	FVector CarpetEndLocation = StartLocation + Direction * M_AircraftBombingSettings.CarpetBombingLength;
	CarpetEndLocation.Z = StartLocation.Z;
	return CarpetEndLocation;
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
	if (not GetIsValidSpawnedAircraftForForcedRetreat() || not GetIsValidGlobalAbilityManager())
	{
		return;
	}

	const FVector TargetLocation = M_SpawnedAircraft->GetActorLocation();
	const FVector RetreatLocation = GetGlobalAbilityManager()->GetAircraftBombingRetreatLocation(this, TargetLocation);
	M_SpawnedAircraft->CarpetBombing_RetreatAndDestroy(RetreatLocation);
}

bool UGA_AircraftBombing::GetIsValidSpawnedAircraftForForcedRetreat() const
{
	if (M_SpawnedAircraft.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_SpawnedAircraft",
		"GetIsValidSpawnedAircraftForForcedRetreat",
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
