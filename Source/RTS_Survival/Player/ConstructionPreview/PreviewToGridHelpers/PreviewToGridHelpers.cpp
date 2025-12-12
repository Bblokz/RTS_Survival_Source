// Copyright (C) Bas Blokzijl - All rights reserved.
#include "PreviewToGridHelpers.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Player/ConstructionPreview/BuildingGridOverlay/BuildingGridOverlay.h"

void FPreviewToGridHelpers::ComputeGridIndicesUnderPreview(
	const ABuildingGridOverlay* Grid,
	const UStaticMeshComponent* PreviewMesh,
	TArray<int32>& OutInstanceIndices)
{
	OutInstanceIndices.Reset();

	if (not IsValid(Grid) || not IsValid(PreviewMesh))
	{
		RTSFunctionLibrary::ReportError(TEXT("ComputeGridIndicesUnderPreview: Invalid Grid or PreviewMesh."));
		return;
	}

	FBox2D AABB2D;
	GetPreviewAABB2D(PreviewMesh, AABB2D);
	if (not AABB2D.bIsValid)
	{
		RTSFunctionLibrary::ReportError(TEXT("ComputeGridIndicesUnderPreview: Preview AABB2D invalid."));
		return;
	}

	// Convert to grid-local space (XY).
	const FBox2D LocalAABB = ToGridLocalAABB2D(Grid, AABB2D);

	// Compute inclusive index ranges in grid-local coordinates.
	int32 MinIx = 0, MaxIx = -1, MinIy = 0, MaxIy = -1;
	ComputeIndexRange_Local(Grid, LocalAABB, MinIx, MaxIx, MinIy, MaxIy);

	// If empty after clamping, nothing to add.
	if (MaxIx < MinIx || MaxIy < MinIy)
	{
		return;
	}

	for (int32 Iy = MinIy; Iy <= MaxIy; ++Iy)
	{
		for (int32 Ix = MinIx; Ix <= MaxIx; ++Ix)
		{
			int32 InstanceIdx = INDEX_NONE;
			if (TryLocalToInstanceIndex(Grid, Ix, Iy, InstanceIdx))
			{
				OutInstanceIndices.Add(InstanceIdx);
			}
		}
	}
}

