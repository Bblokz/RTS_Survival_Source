// Copyright (C) Bas Blokzijl - All rights reserved.

#include "ConvoyRoute.h"

#include "Engine/World.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "NavigationSystem.h"

namespace ConvoyRouteConstants
{
	// Heavy vehicle agent: routes every convoy member can drive.
	constexpr float VehicleAgentRadius = 200.f;
	constexpr float VehicleAgentHeight = 300.f;
	constexpr double NavProjectExtentXY = 900.0;
	constexpr double NavProjectExtentZ = 2000.0;
	constexpr int32 MaxPathHopsPerLeg = 6;
	constexpr double LegArriveTolerance = 400.0;
	constexpr float MinResampleSpacing = 200.f;
	constexpr float DegenerateSegmentLength = 1.f;
}

// ---------------------------------------------------------------------------
// FConvoyRoutePolyline
// ---------------------------------------------------------------------------

void FConvoyRoutePolyline::Build(const TArray<FVector>& InPoints)
{
	M_Points.Reset();
	M_CumulativeDistances.Reset();
	for (const FVector& Point : InPoints)
	{
		if (not M_Points.IsEmpty() and FVector::Dist2D(M_Points.Last(), Point) < ConvoyRouteConstants::DegenerateSegmentLength)
		{
			continue;
		}
		const float Previous = M_CumulativeDistances.IsEmpty() ? 0.f : M_CumulativeDistances.Last();
		const float Step = M_Points.IsEmpty() ? 0.f : FVector::Dist2D(M_Points.Last(), Point);
		M_Points.Add(Point);
		M_CumulativeDistances.Add(Previous + Step);
	}
}

int32 FConvoyRoutePolyline::FindSegmentIndex(const float Distance) const
{
	// Segment i spans points i and i + 1.
	const int32 LastSegment = M_Points.Num() - 2;
	if (LastSegment < 0)
	{
		return INDEX_NONE;
	}
	int32 Low = 0;
	int32 High = LastSegment;
	while (Low < High)
	{
		const int32 Middle = (Low + High + 1) / 2;
		if (M_CumulativeDistances[Middle] <= Distance)
		{
			Low = Middle;
		}
		else
		{
			High = Middle - 1;
		}
	}
	return Low;
}

FVector FConvoyRoutePolyline::GetPointAtDistance(const float Distance) const
{
	if (not GetIsValid())
	{
		return M_Points.IsEmpty() ? FVector::ZeroVector : M_Points[0];
	}
	const float Clamped = FMath::Clamp(Distance, 0.f, GetLength());
	const int32 Segment = FindSegmentIndex(Clamped);
	const float SegmentStart = M_CumulativeDistances[Segment];
	const float SegmentLength = M_CumulativeDistances[Segment + 1] - SegmentStart;
	const float Alpha = SegmentLength > KINDA_SMALL_NUMBER ? (Clamped - SegmentStart) / SegmentLength : 0.f;
	return FMath::Lerp(M_Points[Segment], M_Points[Segment + 1], Alpha);
}

FVector FConvoyRoutePolyline::GetDirectionAtDistance(const float Distance) const
{
	if (not GetIsValid())
	{
		return FVector::ForwardVector;
	}
	const int32 Segment = FindSegmentIndex(FMath::Clamp(Distance, 0.f, GetLength()));
	const FVector Direction = (M_Points[Segment + 1] - M_Points[Segment]).GetSafeNormal2D();
	return Direction.IsNearlyZero() ? FVector::ForwardVector : Direction;
}

FVector FConvoyRoutePolyline::GetOffsetPoint(const float Distance, const float LateralOffset) const
{
	const FVector Direction = GetDirectionAtDistance(Distance);
	// UE is left-handed (+X forward, +Y right), so the right of heading (X, Y) is (-Y, X).
	const FVector Right(-Direction.Y, Direction.X, 0.f);
	return GetPointAtDistance(Distance) + Right * LateralOffset;
}

float FConvoyRoutePolyline::ProjectLocation(const FVector& Location, const float HintDistance, const float SearchWindow,
                                            float& OutLateralOffset) const
{
	OutLateralOffset = 0.f;
	if (not GetIsValid())
	{
		return 0.f;
	}
	const bool bUseWindow = HintDistance >= 0.f;
	const int32 FirstSegment = bUseWindow ? FindSegmentIndex(FMath::Max(0.f, HintDistance - SearchWindow)) : 0;
	const int32 LastSegment = bUseWindow
		                          ? FindSegmentIndex(FMath::Min(GetLength(), HintDistance + SearchWindow))
		                          : M_Points.Num() - 2;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	float BestRouteDistance = 0.f;
	for (int32 Segment = FirstSegment; Segment <= LastSegment; ++Segment)
	{
		const FVector A(M_Points[Segment].X, M_Points[Segment].Y, 0.f);
		const FVector B(M_Points[Segment + 1].X, M_Points[Segment + 1].Y, 0.f);
		const FVector P(Location.X, Location.Y, 0.f);
		const FVector Closest = FMath::ClosestPointOnSegment(P, A, B);
		const float DistanceSquared = FVector::DistSquared(P, Closest);
		if (DistanceSquared >= BestDistanceSquared)
		{
			continue;
		}
		BestDistanceSquared = DistanceSquared;
		BestRouteDistance = M_CumulativeDistances[Segment] + FVector::Dist(A, Closest);
		const FVector Direction = (B - A).GetSafeNormal();
		const FVector Right(-Direction.Y, Direction.X, 0.f);
		OutLateralOffset = FVector::DotProduct(P - Closest, Right);
	}
	return BestRouteDistance;
}

