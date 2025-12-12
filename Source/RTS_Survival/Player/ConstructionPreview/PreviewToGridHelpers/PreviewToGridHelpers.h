// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"

class UStaticMeshComponent;
class ABuildingGridOverlay;

/**
 * Helpers to map a preview mesh footprint to grid instance indices.
 * Mapping is computed in the grid actor's local space (world XY minus grid XY),
 * so it remains correct while the grid moves with the preview.
 */
struct FPreviewToGridHelpers
{
	/**
	 * Compute the instance indices on the grid overlay that lie under the
	 * preview mesh projection onto the XY plane. Rounds outward so that any
	 * partial coverage includes the full tile.
	 *
	 * @param Grid Overlay actor containing the instanced grid (attached to preview).
	 * @param PreviewMesh Static mesh component of the construction preview.
	 * @param OutInstanceIndices Resulting instance indices within the overlay ISM.
	 */
	static void ComputeGridIndicesUnderPreview(
		const ABuildingGridOverlay* Grid,
		const UStaticMeshComponent* PreviewMesh,
		TArray<int32>& OutInstanceIndices);



	
/** Grid-aligned footprint: pick a fixed N×M block of tiles centered on CenterWorld (ignores preview yaw). */
static void ComputeGridIndicesForFootprint_GridAligned(
	const ABuildingGridOverlay* Grid,
	const FVector& CenterWorld,
	float FootprintSizeXUU,
	float FootprintSizeYUU,
	TArray<int32>& OutInstanceIndices);

private:
	static void GetPreviewAABB2D(const UStaticMeshComponent* PreviewMesh, FBox2D& OutAABB2D);

	/** Convert world AABB to grid-local AABB (XY only). */
	static FBox2D ToGridLocalAABB2D(const ABuildingGridOverlay* Grid, const FBox2D& WorldAABB2D);

	/**
	 * Given a grid-local AABB and grid settings, compute the inclusive index
	 * ranges [MinIx..MaxIx], [MinIy..MaxIy] of tile centers that intersect it,
	 * using outward rounding (± CellSize/2 expansion).
	 */
	static void ComputeIndexRange_Local(
		const ABuildingGridOverlay* Grid,
		const FBox2D& LocalAABB2D,
		int32& OutMinIx, int32& OutMaxIx,
		int32& OutMinIy, int32& OutMaxIy);

	/** Map (Ix,Iy) → instance index (row-major). */
	static bool TryLocalToInstanceIndex(
		const ABuildingGridOverlay* Grid,
		int32 Ix, int32 Iy,
		int32& OutInstanceIdx);
};