void FPreviewToGridHelpers::ComputeGridIndicesForFootprint_GridAligned(
	const ABuildingGridOverlay* Grid,
	const FVector& CenterWorld,
	float FootprintSizeXUU,
	float FootprintSizeYUU,
	TArray<int32>& OutInstanceIndices)
{
	OutInstanceIndices.Reset();

	if (!Grid)
	{
		return;
	}

	// We assume the grid was already built (it is for the preview).
	// We need:
	//  - N (grid dimension)
	
	//  - step between neighbors along X and Y (may include visual gap)
	const int32 N = Grid->GetGridCount();                    
	const float CellSize = Grid->GetCellSize();           
	if (N <= 0 || CellSize <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// Instance [0,0] (row-major build order).
	FTransform T00;
	if (!Grid->GetInstanceWorldTransform(/*Index=*/0, T00))
	{
		return;
	}

	// Neighbor along +X: [1,0] is index 1 (when N>1)
	FTransform T10;
	const bool bHasXNeighbor = (N > 1) && Grid->GetInstanceWorldTransform(/*Index=*/1, T10);

	// Neighbor along +Y: [0,1] is index N (when N>1)
	FTransform T01;
	const bool bHasYNeighbor = (N > 1) && Grid->GetInstanceWorldTransform(/*Index=*/N, T01);

	// Compute step from instance centers (this captures cell+gap spacing).
	// Fall back to CellSize if neighbors are missing (degenerate tiny grids).
	const FVector P00 = T00.GetLocation();
	const float StepX = bHasXNeighbor ? FMath::Max(1.f, FMath::Abs(T10.GetLocation().X - P00.X)) : CellSize;
	const float StepY = bHasYNeighbor ? FMath::Max(1.f, FMath::Abs(T01.GetLocation().Y - P00.Y)) : CellSize;

	// How many tiles do we need in each axis?
	// Designers author footprint in Unreal Units; interpret it in terms of *tiles* (visual cell size),
	// not in terms of "step" (cell + gap). This keeps a 3x3 footprint == 3 tiles even if a gap is present.
	const int32 WantTilesX = FMath::Max(1, FMath::CeilToInt(FootprintSizeXUU / CellSize));
	const int32 WantTilesY = FMath::Max(1, FMath::CeilToInt(FootprintSizeYUU / CellSize));

	// Find the index of the tile whose center is closest to CenterWorld.
	// First, find the local offset from the [0,0] center.
	const float DX = CenterWorld.X - P00.X;
	const float DY = CenterWorld.Y - P00.Y;

	// Convert to tile coordinates in [0..N-1] by rounding to nearest.
	int32 CenterIx = FMath::Clamp(FMath::RoundToInt(DX / StepX), 0, N - 1);
	int32 CenterIy = FMath::Clamp(FMath::RoundToInt(DY / StepY), 0, N - 1);

	// Build an axis-aligned block of size WantTilesX × WantTilesY around (CenterIx,CenterIy).
	// For even counts, bias +X/+Y by one (e.g., 2 → covers [0..1] from the center).
	const int32 LeftCount   = (WantTilesX - 1) / 2;
	const int32 RightCount  = WantTilesX - LeftCount - 1;
	const int32 DownCount   = (WantTilesY - 1) / 2;
	const int32 UpCount     = WantTilesY - DownCount - 1;

	const int32 MinIx = FMath::Clamp(CenterIx - LeftCount, 0, N - 1);
	const int32 MaxIx = FMath::Clamp(CenterIx + RightCount, 0, N - 1);
	const int32 MinIy = FMath::Clamp(CenterIy - DownCount, 0, N - 1);
	const int32 MaxIy = FMath::Clamp(CenterIy + UpCount,   0, N - 1);

	// Produce the instance indices (row-major: idx = Iy*N + Ix).
	for (int32 Iy = MinIy; Iy <= MaxIy; ++Iy)
	{
		for (int32 Ix = MinIx; Ix <= MaxIx; ++Ix)
		{
			const int32 InstanceIdx = Iy * N + Ix;
			OutInstanceIndices.Add(InstanceIdx);
		}
	}
}



void FPreviewToGridHelpers::GetPreviewAABB2D(const UStaticMeshComponent* PreviewMesh, FBox2D& OutAABB2D)
{
	const FBoxSphereBounds B = PreviewMesh->Bounds; // world-space, includes scale & rotation
	const FVector Ext = B.BoxExtent;
	const FVector Ctr = B.Origin;

	const FVector2D Min2D(Ctr.X - Ext.X, Ctr.Y - Ext.Y);
	const FVector2D Max2D(Ctr.X + Ext.X, Ctr.Y + Ext.Y);
	OutAABB2D = FBox2D(Min2D, Max2D);
}

FBox2D FPreviewToGridHelpers::ToGridLocalAABB2D(const ABuildingGridOverlay* Grid, const FBox2D& WorldAABB2D)
{
	const FVector GridWorld = Grid->GetActorLocation();
	const FVector2D GridXY(GridWorld.X, GridWorld.Y);

	FBox2D Local;
	Local.Min = WorldAABB2D.Min - GridXY;
	Local.Max = WorldAABB2D.Max - GridXY;
	Local.bIsValid = WorldAABB2D.bIsValid;
	return Local;
}

void FPreviewToGridHelpers::ComputeIndexRange_Local(
    const ABuildingGridOverlay* Grid,
    const FBox2D& LocalAABB2D,
    int32& OutMinIx, int32& OutMaxIx,
    int32& OutMinIy, int32& OutMaxIy)
{
    OutMinIx = 0; OutMaxIx = -1;
    OutMinIy = 0; OutMaxIy = -1;

    const int32 N = Grid->GetGridCount();
    if (N <= 0) return;

    // Instance [0,0] – world to local anchor for the grid
    FTransform T00;
    if (!Grid->GetInstanceWorldTransform(0, T00))
        return;

    const FVector GridWorld = Grid->GetActorLocation();
    const FVector2D P00Local(T00.GetLocation().X - GridWorld.X,
                             T00.GetLocation().Y - GridWorld.Y);

    // Measure step (center-to-center) from neighbors; fallback to CellSize.
    float S = Grid->GetCellSize();
    float StepX = S, StepY = S;

    if (N > 1)
    {
        FTransform T10; // [1,0]
        if (Grid->GetInstanceWorldTransform(1, T10))
        {
            StepX = FMath::Max(StepX, FMath::Abs(T10.GetLocation().X - T00.GetLocation().X));
        }
        FTransform T01; // [0,1]
        if (Grid->GetInstanceWorldTransform(N, T01))
        {
            StepY = FMath::Max(StepY, FMath::Abs(T01.GetLocation().Y - T00.GetLocation().Y));
        }
    }

    // Expand by Step/2 so coverage matches UpdateSingleInstanceOverlapState()
    const float MinX = LocalAABB2D.Min.X - 0.5f * StepX;
    const float MaxX = LocalAABB2D.Max.X + 0.5f * StepX;
    const float MinY = LocalAABB2D.Min.Y - 0.5f * StepY;
    const float MaxY = LocalAABB2D.Max.Y + 0.5f * StepY;

    // Convert to index ranges relative to the [0,0] center at P00Local
    const float LowerIx = (MinX - P00Local.X) / StepX;
    const float UpperIx = (MaxX - P00Local.X) / StepX;
    const float LowerIy = (MinY - P00Local.Y) / StepY;
    const float UpperIy = (MaxY - P00Local.Y) / StepY;

    OutMinIx = FMath::Clamp(FMath::CeilToInt(LowerIx), 0, N - 1);
    OutMaxIx = FMath::Clamp(FMath::FloorToInt(UpperIx), 0, N - 1);
    OutMinIy = FMath::Clamp(FMath::CeilToInt(LowerIy), 0, N - 1);
    OutMaxIy = FMath::Clamp(FMath::FloorToInt(UpperIy), 0, N - 1);
}

bool FPreviewToGridHelpers::TryLocalToInstanceIndex(
	const ABuildingGridOverlay* Grid,
	int32 Ix, int32 Iy,
	int32& OutInstanceIdx)
{
	const int32 N = Grid->GetGridCount();

	if (Ix < 0 || Ix >= N || Iy < 0 || Iy >= N)
	{
		return false;
	}

	OutInstanceIdx = Iy * N + Ix; // row-major
	return true;
}
