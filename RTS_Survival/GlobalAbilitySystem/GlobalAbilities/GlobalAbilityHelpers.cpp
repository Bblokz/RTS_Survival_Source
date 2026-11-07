// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GlobalAbilityHelpers.h"

#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/Aircraft/VTOLDropship/VTOLDropshipAircraft.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

bool GlobalAbilityHelpers::TryFindGroundLocation(
	const UObject* WorldContextObject,
	const FVector& Location,
	FVector& OutGroundLocation)
{
	constexpr float TraceHalfHeight = 200000.0f;
	OutGroundLocation = Location;
	const UWorld* World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	if (not IsValid(World))
	{
		return false;
	}

	const FVector TraceStart(Location.X, Location.Y, Location.Z + TraceHalfHeight);
	const FVector TraceEnd(Location.X, Location.Y, Location.Z - TraceHalfHeight);
	FHitResult LandscapeHit;
	if (not World->LineTraceSingleByChannel(LandscapeHit, TraceStart, TraceEnd, COLLISION_TRACE_LANDSCAPE))
	{
		return false;
	}
	OutGroundLocation = LandscapeHit.ImpactPoint;
	return true;
}

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
	// The VTOL resource shuttles (BP_DropShip_*) skip the aircraft movement tick on purpose: they never fly a drop,
	// strike or move order, so an ability configured with one hovers at its spawn point and never ends.
	if (SpawnedAircraft->IsA<AVTOLDropshipAircraft>())
	{
		RTSFunctionLibrary::ReportError(TEXT("A global ability is configured with a VTOL resource shuttle, which cannot "
			"fly ability orders; use a regular aircraft class. Class: ") + AircraftClass->GetName());
	}
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
