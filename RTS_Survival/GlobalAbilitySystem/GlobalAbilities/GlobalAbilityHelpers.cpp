// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GlobalAbilityHelpers.h"

#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"

AAircraftMaster* GlobalAbilityHelpers::SpawnAircraftAtLocation(
	const UObject* WorldContextObject,
	UClass* AircraftClass,
	const FVector& SpawnLocation)
{
	if (not IsValid(WorldContextObject) || not IsValid(AircraftClass))
	{
		return nullptr;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (not IsValid(World))
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AAircraftMaster* SpawnedAircraft = World->SpawnActor<AAircraftMaster>(
		AircraftClass,
		SpawnLocation,
		FRotator::ZeroRotator,
		SpawnParameters);
	if (not IsValid(SpawnedAircraft))
	{
		return nullptr;
	}

	SpawnedAircraft->SpawnDefaultController();
	return SpawnedAircraft;
}

FVector GlobalAbilityHelpers::BuildAircraftRunEndLocation(
	const FVector& StartLocation,
	const FVector& TargetLocation,
	const float RunLength)
{
	return BuildAircraftRunEndLocationFromDirection(StartLocation, StartLocation, TargetLocation, RunLength);
}

FVector GlobalAbilityHelpers::BuildAircraftRunEndLocationFromDirection(
	const FVector& RunStartLocation,
	const FVector& DirectionStartLocation,
	const FVector& DirectionTargetLocation,
	const float RunLength)
{
	FVector Direction = DirectionTargetLocation - DirectionStartLocation;
	Direction.Z = 0.0f;
	if (not Direction.Normalize())
	{
		return RunStartLocation;
	}

	FVector EndLocation = RunStartLocation + Direction * RunLength;
	EndLocation.Z = RunStartLocation.Z;
	return EndLocation;
}
