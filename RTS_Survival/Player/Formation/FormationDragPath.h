#pragma once

#include "CoreMinimal.h"

/**
 * @brief History of the ground locations the cursor passed while the secondary button was held.
 * The formation controller spreads the selected units along it and the move preview draws it.
 * Start it on the button press and feed it the cursor location every tick until release.
 */
struct RTS_SURVIVAL_API FFormationDragPath
{
	void Start(const FVector& StartLocation);
	void Reset();

	/**
	 * @brief Extends the path once the cursor moved far enough from the last recorded point.
	 * @param Location Ground location under the cursor.
	 * @return True when the path grew, so anything built from it is out of date.
	 */
	bool TryAddPoint(const FVector& Location);

	bool GetIsStarted() const { return not M_Points.IsEmpty(); }
	float GetLength() const { return M_CumulativeLengths.IsEmpty() ? 0.0f : M_CumulativeLengths.Last(); }
	const TArray<FVector>& GetPoints() const { return M_Points; }
	FVector GetStartLocation() const { return M_Points.IsEmpty() ? FVector::ZeroVector : M_Points[0]; }

	/** @return Location on the path this far from its start; clamped to the path's ends. */
	FVector GetLocationAtDistance(float Distance) const;

	/**
	 * @brief Units on the line look to the left of the drag, so dragging the other way turns them around.
	 * @param Distance Distance along the path at which a unit stands.
	 * @return Flat unit vector a unit at that point of the path faces.
	 */
	FVector GetFacingAtDistance(float Distance) const;

	/** @return Facing of the path as a whole, from its first to its last point. */
	FVector GetOverallFacing() const;

private:
	TArray<FVector> M_Points;

	// Parallel to M_Points: flat distance from the first point up to each point.
	TArray<float> M_CumulativeLengths;

	// Doubles every time the path is thinned out, so a very long drag keeps a bounded number of points.
	float M_MinimumPointSpacing = 0.0f;

	void RemoveEveryOtherPoint();
	void RebuildCumulativeLengths();
};
