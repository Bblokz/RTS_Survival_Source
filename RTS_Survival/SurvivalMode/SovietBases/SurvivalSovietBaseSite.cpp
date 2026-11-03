// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalSovietBaseSite.h"

#include "Algo/Reverse.h"
#include "RTS_Survival/Procedural/CustomNodes/EnemyBaseSetup/EnemyBaseSetupPlacement.h"

namespace SovietBaseSiteConstants
{
	constexpr double CellSize = 400.0;
	// tan(4 deg): the terraces of a start location are flat within about 2 degrees.
	constexpr double MaxBuildSlope = 0.07;
	constexpr double MaxFootprintStep = 45.0;
	// tan(27 deg): ramps of a start location run up to about 23 degrees.
	constexpr double MaxDrivableSlope = 0.51;
	constexpr int32 SectorCount = 36;
	constexpr double ApproachStartFraction = 1.04;
	constexpr double ApproachDepth = 3000.0;
	constexpr double ApproachStep = 400.0;
	constexpr double OpenSectorThreshold = 0.75;
	constexpr double OverlookMinHeight = 180.0;
	constexpr double OverlookMinDistanceFraction = 0.75;
	constexpr double OverlookSpacing = 2200.0;
	constexpr int32 OverlookFlatCells = 2;
	constexpr double MaxOverlookRelief = 60.0;
	constexpr int32 FootprintSamplesPerSide = 3;
	constexpr int32 MaxPathExpansions = 12000;
}

void FSurvivalSovietSiteMap::Init(const FVector2D& InCenter, const double InCenterHeight, const double InBuildRadius,
                                  const double InReach)
{
	using namespace SovietBaseSiteConstants;
	M_Center = InCenter;
	M_CenterHeight = InCenterHeight;
	M_BuildRadius = InBuildRadius;
	M_OuterRadius = InBuildRadius + InReach;
	M_Dimension = FMath::CeilToInt32(2.0 * M_OuterRadius / CellSize) + 1;
	M_Origin = InCenter - FVector2D(M_OuterRadius, M_OuterRadius);
	const int32 CellCount = M_Dimension * M_Dimension;
	M_Heights.Init(0.f, CellCount);
	M_HasGround.Init(false, CellCount);
	M_Slopes.Init(0.f, CellCount);
	M_Drivable.Init(false, CellCount);
}

FVector2D FSurvivalSovietSiteMap::GetSamplePosition(const int32 SampleIndex) const
{
	const int32 Y = SampleIndex / M_Dimension;
	const int32 X = SampleIndex % M_Dimension;
	return M_Origin + FVector2D(X, Y) * SovietBaseSiteConstants::CellSize;
}

void FSurvivalSovietSiteMap::SetSample(const int32 SampleIndex, const double Height, const bool bHasGround)
{
	if (M_Heights.IsValidIndex(SampleIndex))
	{
		M_Heights[SampleIndex] = static_cast<float>(Height - M_CenterHeight);
		M_HasGround[SampleIndex] = bHasGround;
	}
}

FIntPoint FSurvivalSovietSiteMap::GetCell(const FVector2D& Position) const
{
	const FVector2D Local = (Position - M_Origin) / SovietBaseSiteConstants::CellSize;
	return FIntPoint(FMath::RoundToInt32(Local.X), FMath::RoundToInt32(Local.Y));
}

void FSurvivalSovietSiteMap::Finish()
{
	BuildSlopes();
	FloodDrivable();
	BuildApproaches();
	BuildOverlooks();
}

double FSurvivalSovietSiteMap::SampleSlopeAt(const int32 X, const int32 Y) const
{
	using namespace SovietBaseSiteConstants;
	const int32 Index = GetIndex(X, Y);
	if (not M_HasGround[Index])
	{
		return TNumericLimits<float>::Max();
	}
	double MaxStep = 0.0;
	const FIntPoint Neighbours[4] = {{X + 1, Y}, {X - 1, Y}, {X, Y + 1}, {X, Y - 1}};
	for (const FIntPoint& Neighbour : Neighbours)
	{
		if (not GetIsValidCell(Neighbour.X, Neighbour.Y))
		{
			continue;
		}
		const int32 NeighbourIndex = GetIndex(Neighbour.X, Neighbour.Y);
		if (not M_HasGround[NeighbourIndex])
		{
			return TNumericLimits<float>::Max();
		}
		MaxStep = FMath::Max(MaxStep, FMath::Abs<double>(M_Heights[NeighbourIndex] - M_Heights[Index]));
	}
	return MaxStep / CellSize;
}

