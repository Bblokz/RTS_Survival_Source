// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTSVehicleAvoidanceSubsystem.h"

#include "GameFramework/Pawn.h"
#include "NavigationSystem.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/PathFollowingComponent/TrackPathFollowingComponent.h"

namespace RTSVehicleAvoidanceSubsystemPrivate
{
	constexpr int32 SearchesBetweenCleanups = 256;
	constexpr float UnmeasuredVehicleRadius = 420.f;

	// Rings searched around a taken spot, how far apart they lie in vehicle radii and how many spots the first holds.
	// Far enough out to get past a whole jam around a spawn point; appearing inside a hull is never acceptable.
	constexpr int32 FreeLocationRingCount = 30;
	constexpr float FreeLocationRingSpacingRadii = 1.f;
	constexpr int32 FreeLocationSpotsOnFirstRing = 8;
	constexpr int32 FreeLocationMostSpotsOnRing = 48;
	const FVector FreeLocationNavExtent(60.f, 60.f, 400.f);
}

void URTSVehicleAvoidanceSubsystem::RegisterVehicle(
	UTrackPathFollowingComponent* PathFollowingComponent,
	APawn* Pawn)
{
	if (not IsValid(PathFollowingComponent) || not IsValid(Pawn))
	{
		return;
	}

	for (FRTSVehicleAvoidanceAgent& Agent : M_Agents)
	{
		if (Agent.PathFollowingComponent.Get() != PathFollowingComponent)
		{
			continue;
		}
		// The controller took another vehicle; measure that one again.
		Agent.Pawn = Pawn;
		Agent.Footprint = FRTSVehicleFootprint();
		return;
	}

	FRTSVehicleAvoidanceAgent NewAgent;
	NewAgent.Pawn = Pawn;
	NewAgent.PathFollowingComponent = PathFollowingComponent;
	M_Agents.Add(MoveTemp(NewAgent));
}

void URTSVehicleAvoidanceSubsystem::UnregisterVehicle(const UTrackPathFollowingComponent* PathFollowingComponent)
{
	for (int32 AgentIndex = M_Agents.Num() - 1; AgentIndex >= 0; --AgentIndex)
	{
		if (M_Agents[AgentIndex].PathFollowingComponent.Get() == PathFollowingComponent)
		{
			M_Agents.RemoveAtSwap(AgentIndex, 1, EAllowShrinking::No);
		}
	}
}

bool URTSVehicleAvoidanceSubsystem::TryCompleteAgent(FRTSVehicleAvoidanceAgent& Agent) const
{
	if (Agent.Footprint.GetIsValid())
	{
		return true;
	}

	// The chassis is handed to the tank by its blueprint after possession, so it is measured on first use.
	const ATankMaster* Tank = Cast<ATankMaster>(Agent.Pawn.Get());
	if (not IsValid(Tank))
	{
		return false;
	}
	Agent.Footprint = RTSVehicleFootprint::BuildFromChassisMesh(Tank->GetTankMesh());
	if (const URTSComponent* RTSComponent = Tank->GetRTSComponent())
	{
		Agent.OwningPlayer = RTSComponent->GetOwningPlayer();
	}
	return Agent.Footprint.GetIsValid();
}

void URTSVehicleAvoidanceSubsystem::RemoveDestroyedAgents()
{
	for (int32 AgentIndex = M_Agents.Num() - 1; AgentIndex >= 0; --AgentIndex)
	{
		const FRTSVehicleAvoidanceAgent& Agent = M_Agents[AgentIndex];
		if (not Agent.Pawn.IsValid() || not Agent.PathFollowingComponent.IsValid())
		{
			M_Agents.RemoveAtSwap(AgentIndex, 1, EAllowShrinking::No);
		}
	}
}

void URTSVehicleAvoidanceSubsystem::FindNeighbours(
	const UTrackPathFollowingComponent* Asker,
	const FVector& Location,
	const float Range,
	TArray<FRTSVehicleAvoidanceAgent>& OutNeighbours)
{
	OutNeighbours.Reset();
	if (--M_SearchesUntilCleanup <= 0)
	{
		RemoveDestroyedAgents();
		M_SearchesUntilCleanup = RTSVehicleAvoidanceSubsystemPrivate::SearchesBetweenCleanups;
	}

	const float RangeSquared = FMath::Square(Range);
	for (FRTSVehicleAvoidanceAgent& Agent : M_Agents)
	{
		const APawn* Pawn = Agent.Pawn.Get();
		if (not IsValid(Pawn) || Agent.PathFollowingComponent.Get() == Asker)
		{
			continue;
		}
		if (FVector::DistSquared2D(Pawn->GetActorLocation(), Location) > RangeSquared)
		{
			continue;
		}
		// A vehicle that waits disabled for its spawn has no hull to run into.
		if (not Pawn->GetActorEnableCollision() || not TryCompleteAgent(Agent))
		{
			continue;
		}
		OutNeighbours.Add(Agent);
	}
}

