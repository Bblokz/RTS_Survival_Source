// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GA_AircraftBombing.h"

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
	AAircraftMaster* SpawnedAircraft = SpawnAircraftAtStartLocation(StartLocation);
	if (not IsValid(SpawnedAircraft))
	{
		return;
	}

	M_SpawnedAircraft = SpawnedAircraft;
	const FVector CarpetEndLocation = BuildCarpetEndLocation(StartLocation, TargetLocation);
	SpawnedAircraft->CarpetBombing(StartLocation, CarpetEndLocation, RetreatLocation);
	StartForcedRetreatTimer();
}

void UGA_AircraftBombing::BeginDestroy()
{
	ClearForcedRetreatTimer();
	Super::BeginDestroy();
}

AAircraftMaster* UGA_AircraftBombing::SpawnAircraftAtStartLocation(const FVector& StartLocation) const
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return nullptr;
	}

	UClass* AircraftClass = M_AircraftBombingSettings.AircraftClass.LoadSynchronous();
	if (not IsValid(AircraftClass))
	{
		RTSFunctionLibrary::ReportError(TEXT("UGA_AircraftBombing has no valid aircraft class configured."));
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

			WeakThis->OnForcedRetreatTimerFinished();
		},
		SafeRetreatDelay,
		false);
}

void UGA_AircraftBombing::OnForcedRetreatTimerFinished()
{
	M_ForcedRetreatTimerHandle.Invalidate();
	if (not HasSpawnedAircraftForForcedRetreat() || not GetIsValidGlobalAbilityManager())
	{
		return;
	}

	const FVector TargetLocation = M_SpawnedAircraft->GetActorLocation();
	const FVector RetreatLocation = GetGlobalAbilityManager()->GetAircraftBombingRetreatLocation(this, TargetLocation);
	M_SpawnedAircraft->CarpetBombing_RetreatAndDestroy(RetreatLocation);
}

bool UGA_AircraftBombing::HasSpawnedAircraftForForcedRetreat() const
{
	return M_SpawnedAircraft.IsValid();
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
