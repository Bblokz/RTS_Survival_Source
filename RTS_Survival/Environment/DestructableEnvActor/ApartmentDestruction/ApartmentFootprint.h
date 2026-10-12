// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "ApartmentFootprint.generated.h"

class UGeometryCollection;

/** @brief How the ground outline of an apartment is turned into an outer ring and an inner core for navigation. */
USTRUCT(BlueprintType)
struct FApartmentFootprintSettings
{
	GENERATED_BODY()

	// How far the outer ring reaches beyond the walls, so units that may not enter keep clear of falling debris.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footprint", meta = (ClampMin = "0.0"))
	float OuterOffset = 250.f;

	// How far inside the walls the outer ring reaches. The navigation mesh widens the blocked core by the radius of
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
	// Walls plus the offset around them; together with the core this covers the whole footprint without overlap.
	TArray<FBox2D> OuterRingBoxes;
	// Everything deeper inside than the ring; no narrow part of the building is left without a core.
	TArray<FBox2D> InnerCoreBoxes;
	// The ring and the core together.
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
};
