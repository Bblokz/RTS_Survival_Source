// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

class AAircraftMaster;
class UObject;
class UWorld;

namespace GlobalAbilityHelpers
{
	AAircraftMaster* SpawnAircraftAtLocation(const UObject* WorldContextObject, UClass* AircraftClass, const FVector& SpawnLocation);
	FVector BuildAircraftRunEndLocation(const FVector& StartLocation, const FVector& TargetLocation, float RunLength);

	/**
	 * @brief Finds the landscape under a location, ignoring units and buildings; drops and previews sit on the ground.
	 * @param WorldContextObject Any object in the world to trace in.
	 * @param Location Location to look under; only X and Y are used.
	 * @param OutGroundLocation The landscape point, or Location when there is no landscape.
	 * @return True when landscape was found.
	 */
	bool TryFindGroundLocation(const UObject* WorldContextObject, const FVector& Location, FVector& OutGroundLocation);

	/**
	 * @brief Builds a run endpoint from a separate direction vector so target-centered runs can share bombing math.
	 * @param RunStartLocation Location where the ordered aircraft run begins.
	 * @param DirectionStartLocation Start point used only to determine the run direction.
	 * @param DirectionTargetLocation Target point used only to determine the run direction.
	 * @param RunLength Designer-controlled horizontal distance for the aircraft run.
	 * @return Run endpoint, or RunStartLocation when no safe horizontal direction exists.
	 */
	FVector BuildAircraftRunEndLocationFromDirection(
		const FVector& RunStartLocation,
		const FVector& DirectionStartLocation,
		const FVector& DirectionTargetLocation,
		float RunLength);
}
