// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

/** An axis-aligned rectangle in the tech wheel's canvas units. */
struct RTS_SURVIVAL_API FTechWheelLabelRect
{
	FVector2D Min = FVector2D::ZeroVector;
	FVector2D Max = FVector2D::ZeroVector;

	static FTechWheelLabelRect MakeCentred(const FVector2D& Centre, const FVector2D& HalfExtent)
	{
		FTechWheelLabelRect Rect;
		Rect.Min = Centre - HalfExtent;
		Rect.Max = Centre + HalfExtent;
		return Rect;
	}

	FVector2D GetCentre() const { return (Min + Max) * 0.5; }
	FVector2D GetSize() const { return Max - Min; }
};

/** Something a label should not cover: a node's outline, a key badge, a pip. */
struct RTS_SURVIVAL_API FTechWheelLabelObstacle
{
	FTechWheelLabelRect Rect;
	// Cost per covered square unit; 1 for things a label must not hide.
	float Weight = 1.f;
	// The label with this index may cover the obstacle for free; INDEX_NONE when it counts for every label.
	int32 IgnoredByLabelIndex = INDEX_NONE;
};

/** One label to place: the node it names and the size of the label including its backing. */
struct RTS_SURVIVAL_API FTechWheelLabelRequest
{
	FVector2D NodeCentre = FVector2D::ZeroVector;
	// Corner radius of the node's widest hexagon outline (a corner points right, the flat edges are top and bottom).
	float OutlineRadius = 0.f;
	FVector2D LabelSize = FVector2D::ZeroVector;
};

/** The disc the labels have to stay on. */
struct RTS_SURVIVAL_API FTechWheelLabelRing
{
	FVector2D Centre = FVector2D::ZeroVector;
	float Radius = 0.f;
};

/**
 * @brief Where a label sits relative to its node, in order of preference: the four plain sides first, then beside
 * the slanted edges, then beside the corners of the flat edges.
 */
enum class ETechWheelLabelSide : uint8
{
	Below,
	Above,
	Right,
	Left,
	LowerRight,
	LowerLeft,
	UpperRight,
	UpperLeft,
	BelowRight,
	BelowLeft,
	AboveRight,
	AboveLeft,
	Count
};

struct RTS_SURVIVAL_API FTechWheelLabelPlacementResult
{
	ETechWheelLabelSide Side = ETechWheelLabelSide::Below;
	FTechWheelLabelRect Rect;
};

/**
 * @brief The maths that keeps the tech wheel's technology names off the node icons, off each other and on the wheel,
 * as pure functions so it is unit tested and the widget only places what these return.
 */
namespace TechWheelLabelPlacement
{
	/** @return The area both rectangles cover; 0 when they do not overlap. */
	RTS_SURVIVAL_API float GetOverlapArea(const FTechWheelLabelRect& First, const FTechWheelLabelRect& Second);

	/**
	 * @brief How much of a rectangle lies off the disc, estimated from a fixed grid of sample points.
	 * @param Rect Rectangle to check.
	 * @param Ring The disc.
	 * @return Area outside the disc; 0 when every sample point is on it.
	 */
	RTS_SURVIVAL_API float GetAreaOutsideRing(const FTechWheelLabelRect& Rect, const FTechWheelLabelRing& Ring);

	/**
	 * @brief Half the width of a hexagon with a corner pointing right, at a height above or below its centre.
	 * @param Radius Corner radius of the hexagon.
	 * @param OffsetY Distance from the centre line; the sign does not matter.
	 * @return 0 above the top edge and below the bottom edge.
	 */
	RTS_SURVIVAL_API float GetHexagonHalfWidthAt(float Radius, float OffsetY);

	/**
	 * @brief Adds a hexagon as a stack of rectangles, so a label next to a slanted edge is not charged for the empty
	 * corners of the hexagon's bounding box.
	 * @param Centre Centre of the hexagon.
	 * @param Radius Corner radius of the hexagon.
	 * @param OutObstacles Receives the rectangles; not emptied.
	 * @param IgnoredByLabelIndex Index of the node's own label, whose sides are clear of the true hexagon but
	 * not of every rectangle of the stack; INDEX_NONE for a hexagon without a label.
	 */
	RTS_SURVIVAL_API void AppendHexagonObstacles(const FVector2D& Centre, float Radius,
	                                             TArray<FTechWheelLabelObstacle>& OutObstacles,
	                                             int32 IgnoredByLabelIndex = INDEX_NONE);

	/**
	 * @brief The rectangle a label takes on one side of its node.
	 * @param Request The label and its node.
	 * @param Side Side to compute.
	 * @return Rectangle of the label's size, clear of the node's own outline.
	 */
	RTS_SURVIVAL_API FTechWheelLabelRect GetCandidateRect(const FTechWheelLabelRequest& Request,
	                                                     ETechWheelLabelSide Side);

	/**
	 * @brief Chooses a side for every label: the one that covers the least of the obstacles, of the labels already
	 * chosen and of the area off the wheel. A label stays below its node when that is free, and never moves next
	 * to another node in a way that reads as that node's name unless nothing better is left.
	 * @param Requests The labels to place.
	 * @param Obstacles Everything the labels should not cover.
	 * @param Ring The disc the labels have to stay on.
	 * @param OutPlacements Receives one placement per request, in the same order; emptied first.
	 */
	RTS_SURVIVAL_API void PlaceLabels(const TArray<FTechWheelLabelRequest>& Requests,
	                                  const TArray<FTechWheelLabelObstacle>& Obstacles,
	                                  const FTechWheelLabelRing& Ring,
	                                  TArray<FTechWheelLabelPlacementResult>& OutPlacements);
}