void FSurvivalSovietSiteMap::BuildSlopes()
{
	for (int32 Y = 0; Y < M_Dimension; ++Y)
	{
		for (int32 X = 0; X < M_Dimension; ++X)
		{
			M_Slopes[GetIndex(X, Y)] = static_cast<float>(SampleSlopeAt(X, Y));
		}
	}
}

void FSurvivalSovietSiteMap::FloodDrivable()
{
	using namespace SovietBaseSiteConstants;
	const FIntPoint Start = GetCell(M_Center);
	if (not GetIsValidCell(Start.X, Start.Y))
	{
		return;
	}
	TArray<FIntPoint> Open;
	Open.Add(Start);
	M_Drivable[GetIndex(Start.X, Start.Y)] = true;
	for (int32 Head = 0; Head < Open.Num(); ++Head)
	{
		const FIntPoint Cell = Open[Head];
		const FIntPoint Neighbours[4] = {
			{Cell.X + 1, Cell.Y}, {Cell.X - 1, Cell.Y}, {Cell.X, Cell.Y + 1}, {Cell.X, Cell.Y - 1}
		};
		for (const FIntPoint& Neighbour : Neighbours)
		{
			if (not GetIsValidCell(Neighbour.X, Neighbour.Y))
			{
				continue;
			}
			const int32 Index = GetIndex(Neighbour.X, Neighbour.Y);
			if (M_Drivable[Index] or M_Slopes[Index] > MaxDrivableSlope)
			{
				continue;
			}
			M_Drivable[Index] = true;
			Open.Add(Neighbour);
		}
	}
}

bool FSurvivalSovietSiteMap::GetIsCellDrivable(const FVector2D& Position) const
{
	const FIntPoint Cell = GetCell(Position);
	return GetIsValidCell(Cell.X, Cell.Y) and M_Drivable[GetIndex(Cell.X, Cell.Y)];
}

void FSurvivalSovietSiteMap::BuildApproaches()
{
	using namespace SovietBaseSiteConstants;
	M_SectorOpenness.Init(0.0, SectorCount);
	const double StartRadius = M_BuildRadius * ApproachStartFraction;
	const int32 Steps = FMath::Max(1, FMath::FloorToInt32(ApproachDepth / ApproachStep));
	for (int32 Sector = 0; Sector < SectorCount; ++Sector)
	{
		const double Yaw = 2.0 * PI * Sector / SectorCount;
		const FVector2D Direction(FMath::Cos(Yaw), FMath::Sin(Yaw));
		int32 Open = 0;
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			Open += GetIsCellDrivable(M_Center + Direction * (StartRadius + Step * ApproachStep)) ? 1 : 0;
		}
		M_SectorOpenness[Sector] = static_cast<double>(Open) / Steps;
	}

	// One yaw per continuous open arc: walk from a closed sector so no arc is cut in two.
	int32 FirstClosed = INDEX_NONE;
	for (int32 Sector = 0; Sector < SectorCount and FirstClosed == INDEX_NONE; ++Sector)
	{
		FirstClosed = M_SectorOpenness[Sector] < OpenSectorThreshold ? Sector : INDEX_NONE;
	}
	M_ApproachYaws.Reset();
	if (FirstClosed == INDEX_NONE)
	{
		return;
	}
	TArray<TPair<int32, double>> Arcs;
	int32 ArcLength = 0;
	for (int32 Offset = 1; Offset <= SectorCount; ++Offset)
	{
		const int32 Sector = (FirstClosed + Offset) % SectorCount;
		if (M_SectorOpenness[Sector] >= OpenSectorThreshold)
		{
			++ArcLength;
			continue;
		}
		if (ArcLength > 0)
		{
			const double MiddleSector = FirstClosed + Offset - (ArcLength + 1) * 0.5;
			Arcs.Add({ArcLength, 2.0 * PI * MiddleSector / SectorCount});
		}
		ArcLength = 0;
	}
	Arcs.Sort([](const TPair<int32, double>& Left, const TPair<int32, double>& Right)
	{
		return Left.Key > Right.Key;
	});
	for (const TPair<int32, double>& Arc : Arcs)
	{
		M_ApproachYaws.Add(Arc.Value);
	}
}

