// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTSUnitGroundRecovery.h"

#include "NavigationSystem.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"

namespace RTSUnitGroundRecoveryStatics
{
	// Reach of the landscape traces above (and below) a unit: covers every map's height range and the KillZ margin.
	constexpr float LandscapeTraceHeight = 400000.0f;
	// How far from a unit that went over the map border the nearest navmesh is looked for.
	const FVector NavSearchExtent(30000.0f, 30000.0f, LandscapeTraceHeight);

	bool TryTraceLandscape(const UWorld& World, const AActor& Unit, const FVector& Start, const FVector& End,
	                       FVector& OutSurface, FString& OutHitName)
	{
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RTSUnitGroundRecovery), false, &Unit);
		FHitResult LandscapeHit;
		if (not World.LineTraceSingleByChannel(LandscapeHit, Start, End, COLLISION_TRACE_LANDSCAPE, QueryParams))
		{
			return false;
		}
		OutSurface = LandscapeHit.ImpactPoint;
		OutHitName = GetNameSafe(LandscapeHit.GetActor()) + TEXT(".") + GetNameSafe(LandscapeHit.GetComponent());
		return true;
	}

	void PlaceAt(AActor& Unit, const FVector& Ground, const float OriginAboveGround)
	{
		// Teleport: no sweep through the landscape on the way up, and physics bodies (vehicles) are reset with it.
		Unit.SetActorLocation(Ground + FVector(0.0f, 0.0f, OriginAboveGround), false, nullptr,
		                      ETeleportType::ResetPhysics);
	}
}

bool FRTSUnitGroundRecovery::TryLiftOutOfLandscape(AActor& Unit, const float OriginAboveGround, const float MinDepthCm)
{
	using namespace RTSUnitGroundRecoveryStatics;
	const UWorld* World = Unit.GetWorld();
	if (not IsValid(World))
	{
		return false;
	}
	// Down to the origin only: the trace hits the landscape just when its surface is above the origin, i.e. the unit
	// starts inside the ground. A landscape hole has no collision, so units in trenches are not touched.
	const FVector Origin = Unit.GetActorLocation();
	FVector Surface;
	FString HitName;
	if (not TryTraceLandscape(*World, Unit, Origin + FVector(0.0f, 0.0f, LandscapeTraceHeight), Origin, Surface,
	                          HitName) || Surface.Z - Origin.Z < MinDepthCm)
	{
		return false;
	}
	PlaceAt(Unit, Surface, OriginAboveGround);
	UE_LOG(LogTemp, Display, TEXT("RTS_SPAWN_INSIDE_LANDSCAPE %s lifted from z%.0f onto the surface at z%.0f (%s)"),
	       *Unit.GetName(), Origin.Z, Surface.Z, *HitName);
	return true;
}

bool FRTSUnitGroundRecovery::TryPlaceBackOnGround(AActor& Unit, const float OriginAboveGround)
{
	using namespace RTSUnitGroundRecoveryStatics;
	UWorld* World = Unit.GetWorld();
	if (not IsValid(World))
	{
		return false;
	}
	const FVector Location = Unit.GetActorLocation();
	FVector Surface;
	FString HitName;
	if (TryTraceLandscape(*World, Unit, Location + FVector(0.0f, 0.0f, LandscapeTraceHeight),
	                      Location - FVector(0.0f, 0.0f, LandscapeTraceHeight), Surface, HitName))
	{
		PlaceAt(Unit, Surface, OriginAboveGround);
		UE_LOG(LogTemp, Display, TEXT("RTS_FELL_OUT_OF_WORLD %s ground found on %s at z%.0f"), *Unit.GetName(), *HitName,
		       Surface.Z);
		return true;
	}
	// No landscape at this X/Y: the unit went over the map border. Only runs for a unit that fell, so the wide search
	// costs nothing during normal play.
	const UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	FNavLocation NavLocation;
	if (NavSystem && NavSystem->ProjectPointToNavigation(Location, NavLocation, NavSearchExtent))
	{
		PlaceAt(Unit, NavLocation.Location, OriginAboveGround);
		return true;
	}
	return false;
}
