// Copyright (C) Bas Blokzijl - All rights reserved.

#include "ApartmentFootprint.h"

#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/GeometryCollectionObject.h"

namespace ApartmentFootprint
{
	// Free cells kept around the outline so the outside is one connected region to flood from.
	constexpr int32 GridBorderCells = 2;
	constexpr int32 MaxGridCellsPerAxis = 1024;
	// Pieces that only touch a cell boundary must not claim the neighbouring cell.
	constexpr double PieceShrink = 1.0;
	constexpr int32 UnreachedDistance = MAX_int32;
	const FName StrippedBoundsAttribute(TEXT("BoundingBox"));

	/** @brief Square occupancy grid over the ground plane of the collection. */
	struct FGrid
	{
		FVector2D Origin = FVector2D::ZeroVector;
		double CellSize = 0.0;
		int32 SizeX = 0;
		int32 SizeY = 0;

		int32 Num() const { return SizeX * SizeY; }
		int32 Index(const int32 X, const int32 Y) const { return Y * SizeX + X; }
		bool Contains(const int32 X, const int32 Y) const { return X >= 0 && Y >= 0 && X < SizeX && Y < SizeY; }
	};

	void CollectPieceRectangles(const FGeometryCollection& Collection, const float MinPieceTopHeight,
	                            TArray<FBox2D>& OutRectangles, FBox2D& OutBounds)
	{
		TArray<FTransform> GlobalTransforms;
		GeometryCollectionAlgo::GlobalMatrices(Collection.Transform, Collection.Parent, GlobalTransforms);
		const auto AddPiece = [&](const FBox& LocalBox, const int32 TransformIndex)
		{
			if (not LocalBox.IsValid || not GlobalTransforms.IsValidIndex(TransformIndex))
			{
				return;
			}
			const FBox PieceBox = LocalBox.TransformBy(GlobalTransforms[TransformIndex]);
			if (PieceBox.Max.Z < MinPieceTopHeight)
			{
				return;
			}
			const FBox2D Rectangle(FVector2D(PieceBox.Min), FVector2D(PieceBox.Max));
			OutRectangles.Add(Rectangle);
			OutBounds += Rectangle;
		};
		const int32 GeometryCount = Collection.NumElements(FGeometryCollection::GeometryGroup);
		for (int32 GeometryIndex = 0; GeometryIndex < GeometryCount; ++GeometryIndex)
		{
			AddPiece(Collection.BoundingBox[GeometryIndex], Collection.TransformIndex[GeometryIndex]);
		}
		if (GeometryCount > 0)
		{
			return;
		}
		// A cooked collection that had its geometry stripped keeps the piece bounds per transform instead.
		const TManagedArray<FBox>* StrippedBounds = Collection.FindAttribute<FBox>(
			StrippedBoundsAttribute, FGeometryCollection::TransformGroup);
		if (StrippedBounds == nullptr)
		{
			return;
		}
		for (int32 TransformIndex = 0; TransformIndex < StrippedBounds->Num(); ++TransformIndex)
		{
			// Clusters have no geometry of their own; only leaves carry a piece.
			if (Collection.Children[TransformIndex].IsEmpty())
			{
				AddPiece((*StrippedBounds)[TransformIndex], TransformIndex);
			}
		}
	}

	void MarkPieces(const FGrid& Grid, const TArray<FBox2D>& Rectangles, TArray<bool>& OutOccupied)
	{
		OutOccupied.Init(false, Grid.Num());
		for (const FBox2D& Rectangle : Rectangles)
		{
			const FVector2D Min = (Rectangle.Min + FVector2D(PieceShrink) - Grid.Origin) / Grid.CellSize;
			const FVector2D Max = (Rectangle.Max - FVector2D(PieceShrink) - Grid.Origin) / Grid.CellSize;
			const int32 MinX = FMath::Clamp(FMath::FloorToInt32(Min.X), 0, Grid.SizeX - 1);
			const int32 MinY = FMath::Clamp(FMath::FloorToInt32(Min.Y), 0, Grid.SizeY - 1);
			const int32 MaxX = FMath::Clamp(FMath::FloorToInt32(Max.X), MinX, Grid.SizeX - 1);
			const int32 MaxY = FMath::Clamp(FMath::FloorToInt32(Max.Y), MinY, Grid.SizeY - 1);
			for (int32 Y = MinY; Y <= MaxY; ++Y)
			{
				for (int32 X = MinX; X <= MaxX; ++X)
				{
					OutOccupied[Grid.Index(X, Y)] = true;
				}
			}
		}
	}

