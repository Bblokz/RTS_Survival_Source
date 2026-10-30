// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalDefensePointFinder.h"

#include "RTS_Survival/SurvivalMode/StartLocations/SurvivalStartPlacementHelpers.h"

namespace SurvivalDefensePointFinderConstants
{
	// Neighbours may be this much higher and the sample still counts as a top (plateaus are flat, not exact).
	constexpr float LocalMaximumTolerance = 5.f;
	constexpr int32 MaxRadialSamples = 256;
	constexpr int32 MaxAngularSamples = 360;
}

void FSurvivalDefensePointFinder::ResolveDefensePoints(
	const FSurvivalGroundProjector& Projector,
	const FVector& DeployCenter,
	const float BuildAreaRadius,
	const TArray<FVector>& DesignerPointsWorld,
	const ESurvivalDefensePointSource Source,
	const FSurvivalDefensePointSearchSettings& Search,
	TArray<FSurvivalDefensePoint>& OutPoints)
{
	OutPoints.Reset();
	float CenterHeight = static_cast<float>(DeployCenter.Z);
	Projector.TryGetHeight(FVector2D(DeployCenter), CenterHeight);

	if (Source != ESurvivalDefensePointSource::AutoOnly)
	{
		AddDesignerPoints(Projector, CenterHeight, DesignerPointsWorld, OutPoints);
	}
	if (Source != ESurvivalDefensePointSource::DesignerOnly and OutPoints.Num() < Search.MaxPoints)
	{
		TArray<FHillTopCandidate> Candidates;
		FindHillTopCandidates(Projector, DeployCenter, CenterHeight, BuildAreaRadius, Search, Candidates);
		SelectSpacedHillTops(Candidates, Search, OutPoints);
	}
	if (OutPoints.IsEmpty())
	{
		AddBuildEdgeFallback(Projector, DeployCenter, CenterHeight, BuildAreaRadius,
		                     Search.FallbackBuildEdgePointCount, OutPoints);
	}
}

void FSurvivalDefensePointFinder::AddDesignerPoints(
	const FSurvivalGroundProjector& Projector,
	const float CenterHeight,
	const TArray<FVector>& DesignerPointsWorld,
	TArray<FSurvivalDefensePoint>& OutPoints)
{
	for (const FVector& DesignerPoint : DesignerPointsWorld)
	{
		FSurvivalDefensePoint& Point = OutPoints.AddDefaulted_GetRef();
		Point.Origin = ESurvivalDefensePointOrigin::Designer;
		Point.Location = DesignerPoint;
		float GroundHeight = static_cast<float>(DesignerPoint.Z);
		// Snap to the landscape so a widget dragged slightly into the air or the ground still works.
		if (Projector.TryGetHeight(FVector2D(DesignerPoint), GroundHeight))
		{
			Point.Location.Z = GroundHeight;
		}
		Point.HeightAboveDeployCenter = static_cast<float>(Point.Location.Z) - CenterHeight;
	}
}

void FSurvivalDefensePointFinder::SampleHeightGrid(
	const FSurvivalGroundProjector& Projector,
	const FVector& DeployCenter,
	const float BuildAreaRadius,
	const FSurvivalDefensePointSearchSettings& Search,
	FPolarHeightGrid& OutGrid)
{
	using namespace SurvivalDefensePointFinderConstants;
	const float InnerRadius = BuildAreaRadius * Search.InnerRadiusFraction;
	const float OuterRadius = BuildAreaRadius + Search.SearchDistanceBeyondBuildEdge;
	OutGrid.RadialCount = FMath::Clamp(
		FMath::CeilToInt((OuterRadius - InnerRadius) / Search.RadialStep) + 1, 1, MaxRadialSamples);
	OutGrid.AngularCount = FMath::Clamp(
		FMath::RoundToInt(360.f / Search.AngularStepDegrees), 4, MaxAngularSamples);
	const int32 SampleCount = OutGrid.RadialCount * OutGrid.AngularCount;
	OutGrid.Heights.SetNumZeroed(SampleCount);
	OutGrid.bIsValid.SetNumZeroed(SampleCount);
	OutGrid.Positions.SetNumZeroed(SampleCount);

	const FVector2D Center(DeployCenter);
	for (int32 RadialIndex = 0; RadialIndex < OutGrid.RadialCount; ++RadialIndex)
	{
		const float Radius = InnerRadius + RadialIndex * Search.RadialStep;
		for (int32 AngularIndex = 0; AngularIndex < OutGrid.AngularCount; ++AngularIndex)
		{
			const float Angle = 2.f * PI * AngularIndex / OutGrid.AngularCount;
			const int32 SampleIndex = OutGrid.GetIndex(RadialIndex, AngularIndex);
			OutGrid.Positions[SampleIndex] = Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
			OutGrid.bIsValid[SampleIndex] = Projector.TryGetHeight(
				OutGrid.Positions[SampleIndex], OutGrid.Heights[SampleIndex]);
		}
	}
}

bool FSurvivalDefensePointFinder::GetIsLocalMaximum(const FPolarHeightGrid& Grid, const int32 SampleIndex)
{
	const float Height = Grid.Heights[SampleIndex];
	const int32 RadialIndex = SampleIndex / Grid.AngularCount;
	const int32 AngularIndex = SampleIndex % Grid.AngularCount;
	for (int32 RadialOffset = -1; RadialOffset <= 1; ++RadialOffset)
	{
		const int32 NeighbourRadial = RadialIndex + RadialOffset;
		if (NeighbourRadial < 0 or NeighbourRadial >= Grid.RadialCount)
		{
			continue;
		}
		for (int32 AngularOffset = -1; AngularOffset <= 1; ++AngularOffset)
		{
			const int32 NeighbourAngular = (AngularIndex + AngularOffset + Grid.AngularCount) % Grid.AngularCount;
			const int32 NeighbourIndex = Grid.GetIndex(NeighbourRadial, NeighbourAngular);
			if (Grid.bIsValid[NeighbourIndex] and Grid.Heights[NeighbourIndex] >
				Height + SurvivalDefensePointFinderConstants::LocalMaximumTolerance)
			{
				return false;
			}
		}
	}
	return true;
}

