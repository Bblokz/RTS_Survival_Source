// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "ApartmentFootprint.generated.h"

class FGeometryCollection;
class UGeometryCollection;

/** @brief How the ground outline of an apartment is turned into an outer ring and an inner core for navigation. */
USTRUCT(BlueprintType)
struct FApartmentFootprintSettings
{
	GENERATED_BODY()

	// Width of the band around the walls; vehicles that may not enter the building keep this far away from it.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footprint", meta = (ClampMin = "0.0"))
	float OuterOffset = 250.f;

	// How far inside the walls the inner ring reaches. The navigation mesh widens the blocked core by the radius of
	// each agent (200 for heavy tanks), so the centre of a heavy tank gets this depth minus its radius past the wall.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footprint", meta = (ClampMin = "0.0"))
	float RingDepth = 300.f;

	// Resolution of the outline; offsets and depths are rounded to whole cells.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footprint", meta = (ClampMin = "25.0"))
	float CellSize = 50.f;

	// Pieces that end below this height above the pivot (loose rubble) do not count as part of the building.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footprint", meta = (ClampMin = "0.0"))
	float MinPieceTopHeight = 150.f;
};

/** @brief The areas derived from an apartment's outline, as boxes in the space of the geometry collection. */
struct FApartmentFootprintAreas
{
	// The offset around the walls, outside the building.
	TArray<FBox2D> OuterBandBoxes;
	// The walls and the rooms behind them, as deep as the ring depth.
	TArray<FBox2D> InnerRingBoxes;
	// Everything deeper inside than the ring; no narrow part of the building is left without a core.
	TArray<FBox2D> InnerCoreBoxes;
	// The band, the ring and the core together; they do not overlap.
	FBox2D TotalBounds = FBox2D(ForceInit);
};

/**
 * @brief Derives the navigation areas of an apartment from its geometry collection, so any building shape gets an
 * outer ring and an inner core without hand-placed volumes.
 */
struct RTS_SURVIVAL_API FApartmentFootprint
{
	/**
	 * @brief Rasterises the building's pieces to a ground outline, fills courtyards and holes, and splits it by depth.
	 * @param Collection The apartment's rest collection, pivot at the footprint centre on the ground.
	 * @param Settings Offsets and resolution.
	 * @param OutAreas Ring and core boxes in collection space.
	 * @return False when the collection holds no usable pieces or the outline would be unreasonably large.
	 */
	static bool BuildAreas(const UGeometryCollection& Collection, const FApartmentFootprintSettings& Settings,
	                       FApartmentFootprintAreas& OutAreas);

	/**
	 * @brief Bounds of one piece in collection space; also works on a cooked collection whose geometry was stripped.
	 * @param Collection The collection the piece belongs to.
	 * @param TransformIndex Transform of the piece (a leaf).
	 * @param GlobalTransforms Collection-space transform of every transform index.
	 * @param OutBounds Receives the bounds.
	 * @return False for a transform without geometry of its own, such as a cluster.
	 */
	static bool TryGetPieceBounds(const FGeometryCollection& Collection, int32 TransformIndex,
	                              const TArray<FTransform>& GlobalTransforms, FBox& OutBounds);
};
