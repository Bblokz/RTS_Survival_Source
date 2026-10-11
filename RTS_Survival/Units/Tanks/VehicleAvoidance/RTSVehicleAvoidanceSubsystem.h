// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTSVehicleFootprint.h"
#include "Subsystems/WorldSubsystem.h"

#include "RTSVehicleAvoidanceSubsystem.generated.h"

class APawn;
class UTrackPathFollowingComponent;

/** A vehicle the avoidance knows about, with everything a neighbour needs without searching for components. */
USTRUCT()
struct FRTSVehicleAvoidanceAgent
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<APawn> Pawn;

	UPROPERTY()
	TWeakObjectPtr<UTrackPathFollowingComponent> PathFollowingComponent;

	FRTSVehicleFootprint Footprint;

	uint8 OwningPlayer = 0;
};

/**
 * @brief Lets driving vehicles find the vehicles around them before they touch, now that hulls block each other
 * and no longer report overlaps. Path following registers its vehicle here and asks for its neighbours.
 */
UCLASS()
class RTS_SURVIVAL_API URTSVehicleAvoidanceSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterVehicle(UTrackPathFollowingComponent* PathFollowingComponent, APawn* Pawn);
	void UnregisterVehicle(const UTrackPathFollowingComponent* PathFollowingComponent);

	/**
	 * @brief Collects the vehicles close enough to matter for steering and speed of one driving vehicle.
	 * @param Asker Path following of the vehicle that asks; it is never part of the result.
	 * @param Location Where the asking vehicle is.
	 * @param Range Centre-to-centre distance within which other vehicles are returned.
	 * @param OutNeighbours Receives copies of the nearby agents; emptied first.
	 */
	void FindNeighbours(
		const UTrackPathFollowingComponent* Asker,
		const FVector& Location,
		float Range,
		TArray<FRTSVehicleAvoidanceAgent>& OutNeighbours);

	/**
	 * @brief Finds the vehicle whose hull is closest to a vehicle that is hemmed in.
	 * @param Asker Vehicle that asks; never returned.
	 * @param MaxHullDistance How far apart the hulls may be, measured between their corner circles.
	 * @param OutNearest Receives the closest vehicle.
	 * @return True when a vehicle was that close.
	 */
	bool FindNearestVehicle(const APawn* Asker, float MaxHullDistance, FRTSVehicleAvoidanceAgent& OutNearest);

	/** @return The footprint of the vehicle that registered with this path following, invalid when unknown. */
	FRTSVehicleFootprint GetFootprint(const UTrackPathFollowingComponent* PathFollowingComponent);

	/**
	 * @brief Tells whether a vehicle could be put somewhere without landing inside another hull.
	 * @param Location Spot to test.
	 * @param Radius Space the vehicle needs around that spot.
	 * @param IgnoredPawn Vehicle that is being moved.
	 * @return True when no other registered vehicle reaches into that space.
	 */
	bool GetIsLocationFreeOfVehicles(const FVector& Location, float Radius, const APawn* IgnoredPawn) const;

	/**
	 * @brief Tells whether a vehicle can be teleported somewhere without touching another hull, however both are turned.
	 * @param Location Spot the vehicle would be put on.
	 * @param Vehicle Vehicle that is being moved; its own size decides how much room is needed.
	 * @return True when the circles around the hull corners of the vehicle and of every other vehicle stay apart.
	 */
	bool GetIsLocationFreeForVehicle(const FVector& Location, const APawn* Vehicle) const;

	/**
	 * @brief Looks around a spot that is taken for the nearest ground a vehicle can be put on without touching a hull.
	 * @param Origin Spot the vehicle was meant to appear on.
	 * @param Radius Space the vehicle needs around it.
	 * @param IgnoredPawn Vehicle that is being placed.
	 * @param OutLocation Receives the free spot, at the same height above the ground as Origin.
	 * @return True when a free spot on the navmesh was found close by.
	 */
	bool FindFreeLocationNear(
		const FVector& Origin,
		float Radius,
		const APawn* IgnoredPawn,
		FVector& OutLocation) const;

private:
	UPROPERTY()
	TArray<FRTSVehicleAvoidanceAgent> M_Agents;

	// Destroyed vehicles are swept out while searching, at most this often.
	int32 M_SearchesUntilCleanup = 0;

	void RemoveDestroyedAgents();
	bool TryCompleteAgent(FRTSVehicleAvoidanceAgent& Agent) const;
};