	/**
	 * @brief Breadth-first distances in cells from a set of seed cells, spreading only over cells that may be entered.
	 * @param Grid The grid the cells live on.
	 * @param bDiagonal Whether steps may be diagonal; diagonal steps keep the ring equally deep around corners.
	 * @param CanEnter Cells the search may spread into.
	 * @param InOutDistance Seeds are the cells that already hold 0; all others must hold UnreachedDistance.
	 */
	void SpreadDistance(const FGrid& Grid, const bool bDiagonal, const TArray<bool>& CanEnter,
	                    TArray<int32>& InOutDistance)
	{
		TArray<int32> Queue;
		Queue.Reserve(Grid.Num());
		for (int32 CellIndex = 0; CellIndex < Grid.Num(); ++CellIndex)
		{
			if (InOutDistance[CellIndex] == 0)
			{
				Queue.Add(CellIndex);
			}
		}
		for (int32 QueueIndex = 0; QueueIndex < Queue.Num(); ++QueueIndex)
		{
			const int32 CellIndex = Queue[QueueIndex];
			const int32 CellX = CellIndex % Grid.SizeX;
			const int32 CellY = CellIndex / Grid.SizeX;
			for (int32 StepY = -1; StepY <= 1; ++StepY)
			{
				for (int32 StepX = -1; StepX <= 1; ++StepX)
				{
					const bool bIsDiagonalStep = StepX != 0 && StepY != 0;
					if ((StepX == 0 && StepY == 0) || (bIsDiagonalStep && not bDiagonal)
						|| not Grid.Contains(CellX + StepX, CellY + StepY))
					{
						continue;
					}
					const int32 NeighbourIndex = Grid.Index(CellX + StepX, CellY + StepY);
					if (not CanEnter[NeighbourIndex] || InOutDistance[NeighbourIndex] != UnreachedDistance)
					{
						continue;
					}
					InOutDistance[NeighbourIndex] = InOutDistance[CellIndex] + 1;
					Queue.Add(NeighbourIndex);
				}
			}
		}
	}

	/** @brief The outline: every cell the outside cannot reach, so courtyards and blast holes count as building. */
	void FillFootprint(const FGrid& Grid, const TArray<bool>& Occupied, TArray<bool>& OutFootprint)
	{
		TArray<bool> IsFree;
		IsFree.Reserve(Grid.Num());
		for (const bool bOccupied : Occupied)
		{
			IsFree.Add(not bOccupied);
		}
		TArray<int32> DistanceFromBorder;
		DistanceFromBorder.Init(UnreachedDistance, Grid.Num());
		// The border cells are free by construction.
		DistanceFromBorder[Grid.Index(0, 0)] = 0;
		SpreadDistance(Grid, false, IsFree, DistanceFromBorder);
		OutFootprint.Reserve(Grid.Num());
		for (const int32 Distance : DistanceFromBorder)
		{
			OutFootprint.Add(Distance == UnreachedDistance);
		}
	}

	bool GetIsDeepestCellAround(const FGrid& Grid, const TArray<int32>& Depth, const int32 CellX, const int32 CellY)
	{
		const int32 OwnDepth = Depth[Grid.Index(CellX, CellY)];
		for (int32 StepY = -1; StepY <= 1; ++StepY)
		{
			for (int32 StepX = -1; StepX <= 1; ++StepX)
			{
				if (Grid.Contains(CellX + StepX, CellY + StepY)
					&& Depth[Grid.Index(CellX + StepX, CellY + StepY)] > OwnDepth)
				{
					return false;
				}
			}
		}
		return true;
	}

	/**
	 * @brief Splits the outline into the core and the ring.
	 * @param Grid The grid the cells live on.
	 * @param Footprint The filled outline.
	 * @param RingDepthCells How deep the ring reaches into the outline.
	 * @param OuterOffsetCells How far the ring reaches beyond the outline.
	 * @param OutCore Cells deeper than the ring, plus the spine of parts too narrow to have any.
	 * @param OutRing Remaining outline cells plus the offset around the outline.
	 */
	void SplitRingAndCore(const FGrid& Grid, const TArray<bool>& Footprint, const int32 RingDepthCells,
	                      const int32 OuterOffsetCells, TArray<bool>& OutCore, TArray<bool>& OutRing)
	{
		TArray<bool> IsOutside;
		TArray<bool> AllCells;
		TArray<int32> DepthIntoFootprint;
		TArray<int32> DistanceFromFootprint;
		IsOutside.Reserve(Grid.Num());
		AllCells.Init(true, Grid.Num());
		DepthIntoFootprint.Reserve(Grid.Num());
		DistanceFromFootprint.Reserve(Grid.Num());
		for (const bool bInFootprint : Footprint)
		{
			IsOutside.Add(not bInFootprint);
			DepthIntoFootprint.Add(bInFootprint ? UnreachedDistance : 0);
			DistanceFromFootprint.Add(bInFootprint ? 0 : UnreachedDistance);
		}
		SpreadDistance(Grid, true, AllCells, DepthIntoFootprint);
		SpreadDistance(Grid, true, IsOutside, DistanceFromFootprint);

		OutCore.Init(false, Grid.Num());
		OutRing.Init(false, Grid.Num());
		for (int32 CellY = 0; CellY < Grid.SizeY; ++CellY)
		{
			for (int32 CellX = 0; CellX < Grid.SizeX; ++CellX)
			{
				const int32 CellIndex = Grid.Index(CellX, CellY);
				if (not Footprint[CellIndex])
				{
					OutRing[CellIndex] = DistanceFromFootprint[CellIndex] <= OuterOffsetCells;
					continue;
				}
				// A part narrower than twice the ring depth has no deep cells; its spine still has to block.
				const bool bIsCore = DepthIntoFootprint[CellIndex] > RingDepthCells
					|| GetIsDeepestCellAround(Grid, DepthIntoFootprint, CellX, CellY);
				OutCore[CellIndex] = bIsCore;
				OutRing[CellIndex] = not bIsCore;
			}
		}
	}

