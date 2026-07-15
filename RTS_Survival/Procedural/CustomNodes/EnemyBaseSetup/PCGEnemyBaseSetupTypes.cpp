// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.
#include "PCGEnemyBaseSetupTypes.h"

// ---------------------------------------------------------------------------
// FEnemyFootprint
// ---------------------------------------------------------------------------

FEnemyFootprint FEnemyFootprint::Inflated(const double Amount) const
{
	FEnemyFootprint Result = *this;
	Result.HalfExtents.X = FMath::Max(1.0, HalfExtents.X + Amount);
	Result.HalfExtents.Y = FMath::Max(1.0, HalfExtents.Y + Amount);
	return Result;
}

void FEnemyFootprint::GetAxes(FVector2D& OutAxisX, FVector2D& OutAxisY) const
{
	double SinYaw = 0.0;
	double CosYaw = 0.0;
	FMath::SinCos(&SinYaw, &CosYaw, YawRadians);
	OutAxisX = FVector2D(CosYaw, SinYaw);
	OutAxisY = FVector2D(-SinYaw, CosYaw);
}

void FEnemyFootprint::GetCorners(FVector2D OutCorners[4]) const
{
	FVector2D AxisX;
	FVector2D AxisY;
	GetAxes(AxisX, AxisY);

	const FVector2D ExtentX = AxisX * HalfExtents.X;
	const FVector2D ExtentY = AxisY * HalfExtents.Y;

	OutCorners[0] = Center + ExtentX + ExtentY;
	OutCorners[1] = Center + ExtentX - ExtentY;
	OutCorners[2] = Center - ExtentX - ExtentY;
	OutCorners[3] = Center - ExtentX + ExtentY;
}

FBox2D FEnemyFootprint::GetAABB() const
{
	FVector2D Corners[4];
	GetCorners(Corners);

	FBox2D Result(ForceInit);
	for (int32 CornerIndex = 0; CornerIndex < 4; ++CornerIndex)
	{
		Result += Corners[CornerIndex];
	}
	return Result;
}

double FEnemyFootprint::SupportRadius(const FVector2D& WorldDirection) const
{
	FVector2D AxisX;
	FVector2D AxisY;
	GetAxes(AxisX, AxisY);

	return FMath::Abs(FVector2D::DotProduct(AxisX, WorldDirection)) * HalfExtents.X
		+ FMath::Abs(FVector2D::DotProduct(AxisY, WorldDirection)) * HalfExtents.Y;
}

namespace
{
	/** @brief True when the two footprints overlap when projected onto the given axis. */
	bool EBS_OverlapsOnAxis(const FEnemyFootprint& A, const FEnemyFootprint& B, const FVector2D& Axis)
	{
		const double CenterDistance = FMath::Abs(FVector2D::DotProduct(B.Center - A.Center, Axis));
		return CenterDistance <= A.SupportRadius(Axis) + B.SupportRadius(Axis);
	}
}

bool FEnemyFootprint::Overlaps(const FEnemyFootprint& Other) const
{
	FVector2D AxisX;
	FVector2D AxisY;
	GetAxes(AxisX, AxisY);

	FVector2D OtherAxisX;
	FVector2D OtherAxisY;
	Other.GetAxes(OtherAxisX, OtherAxisY);

	// SAT: two oriented rectangles overlap iff no separating axis exists among their 4 axes.
	return EBS_OverlapsOnAxis(*this, Other, AxisX)
		&& EBS_OverlapsOnAxis(*this, Other, AxisY)
		&& EBS_OverlapsOnAxis(*this, Other, OtherAxisX)
		&& EBS_OverlapsOnAxis(*this, Other, OtherAxisY);
}

// ---------------------------------------------------------------------------
// FEnemyOccupancyGrid
// ---------------------------------------------------------------------------

FEnemyOccupancyGrid::FEnemyOccupancyGrid(const double InCellSize)
	: M_CellSize(FMath::Max(100.0, InCellSize))
{
}

FIntPoint FEnemyOccupancyGrid::CellOf(const FVector2D& Position) const
{
	return FIntPoint(
		FMath::FloorToInt32(Position.X / M_CellSize),
		FMath::FloorToInt32(Position.Y / M_CellSize));
}

void FEnemyOccupancyGrid::ForEachCellOf(
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

int32 FEnemyOccupancyGrid::Add(const FEnemyFootprint& Footprint, const EEnemyPlacementCategory Type)
{
	const int32 EntryIndex = M_Entries.Add(FEntry{Footprint, Type});

	ForEachCellOf(Footprint.GetAABB(), [this, EntryIndex](const FIntPoint& Cell)
	{
		M_Cells.FindOrAdd(Cell).Add(EntryIndex);
	});

	return EntryIndex;
}

bool FEnemyOccupancyGrid::OverlapsAny(const FEnemyFootprint& Footprint, const uint8 TypeMask) const
{
	bool bOverlaps = false;
	// Entries can span multiple cells; remember which were already SAT-tested.
	TSet<int32> TestedEntries;

	ForEachCellOf(Footprint.GetAABB(), [this, &Footprint, TypeMask, &bOverlaps, &TestedEntries](const FIntPoint& Cell)
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
			if ((EnemyOccupancyMask(Entry.Type) & TypeMask) == 0)
			{
				continue;
			}

			if (Entry.Footprint.Overlaps(Footprint))
			{
				bOverlaps = true;
				return;
			}
		}
	});

	return bOverlaps;
}
