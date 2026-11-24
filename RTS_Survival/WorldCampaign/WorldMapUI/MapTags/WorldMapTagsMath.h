// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

/** One straight piece of a dashed line, in the local space of the widget that paints it. */
struct RTS_SURVIVAL_API FWorldMapDashSegment
{
	FVector2D Start = FVector2D::ZeroVector;
	FVector2D End = FVector2D::ZeroVector;
};

/** One tag as a box in the overlay's local units, and whether the placement pass keeps it on screen. */
struct RTS_SURVIVAL_API FWorldMapTagPlacement
{
	FVector2D Center = FVector2D::ZeroVector;
	FVector2D Size = FVector2D::ZeroVector;
	bool bIsVisible = true;
};

/**
 * @brief The maths behind the world map tags as pure functions, so turns away, dashes, the country label point and
 * the tag placement are unit tested and the widget only places what these return.
 */
namespace WorldMapTagsMath
{
	/**
	 * @brief Length of the rest of a piece's move order in XY, the way the division consumes it each turn.
	 * @param CurrentLocation Where the piece stands now.
	 * @param PathPoints Points of the move order.
	 * @param NextPathPointIndex First point not reached yet; an invalid index means nothing is left.
	 * @return XY distance still to travel; 0 when the order is used up.
	 */
	RTS_SURVIVAL_API float GetRemainingPathLengthXY(const FVector& CurrentLocation, const TArray<FVector>& PathPoints,
	                                                int32 NextPathPointIndex);

	/**
	 * @brief Enemy turns a piece needs to reach the end of its order.
	 * @param RemainingDistance XY distance still to travel.
	 * @param DistancePerTurn XY distance one turn of movement covers.
	 * @return 0 when nothing is left, otherwise at least 1; INDEX_NONE when the piece cannot move.
	 */
	RTS_SURVIVAL_API int32 GetTurnsAway(float RemainingDistance, float DistancePerTurn);

	/**
	 * @brief Cuts a segment to a rectangle so a target far off screen never produces thousands of dashes.
	 * @param InOutStart Segment start; moved onto the rectangle edge when it lies outside.
	 * @param InOutEnd Segment end; moved onto the rectangle edge when it lies outside.
	 * @param RectMin Top-left of the rectangle.
	 * @param RectMax Bottom-right of the rectangle.
	 * @return false when no part of the segment lies inside the rectangle.
	 */
	RTS_SURVIVAL_API bool ClipSegmentToRect(FVector2D& InOutStart, FVector2D& InOutEnd, const FVector2D& RectMin,
	                                        const FVector2D& RectMax);

	/**
	 * @brief Splits a line into dashes; the last dash is shortened to end exactly on the line's end.
	 * @param Start Line start.
	 * @param End Line end.
	 * @param DashLength Length of one dash; must be positive.
	 * @param GapLength Length of the gap after each dash.
	 * @param MaxDashCount Upper bound on the dashes written.
	 * @param OutDashes Receives the dashes; emptied first.
	 */
	RTS_SURVIVAL_API void BuildDashSegments(const FVector2D& Start, const FVector2D& End, float DashLength,
	                                        float GapLength, int32 MaxDashCount,
	                                        TArray<FWorldMapDashSegment>& OutDashes);

	/**
	 * @brief Where a country's name goes: the mean of the anchors the country regulator mapped to the country. The
	 * mesh bounds are not used because Russia's and France's bounds span most of the map.
	 * @param AnchorLocations World locations of the country's anchors.
	 * @param OutLabelPoint Receives the label point.
	 * @return false when the country has no anchors.
	 */
	RTS_SURVIVAL_API bool TryGetCountryLabelPoint(const TArray<FVector>& AnchorLocations, FVector& OutLabelPoint);

	/**
	 * @brief A tag is only drawn when all of it lies below the top bar, so the bar never cuts it in half.
	 * @param TagCenter Middle of the tag.
	 * @param TagSize Size of the tag.
	 * @param TopBarHeight Height of the band at the top of the overlay that tags must stay out of.
	 * @return false when any part of the tag would lie in the band.
	 */
	RTS_SURVIVAL_API bool GetIsTagClearOfTopBar(const FVector2D& TagCenter, const FVector2D& TagSize,
	                                            float TopBarHeight);

	/**
	 * @brief Whether two tags are closer than the gap that must stay free between them.
	 * @param CenterA Middle of the first tag.
	 * @param SizeA Size of the first tag.
	 * @param CenterB Middle of the second tag.
	 * @param SizeB Size of the second tag.
	 * @param Gap Space that must stay free between the two; tags exactly this far apart do not overlap.
	 * @return true when the tags overlap or leave less than the gap.
	 */
	RTS_SURVIVAL_API bool GetDoTagsOverlap(const FVector2D& CenterA, const FVector2D& SizeA, const FVector2D& CenterB,
	                                       const FVector2D& SizeB, float Gap);

	/**
	 * @brief Lifts a tag above a map marker (the player HQ) it would cover; a tag that is clear stays where it is.
	 * @param TagCenter Middle of the tag where its country puts it.
	 * @param TagSize Size of the tag.
	 * @param MarkerCenter Middle of the marker's box.
	 * @param MarkerHalfSize Half the width and height the marker takes on screen.
	 * @param Gap Space kept between the marker's box and the tag.
	 * @return Middle of the tag to use.
	 */
	RTS_SURVIVAL_API FVector2D GetTagCenterClearOfMarker(const FVector2D& TagCenter, const FVector2D& TagSize,
	                                                     const FVector2D& MarkerCenter,
	                                                     const FVector2D& MarkerHalfSize, float Gap);

	/**
	 * @brief Keeps tags off the top bar and off each other. Earlier tags win: a later tag that comes too close to a
	 * kept one is nudged straight above or below it, and hidden when that spot is taken or under the bar.
	 * @param InOutTags Tags in priority order; hidden ones are skipped. Receives the nudged middles and who is hidden.
	 * @param Gap Space kept between two tags.
	 * @param TopBarHeight Height of the band at the top of the overlay that tags must stay out of.
	 */
	RTS_SURVIVAL_API void ResolveTagOverlaps(TArray<FWorldMapTagPlacement>& InOutTags, float Gap, float TopBarHeight);
}
