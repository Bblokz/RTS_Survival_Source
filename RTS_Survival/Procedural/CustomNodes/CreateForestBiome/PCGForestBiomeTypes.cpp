// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.
#include "PCGForestBiomeTypes.h"

// ---------------------------------------------------------------------------
// FForestOccupancyGrid
// ---------------------------------------------------------------------------

FForestOccupancyGrid::FForestOccupancyGrid(const double InCellSize)
	: M_CellSize(FMath::Max(100.0, InCellSize))
{
}

FIntPoint FForestOccupancyGrid::CellOf(const FVector2D& Position) const
{
	return FIntPoint(
		FMath::FloorToInt32(Position.X / M_CellSize),
		FMath::FloorToInt32(Position.Y / M_CellSize));
}

void FForestOccupancyGrid::ForEachCellOf(
	const FBox2D& AABB,
	TFunctionRef<void(const FIntPoint&)> Visitor) const
{
	const FIntPoint MinCell = CellOf(AABB.Min);
	const FIntPoint MaxCell = CellOf(AABB.Max);

	for (int32 CellX = MinCell.X; CellX <= MaxCell.X; ++CellX)
	{
		for (int32 CellY = MinCell.Y; CellY <= MaxCell.Y; ++CellY)
		{
			Visitor(FIntPoint(CellX, CellY));
		}
	}
}

void FForestOccupancyGrid::Add(const FVector2D& Center, const double Radius, const EForestOccupancy Type)
{
	const int32 EntryIndex = M_Entries.Add(FEntry{Center, Radius, Type});

	const FBox2D AABB(Center - FVector2D(Radius), Center + FVector2D(Radius));
	ForEachCellOf(AABB, [this, EntryIndex](const FIntPoint& Cell)
	{
		M_Cells.FindOrAdd(Cell).Add(EntryIndex);
	});
}

bool FForestOccupancyGrid::OverlapsAny(
	const FVector2D& Center,
	const double Radius,
	const uint8 TypeMask,
	const double ExtraClearance) const
{
	// The query reaches this far from its center along each axis; any entry that can be within the
	// required separation is registered in at least one of the cells this box covers.
	const double Reach = Radius + FMath::Max(0.0, ExtraClearance);
	const FBox2D QueryAABB(Center - FVector2D(Reach), Center + FVector2D(Reach));

	bool bOverlaps = false;
	// Entries can span multiple cells; remember which were already distance-tested.
	TSet<int32> TestedEntries;

	ForEachCellOf(QueryAABB, [&](const FIntPoint& Cell)
	{
		if (bOverlaps)
		{
			return;
		}

		const TArray<int32>* CellEntries = M_Cells.Find(Cell);
		if (CellEntries == nullptr)
		{
			return;
		}

		for (const int32 EntryIndex : *CellEntries)
		{
			bool bAlreadyTested = false;
			TestedEntries.Add(EntryIndex, &bAlreadyTested);
			if (bAlreadyTested)
			{
				continue;
			}

			const FEntry& Entry = M_Entries[EntryIndex];
			if ((ForestOccupancyMask(Entry.Type) & TypeMask) == 0)
			{
				continue;
			}

			const double MinSeparation = Radius + Entry.Radius + FMath::Max(0.0, ExtraClearance);
			if (FVector2D::DistSquared(Center, Entry.Center) < MinSeparation * MinSeparation)
			{
				bOverlaps = true;
				return;
			}
		}
	});

	return bOverlaps;
}