bool URTSVehicleAvoidanceSubsystem::FindNearestVehicle(
	const APawn* Asker,
	const float MaxHullDistance,
	FRTSVehicleAvoidanceAgent& OutNearest)
{
	if (not IsValid(Asker))
	{
		return false;
	}

	float AskerCornerRadius = 0.f;
	for (FRTSVehicleAvoidanceAgent& Agent : M_Agents)
	{
		if (Agent.Pawn.Get() == Asker && TryCompleteAgent(Agent))
		{
			AskerCornerRadius = Agent.Footprint.GetCornerRadius();
		}
	}

	const FVector AskerLocation = Asker->GetActorLocation();
	float NearestHullDistance = MaxHullDistance;
	bool bFoundVehicle = false;
	for (FRTSVehicleAvoidanceAgent& Agent : M_Agents)
	{
		const APawn* Pawn = Agent.Pawn.Get();
		if (not IsValid(Pawn) || Pawn == Asker || not Pawn->GetActorEnableCollision() || not TryCompleteAgent(Agent))
		{
			continue;
		}
		const float HullDistance = FVector::Dist2D(Pawn->GetActorLocation(), AskerLocation) -
			AskerCornerRadius - Agent.Footprint.GetCornerRadius();
		if (HullDistance >= NearestHullDistance)
		{
			continue;
		}
		NearestHullDistance = HullDistance;
		OutNearest = Agent;
		bFoundVehicle = true;
	}
	return bFoundVehicle;
}

bool URTSVehicleAvoidanceSubsystem::GetIsLocationFreeForVehicle(const FVector& Location, const APawn* Vehicle) const
{
	float VehicleRadius = RTSVehicleAvoidanceSubsystemPrivate::UnmeasuredVehicleRadius;
	for (const FRTSVehicleAvoidanceAgent& Agent : M_Agents)
	{
		if (Agent.Pawn.Get() == Vehicle && Agent.Footprint.GetIsValid())
		{
			VehicleRadius = Agent.Footprint.GetCornerRadius();
		}
	}
	return GetIsLocationFreeOfVehicles(Location, VehicleRadius, Vehicle);
}

bool URTSVehicleAvoidanceSubsystem::FindFreeLocationNear(
	const FVector& Origin,
	const float Radius,
	const APawn* IgnoredPawn,
	FVector& OutLocation) const
{
	using namespace RTSVehicleAvoidanceSubsystemPrivate;
	const UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (NavigationSystem == nullptr)
	{
		return false;
	}
	// Vehicles are placed a little above the ground they stand on; the free spot keeps that height.
	FNavLocation OriginOnNavmesh;
	const float HeightAboveGround =
		NavigationSystem->ProjectPointToNavigation(Origin, OriginOnNavmesh, FreeLocationNavExtent)
			? Origin.Z - OriginOnNavmesh.Location.Z
			: 0.f;

	for (int32 RingIndex = 1; RingIndex <= FreeLocationRingCount; ++RingIndex)
	{
		const float RingDistance = RingIndex * Radius * FreeLocationRingSpacingRadii;
		const int32 SpotCount = FMath::Min(FreeLocationSpotsOnFirstRing * RingIndex, FreeLocationMostSpotsOnRing);
		for (int32 SpotIndex = 0; SpotIndex < SpotCount; ++SpotIndex)
		{
			const float AngleDegrees = 360.f * SpotIndex / SpotCount;
			const FVector Candidate = Origin + FRotator(0.f, AngleDegrees, 0.f).Vector() * RingDistance;
			FNavLocation CandidateOnNavmesh;
			if (not GetIsLocationFreeOfVehicles(Candidate, Radius, IgnoredPawn) ||
				not NavigationSystem->ProjectPointToNavigation(Candidate, CandidateOnNavmesh, FreeLocationNavExtent))
			{
				continue;
			}
			OutLocation = FVector(Candidate.X, Candidate.Y, CandidateOnNavmesh.Location.Z + HeightAboveGround);
			return true;
		}
	}
	return false;
}

FRTSVehicleFootprint URTSVehicleAvoidanceSubsystem::GetFootprint(
	const UTrackPathFollowingComponent* PathFollowingComponent)
{
	for (FRTSVehicleAvoidanceAgent& Agent : M_Agents)
	{
		if (Agent.PathFollowingComponent.Get() != PathFollowingComponent)
		{
			continue;
		}
		TryCompleteAgent(Agent);
		return Agent.Footprint;
	}
	return FRTSVehicleFootprint();
}

bool URTSVehicleAvoidanceSubsystem::GetIsLocationFreeOfVehicles(
	const FVector& Location,
	const float Radius,
	const APawn* IgnoredPawn) const
{
	for (const FRTSVehicleAvoidanceAgent& Agent : M_Agents)
	{
		const APawn* Pawn = Agent.Pawn.Get();
		if (not IsValid(Pawn) || Pawn == IgnoredPawn || not Pawn->GetActorEnableCollision())
		{
			continue;
		}
		// A vehicle nobody drove near yet has not been measured; it is taken to be a large one.
		const float OtherRadius = Agent.Footprint.GetIsValid()
			                          ? Agent.Footprint.GetCornerRadius()
			                          : RTSVehicleAvoidanceSubsystemPrivate::UnmeasuredVehicleRadius;
		const float RequiredDistance = Radius + OtherRadius;
		if (FVector::DistSquared2D(Pawn->GetActorLocation(), Location) < FMath::Square(RequiredDistance))
		{
			return false;
		}
	}
	return true;
}