double FSurvivalSovietSiteMap::GetApproachOpenness(const double YawRadians) const
{
	using namespace SovietBaseSiteConstants;
	if (M_SectorOpenness.IsEmpty())
	{
		return 1.0;
	}
	const double Normalized = FMath::Fmod(YawRadians, 2.0 * PI);
	const double Positive = Normalized < 0.0 ? Normalized + 2.0 * PI : Normalized;
	const int32 Sector = FMath::RoundToInt32(Positive / (2.0 * PI) * SectorCount) % SectorCount;
	return M_SectorOpenness[Sector];
}

bool FSurvivalSovietSiteMap::GetIsCellOverlook(const int32 X, const int32 Y) const
{
	using namespace SovietBaseSiteConstants;
	const double CellHeight = M_Heights[GetIndex(X, Y)];
	if (CellHeight < OverlookMinHeight)
	{
		return false;
	}
	for (int32 OffsetY = -OverlookFlatCells; OffsetY <= OverlookFlatCells; ++OffsetY)
	{
		for (int32 OffsetX = -OverlookFlatCells; OffsetX <= OverlookFlatCells; ++OffsetX)
		{
			if (not GetIsValidCell(X + OffsetX, Y + OffsetY))
			{
				return false;
			}
			const int32 Index = GetIndex(X + OffsetX, Y + OffsetY);
			if (not M_HasGround[Index] or FMath::Abs(M_Heights[Index] - CellHeight) > MaxOverlookRelief)
			{
				return false;
			}
		}
	}
	return true;
}

void FSurvivalSovietSiteMap::BuildOverlooks()
{
	using namespace SovietBaseSiteConstants;
	struct FCandidate
	{
		FVector2D Position;
		double Height = 0.0;
	};
	TArray<FCandidate> Candidates;
	const double MinDistanceSquared = FMath::Square(M_BuildRadius * OverlookMinDistanceFraction);
	for (int32 Y = 0; Y < M_Dimension; ++Y)
	{
		for (int32 X = 0; X < M_Dimension; ++X)
		{
			const FVector2D Position = GetSamplePosition(GetIndex(X, Y));
			if (FVector2D::DistSquared(Position, M_Center) >= MinDistanceSquared and GetIsCellOverlook(X, Y))
			{
				Candidates.Add({Position, M_Heights[GetIndex(X, Y)]});
			}
		}
	}
	// Nearest the base first: those are the crests that look down on it, not hills in the distance.
	const FVector2D Center = M_Center;
	Candidates.Sort([Center](const FCandidate& Left, const FCandidate& Right)
	{
		return FVector2D::DistSquared(Left.Position, Center) < FVector2D::DistSquared(Right.Position, Center);
	});
	M_Overlooks.Reset();
	for (const FCandidate& Candidate : Candidates)
	{
		const bool bTooClose = M_Overlooks.ContainsByPredicate([&Candidate](const FVector2D& Picked)
		{
			return FVector2D::Distance(Picked, Candidate.Position) < OverlookSpacing;
		});
		if (not bTooClose)
		{
			M_Overlooks.Add(Candidate.Position);
		}
	}
}

