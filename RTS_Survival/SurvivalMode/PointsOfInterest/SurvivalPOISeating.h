// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/SurvivalMode/PointsOfInterest/SurvivalPOITypes.h"

class AActor;
class FSurvivalGroundProjector;

/**
 * @brief How an object meets the ground, measured on its visible meshes (never on actor bounds: root sprites and
 * geometry collections make those lie). The footprint is split into four quadrants; each quadrant's lowest underside
 * point against the ground below it is that quadrant's gap (negative = sunk).
 */
struct FSurvivalPOISeatReport
{
	bool bMeasured = false;
	// True when the underside traces hit nothing and the visible bounding box bottom was used instead.
	bool bUsedBoxBottom = false;
	FVector LocalBoxMin = FVector::ZeroVector;
	FVector LocalBoxMax = FVector::ZeroVector;
	// Per quadrant (-X-Y, +X-Y, +X+Y, -X+Y): world XY of the lowest underside point, its Z and the ground Z under it.
	TArray<FVector> ContactPoints;
	TArray<float> GroundHeights;
	TArray<float> QuadrantGaps;
	// The second lowest quadrant gap: how far the object rests above the ground on its two lowest supports.
	float SupportGap = 0.f;
	// The highest quadrant gap (overhangs and gaps between legs read high on flat ground).
	float MaxGap = 0.f;
	float MaxSink = 0.f;
	// Ground slope under the footprint centre and the angle between the actor's up axis and the ground normal.
	float GroundSlopeDegrees = 0.f;
	float NormalDeviationDegrees = 0.f;
	int32 UndersideHits = 0;
};

namespace SurvivalPOISeating
{
	/** @return Whether the actor has visible static or skeletal meshes; OutLocalBox is their box in actor space. */
	bool GetVisibleLocalBox(const AActor& Actor, FBox& OutLocalBox);

	/** @brief Measures how the actor meets the ground as it stands now. */
	void MeasureSeat(const AActor& Actor, const FSurvivalGroundProjector& Projector, FSurvivalPOISeatReport& OutReport);

	/**
	 * @brief Puts a freshly spawned actor on the ground: tilts it to the ground under its footprint (AlignToGround),
	 * then lowers it until no quadrant floats, sinking none more than the limit; PivotOnGround only snaps the pivot.
	 * @param SinkCm Planned extra depth (half buried in drift).
	 * @param MaxTiltDegrees Tilt limit for AlignToGround.
	 * @param OutReport The seat as measured after the move.
	 */
	void SeatActor(AActor& Actor, const ESurvivalPOISeatMode SeatMode, const float SinkCm, const float MaxTiltDegrees,
	               const FSurvivalGroundProjector& Projector, FSurvivalPOISeatReport& OutReport);
}
