// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/WorldCountryOccupation/WorldCountryLiberationRules.h"

/**
 * @brief How far every point of a country lies from the country's edge, as a small greyscale picture laid over the
 * country's bounds. The country fill material reads it to draw a border band of any width inside the country.
 */
struct RTS_SURVIVAL_API FWorldCountryEdgeDistanceField
{
	/** Map-plane rectangle the picture covers. */
	FBox2D Bounds = FBox2D(ForceInit);
	int32 Width = 0;
	int32 Height = 0;

	/** Row by row from the low Y edge; 0 = on the edge or outside, 255 = MaxEncodedDistance or further inside. */
	TArray<uint8> Pixels;

	bool GetIsValid() const { return Width > 0 && Height > 0 && Pixels.Num() == Width * Height; }
};

namespace WorldCountryEdgeDistance
{
	// Distances beyond this many map units all read as "far inside"; a border band cannot be wider than this.
	constexpr float MaxEncodedDistance = 64.f;

	/**
	 * @brief Rasterises the country's triangles and measures each inside pixel's distance to the nearest outside one.
	 * The clip rectangle's own edges are not treated as country edges, so a country cut by the map gets no false border.
	 * @param CountryTriangles Triangles of the country fill mesh on the map plane.
	 * @param ClipRect Part of the map that matters; overseas parts of a country outside it are ignored.
	 * @param MaxPixelsPerSide Upper limit for the longer side of the picture.
	 * @return The field; invalid when the country has no triangle inside the clip rectangle.
	 */
	RTS_SURVIVAL_API FWorldCountryEdgeDistanceField BuildEdgeDistanceField(
		const TArray<FWorldCountryTriangle2D>& CountryTriangles, const FBox2D& ClipRect, int32 MaxPixelsPerSide);
}