bool FSurvivalSovietSiteMap::GetIsFootprintBuildable(const FEnemyFootprint& Footprint) const
{
	using namespace SovietBaseSiteConstants;
	if (M_Heights.IsEmpty())
	{
		return true;
	}
	FVector2D AxisX;
	FVector2D AxisY;
	Footprint.GetAxes(AxisX, AxisY);
	double MinHeight = TNumericLimits<double>::Max();
	double MaxHeight = TNumericLimits<double>::Lowest();
	for (int32 SampleY = 0; SampleY < FootprintSamplesPerSide; ++SampleY)
	{
		for (int32 SampleX = 0; SampleX < FootprintSamplesPerSide; ++SampleX)
		{
			const double AlphaX = (2.0 * SampleX) / (FootprintSamplesPerSide - 1) - 1.0;
			const double AlphaY = (2.0 * SampleY) / (FootprintSamplesPerSide - 1) - 1.0;
			const FVector2D Position = Footprint.Center + AxisX * (AlphaX * Footprint.HalfExtents.X)
				+ AxisY * (AlphaY * Footprint.HalfExtents.Y);
			const FIntPoint Cell = GetCell(Position);
			if (not GetIsValidCell(Cell.X, Cell.Y))
			{
				return false;
			}
			const int32 Index = GetIndex(Cell.X, Cell.Y);
			if (M_Slopes[Index] > MaxBuildSlope)
			{
				return false;
			}
			MinHeight = FMath::Min<double>(MinHeight, M_Heights[Index]);
			MaxHeight = FMath::Max<double>(MaxHeight, M_Heights[Index]);
		}
	}
	return MaxHeight - MinHeight <= MaxFootprintStep;
}

bool FSurvivalSovietSiteMap::GetIsDrivable(const FVector2D& Position) const
{
	return M_Heights.IsEmpty() or GetIsCellDrivable(Position);
}

double FSurvivalSovietSiteMap::GetRelativeHeight(const FVector2D& Position) const
{
	const FIntPoint Cell = GetCell(Position);
	return GetIsValidCell(Cell.X, Cell.Y) ? M_Heights[GetIndex(Cell.X, Cell.Y)] : 0.0;
}

TArray<FVector2D> FSurvivalSovietSiteMap::FindDrivablePath(const FVector2D& From, const FVector2D& To) const
{
	using namespace SovietBaseSiteConstants;
	TArray<FVector2D> Path;
	const FIntPoint Start = GetCell(From);
	const FIntPoint Goal = GetCell(To);
	if (not GetIsValidCell(Start.X, Start.Y) or not GetIsValidCell(Goal.X, Goal.Y))
	{
		return Path;
	}

	// Breadth first over the 4-neighbourhood; the grid is small and the result is simplified afterwards.
	TArray<int32> CameFrom;
	CameFrom.Init(INDEX_NONE, M_Heights.Num());
	TArray<FIntPoint> Open;
	Open.Add(Start);
	CameFrom[GetIndex(Start.X, Start.Y)] = GetIndex(Start.X, Start.Y);
	bool bReached = false;
	for (int32 Head = 0; Head < Open.Num() and Head < MaxPathExpansions and not bReached; ++Head)
	{
		const FIntPoint Cell = Open[Head];
		const FIntPoint Neighbours[4] = {
			{Cell.X + 1, Cell.Y}, {Cell.X - 1, Cell.Y}, {Cell.X, Cell.Y + 1}, {Cell.X, Cell.Y - 1}
		};
		for (const FIntPoint& Neighbour : Neighbours)
		{
			if (not GetIsValidCell(Neighbour.X, Neighbour.Y))
			{
				continue;
			}
			const int32 Index = GetIndex(Neighbour.X, Neighbour.Y);
			if (CameFrom[Index] != INDEX_NONE or not M_Drivable[Index])
			{
				continue;
			}
			CameFrom[Index] = GetIndex(Cell.X, Cell.Y);
			Open.Add(Neighbour);
			bReached = bReached or Neighbour == Goal;
		}
	}
	if (not bReached)
	{
		return Path;
	}

	TArray<FVector2D> Cells;
	int32 Index = GetIndex(Goal.X, Goal.Y);
	while (CameFrom[Index] != Index)
	{
		Cells.Add(GetSamplePosition(Index));
		Index = CameFrom[Index];
	}
	Algo::Reverse(Cells);

	// Keep every fourth cell: enough to follow a ramp, few enough for a smooth spline.
	constexpr int32 KeepEveryCells = 4;
	Path.Add(From);
	for (int32 CellIndex = KeepEveryCells; CellIndex + 1 < Cells.Num(); CellIndex += KeepEveryCells)
	{
		Path.Add(Cells[CellIndex]);
	}
	Path.Add(To);
	return Path;
}