void FSurvivalDefensePointFinder::FindHillTopCandidates(
	const FSurvivalGroundProjector& Projector,
	const FVector& DeployCenter,
	const float CenterHeight,
	const float BuildAreaRadius,
	const FSurvivalDefensePointSearchSettings& Search,
	TArray<FHillTopCandidate>& OutCandidates)
{
	FPolarHeightGrid Grid;
	SampleHeightGrid(Projector, DeployCenter, BuildAreaRadius, Search, Grid);
	const FVector2D Center(DeployCenter);
	for (int32 SampleIndex = 0; SampleIndex < Grid.Heights.Num(); ++SampleIndex)
	{
		const float Height = Grid.Heights[SampleIndex];
		if (not Grid.bIsValid[SampleIndex] or Height - CenterHeight < Search.MinHeightAboveDeployCenter)
		{
			continue;
		}
		if (not GetIsLocalMaximum(Grid, SampleIndex)
			or not GetIsFlatTop(Projector, Grid.Positions[SampleIndex], Height, Search))
		{
			continue;
		}
		FHillTopCandidate& Candidate = OutCandidates.AddDefaulted_GetRef();
		Candidate.Location = FVector(Grid.Positions[SampleIndex], Height);
		Candidate.HeightAboveCenter = Height - CenterHeight;
		Candidate.DistanceToCenter = FVector2D::Distance(Grid.Positions[SampleIndex], Center);
	}
}

bool FSurvivalDefensePointFinder::GetIsFlatTop(
	const FSurvivalGroundProjector& Projector,
	const FVector2D& Position,
	const float Height,
	const FSurvivalDefensePointSearchSettings& Search)
{
	const float MaxHeightDifference =
		FMath::Tan(FMath::DegreesToRadians(Search.MaxTopSlopeDegrees)) * Search.SlopeSampleDistance;
	const FVector2D Offsets[] = {
		FVector2D(Search.SlopeSampleDistance, 0.f), FVector2D(-Search.SlopeSampleDistance, 0.f),
		FVector2D(0.f, Search.SlopeSampleDistance), FVector2D(0.f, -Search.SlopeSampleDistance)
	};
	for (const FVector2D& Offset : Offsets)
	{
		float NeighbourHeight = 0.f;
		if (not Projector.TryGetHeight(Position + Offset, NeighbourHeight))
		{
			return false;
		}
		if (FMath::Abs(NeighbourHeight - Height) > MaxHeightDifference)
		{
			return false;
		}
	}
	return true;
}

void FSurvivalDefensePointFinder::SelectSpacedHillTops(
	const TArray<FHillTopCandidate>& Candidates,
	const FSurvivalDefensePointSearchSettings& Search,
	TArray<FSurvivalDefensePoint>& InOutPoints)
{
	TArray<FHillTopCandidate> Sorted = Candidates;
	// Highest first; on a plateau (equal heights) the edge closest to the base wins: it overlooks the build area.
	Sorted.Sort([](const FHillTopCandidate& A, const FHillTopCandidate& B)
	{
		if (not FMath::IsNearlyEqual(A.HeightAboveCenter, B.HeightAboveCenter,
		                             SurvivalDefensePointFinderConstants::LocalMaximumTolerance))
		{
			return A.HeightAboveCenter > B.HeightAboveCenter;
		}
		return A.DistanceToCenter < B.DistanceToCenter;
	});
	for (const FHillTopCandidate& Candidate : Sorted)
	{
		if (InOutPoints.Num() >= Search.MaxPoints)
		{
			return;
		}
		if (not GetIsFarFromPoints(Candidate.Location, InOutPoints, Search.MinSpacing))
		{
			continue;
		}
		FSurvivalDefensePoint& Point = InOutPoints.AddDefaulted_GetRef();
		Point.Location = Candidate.Location;
		Point.HeightAboveDeployCenter = Candidate.HeightAboveCenter;
		Point.Origin = ESurvivalDefensePointOrigin::HillTop;
	}
}

void FSurvivalDefensePointFinder::AddBuildEdgeFallback(
	const FSurvivalGroundProjector& Projector,
	const FVector& DeployCenter,
	const float CenterHeight,
	const float BuildAreaRadius,
	const int32 PointCount,
	TArray<FSurvivalDefensePoint>& OutPoints)
{
	for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
	{
		const float Angle = 2.f * PI * PointIndex / PointCount;
		const FVector2D Position = FVector2D(DeployCenter) +
			FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * BuildAreaRadius;
		float Height = CenterHeight;
		if (not Projector.TryGetHeight(Position, Height))
		{
			continue;
		}
		FSurvivalDefensePoint& Point = OutPoints.AddDefaulted_GetRef();
		Point.Location = FVector(Position, Height);
		Point.HeightAboveDeployCenter = Height - CenterHeight;
		Point.Origin = ESurvivalDefensePointOrigin::BuildEdgeFallback;
	}
}

bool FSurvivalDefensePointFinder::GetIsFarFromPoints(
	const FVector& Location,
	const TArray<FSurvivalDefensePoint>& Points,
	const float MinSpacing)
{
	for (const FSurvivalDefensePoint& Point : Points)
	{
		if (FVector::DistSquared2D(Point.Location, Location) < FMath::Square(MinSpacing))
		{
			return false;
		}
	}
	return true;
}