	/** @brief Covers the marked cells with as few boxes as a row-by-row merge gives; the mask is consumed. */
	void MergeCellsIntoBoxes(const FGrid& Grid, TArray<bool>& InOutMask, TArray<FBox2D>& OutBoxes)
	{
		for (int32 StartY = 0; StartY < Grid.SizeY; ++StartY)
		{
			for (int32 StartX = 0; StartX < Grid.SizeX; ++StartX)
			{
				if (not InOutMask[Grid.Index(StartX, StartY)])
				{
					continue;
				}
				int32 EndX = StartX;
				while (EndX + 1 < Grid.SizeX && InOutMask[Grid.Index(EndX + 1, StartY)])
				{
					++EndX;
				}
				const auto IsRowFull = [&Grid, &InOutMask, StartX, EndX](const int32 RowY)
				{
					for (int32 X = StartX; X <= EndX; ++X)
					{
						if (not InOutMask[Grid.Index(X, RowY)])
						{
							return false;
						}
					}
					return true;
				};
				int32 EndY = StartY;
				while (EndY + 1 < Grid.SizeY && IsRowFull(EndY + 1))
				{
					++EndY;
				}
				for (int32 Y = StartY; Y <= EndY; ++Y)
				{
					for (int32 X = StartX; X <= EndX; ++X)
					{
						InOutMask[Grid.Index(X, Y)] = false;
					}
				}
				OutBoxes.Emplace(Grid.Origin + FVector2D(StartX, StartY) * Grid.CellSize,
				                 Grid.Origin + FVector2D(EndX + 1, EndY + 1) * Grid.CellSize);
			}
		}
	}
}

bool FApartmentFootprint::BuildAreas(const UGeometryCollection& Collection,
                                     const FApartmentFootprintSettings& Settings,
                                     FApartmentFootprintAreas& OutAreas)
{
	using namespace ApartmentFootprint;
	OutAreas = FApartmentFootprintAreas();
	const TSharedPtr<const FGeometryCollection, ESPMode::ThreadSafe> GeometryCollection = Collection.
		GetGeometryCollection();
	if (not GeometryCollection.IsValid() || Settings.CellSize <= 0.f)
	{
		return false;
	}
	TArray<FBox2D> PieceRectangles;
	FBox2D PieceBounds(ForceInit);
	CollectPieceRectangles(*GeometryCollection, Settings.MinPieceTopHeight, PieceRectangles, PieceBounds);
	if (PieceRectangles.IsEmpty())
	{
		return false;
	}

	const int32 OuterOffsetCells = FMath::RoundToInt32(Settings.OuterOffset / Settings.CellSize);
	const int32 RingDepthCells = FMath::RoundToInt32(Settings.RingDepth / Settings.CellSize);
	const int32 MarginCells = OuterOffsetCells + GridBorderCells;
	FGrid Grid;
	Grid.CellSize = Settings.CellSize;
	Grid.Origin = PieceBounds.Min - FVector2D(MarginCells * Grid.CellSize);
	const FVector2D PieceSize = PieceBounds.GetSize();
	Grid.SizeX = FMath::CeilToInt32(PieceSize.X / Grid.CellSize) + 2 * MarginCells;
	Grid.SizeY = FMath::CeilToInt32(PieceSize.Y / Grid.CellSize) + 2 * MarginCells;
	if (Grid.SizeX > MaxGridCellsPerAxis || Grid.SizeY > MaxGridCellsPerAxis)
	{
		return false;
	}

	TArray<bool> Occupied;
	TArray<bool> Footprint;
	TArray<bool> Core;
	TArray<bool> Ring;
	MarkPieces(Grid, PieceRectangles, Occupied);
	FillFootprint(Grid, Occupied, Footprint);
	SplitRingAndCore(Grid, Footprint, RingDepthCells, OuterOffsetCells, Core, Ring);
	MergeCellsIntoBoxes(Grid, Core, OutAreas.InnerCoreBoxes);
	MergeCellsIntoBoxes(Grid, Ring, OutAreas.OuterRingBoxes);
	for (const FBox2D& RingBox : OutAreas.OuterRingBoxes)
	{
		OutAreas.TotalBounds += RingBox;
	}
	for (const FBox2D& CoreBox : OutAreas.InnerCoreBoxes)
	{
		OutAreas.TotalBounds += CoreBox;
	}
	return true;
}