// ---------------------------------------------------------------------------
// UConvoyRouteLibrary
// ---------------------------------------------------------------------------

namespace
{
	UNavigationSystemV1* ConvoyRoute_GetNavigationSystem(const UWorld* World)
	{
		return IsValid(World) ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(const_cast<UWorld*>(World)) : nullptr;
	}

	FNavAgentProperties ConvoyRoute_GetVehicleAgent()
	{
		FNavAgentProperties Agent;
		Agent.AgentRadius = ConvoyRouteConstants::VehicleAgentRadius;
		Agent.AgentHeight = ConvoyRouteConstants::VehicleAgentHeight;
		return Agent;
	}

	void ConvoyRoute_Resample(const TArray<FVector>& Dense, const float Spacing, TArray<FVector>& OutPoints)
	{
		FConvoyRoutePolyline Polyline;
		Polyline.Build(Dense);
		OutPoints.Reset();
		if (not Polyline.GetIsValid())
		{
			OutPoints = Dense;
			return;
		}
		const float Length = Polyline.GetLength();
		const int32 Steps = FMath::Max(1, FMath::CeilToInt(Length / Spacing));
		for (int32 Step = 0; Step <= Steps; ++Step)
		{
			OutPoints.Add(Polyline.GetPointAtDistance(Length * Step / Steps));
		}
	}
}

bool UConvoyRouteLibrary::ProjectToVehicleNavmesh(const UWorld* World, const FVector& Location, FVector& OutProjected)
{
	using namespace ConvoyRouteConstants;
	UNavigationSystemV1* NavigationSystem = ConvoyRoute_GetNavigationSystem(World);
	if (NavigationSystem == nullptr)
	{
		return false;
	}
	const ANavigationData* NavData = NavigationSystem->GetNavDataForProps(ConvoyRoute_GetVehicleAgent());
	FNavLocation Projected;
	const FVector Extent(NavProjectExtentXY, NavProjectExtentXY, NavProjectExtentZ);
	if (not NavigationSystem->ProjectPointToNavigation(Location, Projected, Extent, NavData))
	{
		return false;
	}
	OutProjected = Projected.Location;
	return true;
}

bool UConvoyRouteLibrary::FindChainedVehiclePath(const UWorld* World, const FVector& Start, const FVector& Goal,
                                                 TArray<FVector>& OutPoints)
{
	using namespace ConvoyRouteConstants;
	OutPoints.Reset();
	UNavigationSystemV1* NavigationSystem = ConvoyRoute_GetNavigationSystem(World);
	const FNavAgentProperties Agent = ConvoyRoute_GetVehicleAgent();
	const ANavigationData* NavData = NavigationSystem != nullptr ? NavigationSystem->GetNavDataForProps(Agent) : nullptr;
	if (NavData == nullptr)
	{
		return false;
	}
	FVector Current = Start;
	OutPoints.Add(Start);
	for (int32 Hop = 0; Hop < MaxPathHopsPerLeg; ++Hop)
	{
		const FPathFindingQuery Query(nullptr, *NavData, Current, Goal,
		                              UNavigationQueryFilter::GetQueryFilter(*NavData, nullptr, nullptr));
		const FPathFindingResult Path = NavigationSystem->FindPathSync(Agent, Query);
		if (not Path.IsSuccessful() or not Path.Path.IsValid() or Path.Path->GetPathPoints().IsEmpty())
		{
			return false;
		}
		for (const FNavPathPoint& PathPoint : Path.Path->GetPathPoints())
		{
			OutPoints.Add(PathPoint.Location);
		}
		const FVector End = Path.Path->GetPathPoints().Last().Location;
		if (FVector::Dist2D(End, Goal) <= LegArriveTolerance)
		{
			return true;
		}
		if (FVector::Dist2D(End, Current) < DegenerateSegmentLength)
		{
			return false;
		}
		Current = End;
	}
	return false;
}

FConvoyRoute UConvoyRouteLibrary::BuildNavigableRoute(const UObject* WorldContextObject,
                                                      const TArray<FVector>& ControlPoints,
                                                      const float ResampleSpacing, const FName RouteId,
                                                      bool& bOutReachedEnd)
{
	FConvoyRoute Route;
	Route.RouteId = RouteId;
	bOutReachedEnd = false;
	const UWorld* World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	if (World == nullptr or ControlPoints.Num() < 2)
	{
		return Route;
	}
	TArray<FVector> Projected;
	for (const FVector& ControlPoint : ControlPoints)
	{
		FVector OnNav;
		if (ProjectToVehicleNavmesh(World, ControlPoint, OnNav))
		{
			Projected.Add(OnNav);
		}
	}
	if (Projected.Num() < 2)
	{
		return Route;
	}
	TArray<FVector> Dense;
	bOutReachedEnd = true;
	for (int32 Leg = 0; Leg + 1 < Projected.Num(); ++Leg)
	{
		TArray<FVector> LegPoints;
		const bool bReached = FindChainedVehiclePath(World, Projected[Leg], Projected[Leg + 1], LegPoints);
		Dense.Append(LegPoints);
		if (not bReached)
		{
			bOutReachedEnd = false;
			break;
		}
	}
	ConvoyRoute_Resample(Dense, FMath::Max(ResampleSpacing, ConvoyRouteConstants::MinResampleSpacing), Route.Points);
	return Route;
}

float UConvoyRouteLibrary::GetRouteLength(const FConvoyRoute& Route)
{
	FConvoyRoutePolyline Polyline;
	Polyline.Build(Route.Points);
	return Polyline.GetLength();
}
