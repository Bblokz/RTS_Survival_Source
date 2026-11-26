// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

class UWorld;

/**
 * @brief Keeps mission drops (reinforcement aircraft, supply crates) off buildings: a spot is clear when no deployed
 * or deploying nomadic building, building expansion or placed building preview is within the clearance radius.
 * Used by UGA_AircraftDrop (when a mission asks for it), the Soviet counterattack's reinforcements and the player
 * resource safety net.
 */
struct RTS_SURVIVAL_API FBuildingFreeDropSpots
{
	/**
	 * @brief Overlaps the building placement channel (building and expansion meshes, placed previews) and checks the
	 * nomadics that are unfolding, whose building mesh may not collide yet.
	 * @param World World to query.
	 * @param Location Ground location of the spot.
	 * @param ClearanceRadius 2D distance the spot keeps from any building.
	 * @return True when no building is within ClearanceRadius.
	 */
	static bool GetIsSpotClearOfBuildings(const UWorld& World, const FVector& Location, float ClearanceRadius);

	/**
	 * @brief Searches rings around the anchor, nearest first, for a navmesh spot clear of buildings.
	 * @param WorldContextObject Any object in the world.
	 * @param Anchor Preferred spot; returned as projected when it is already clear.
	 * @param ClearanceRadius 2D distance the spot keeps from any building.
	 * @param ExtraAccept Extra rule for a candidate (spacing from spots already taken); return false to skip it.
	 * @param OutSpot The spot found.
	 * @return False when nothing within MaxSearchRadius qualified; OutSpot is the anchor then.
	 */
	static bool FindBuildingFreeSpot(const UObject* WorldContextObject, const FVector& Anchor, float ClearanceRadius,
	                                 TFunctionRef<bool(const FVector&)> ExtraAccept, FVector& OutSpot);

	/** @brief FindBuildingFreeSpot without an extra rule. */
	static bool FindBuildingFreeSpot(const UObject* WorldContextObject, const FVector& Anchor, float ClearanceRadius,
	                                 FVector& OutSpot);
};
