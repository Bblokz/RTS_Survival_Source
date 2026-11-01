// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyTypes.h"

#include "ConvoyRoute.generated.h"

class UWorld;

/**
 * @brief Arc-length view of a route: the column is placed and measured by distance along it.
 * Built once per convoy from FConvoyRoute::Points; all queries are 2D (Z is taken from the points).
 */
struct RTS_SURVIVAL_API FConvoyRoutePolyline
{
	void Build(const TArray<FVector>& InPoints);

	bool GetIsValid() const { return M_Points.Num() >= 2; }
	float GetLength() const { return M_CumulativeDistances.IsEmpty() ? 0.f : M_CumulativeDistances.Last(); }
	const TArray<FVector>& GetPoints() const { return M_Points; }

	/** @return The point at Distance along the route (clamped to the route). */
	FVector GetPointAtDistance(float Distance) const;

	/** @return The unit 2D heading of the route at Distance. */
	FVector GetDirectionAtDistance(float Distance) const;

	/**
	 * @brief Projects a location onto the route, searching only the stretch around a hint so a route that doubles
	 * back near itself never makes a unit jump ahead.
	 * @param Location World location to project.
	 * @param HintDistance Where the unit was along the route last time; negative searches the whole route.
	 * @param SearchWindow Half length of the searched stretch around the hint.
	 * @param OutLateralOffset Signed sideways distance from the route (positive = right of the heading).
	 * @return Distance along the route of the closest point.
	 */
	float ProjectLocation(const FVector& Location, float HintDistance, float SearchWindow, float& OutLateralOffset) const;

	/** @return Point at Distance moved sideways by LateralOffset (positive = right of the heading). */
	FVector GetOffsetPoint(float Distance, float LateralOffset) const;

private:
	TArray<FVector> M_Points;
	TArray<float> M_CumulativeDistances;

	int32 FindSegmentIndex(float Distance) const;
};

/** @brief Builds convoy routes that stay drivable for heavy vehicles; usable from any game mode or Blueprint. */
UCLASS()
class RTS_SURVIVAL_API UConvoyRouteLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * @brief Turns a few control points into a dense drivable point list: each leg is chained through nav path
	 * queries (single queries stop at the search-node limit on long drives) and resampled.
	 * @param WorldContextObject Any object in the world.
	 * @param ControlPoints Start, optional via points, exit.
	 * @param ResampleSpacing Distance between the output points.
	 * @param RouteId Stored in the result.
	 * @param bOutReachedEnd False when some leg could not reach its end (the route stops at the last reached point).
	 * @return The route; empty points when the start is off the navmesh.
	 */
	UFUNCTION(BlueprintCallable, Category = "Convoy", meta = (WorldContext = "WorldContextObject"))
	static FConvoyRoute BuildNavigableRoute(const UObject* WorldContextObject, const TArray<FVector>& ControlPoints,
	                                        float ResampleSpacing, FName RouteId, bool& bOutReachedEnd);

	/** @return Length of the polyline through Points (2D). */
	UFUNCTION(BlueprintPure, Category = "Convoy")
	static float GetRouteLength(const FConvoyRoute& Route);

	/**
	 * @brief Projects a point to the vehicle navmesh.
	 * @return False when nothing navigable is within the search extent.
	 */
	static bool ProjectToVehicleNavmesh(const UWorld* World, const FVector& Location, FVector& OutProjected);

	/**
	 * @brief One leg of nav path points from Start to Goal for a heavy vehicle, chained from partial ends.
	 * @return Whether Goal was reached; OutPoints holds the path (start included).
	 */
	static bool FindChainedVehiclePath(const UWorld* World, const FVector& Start, const FVector& Goal,
	                                   TArray<FVector>& OutPoints);
};
