// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionBase.h"

#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/DeveloperSettings/WorldCampaignSettings.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionInfluenceComponent.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionMovementComponent.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/AnchorPoint/AnchorPoint.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Boundary/WorldSplineBoundary.h"

namespace
{
	constexpr float BoundarySampleSpacing = 250.f;
	constexpr float MinPathPointDistanceSquared = 1.f;
	constexpr float MinDirectionSizeSquared = 1.f;
	constexpr float AdvanceTurnTargetNearlyZeroTolerance = 1.f;
	constexpr float AdvanceTurnTargetReachedDistanceSquared = 1.f;
	constexpr float AnchorDetourClearanceMin = 5.f;
	constexpr float AnchorDetourClearanceScale = 0.1f;
	constexpr int32 AnchorBoundaryStandoffSampleCount = 96;
	constexpr int32 AnchorProjectionMaxPassCount = 8;
	constexpr float BoundarySegmentSampleSpacing = 100.f;
	constexpr float BoundaryIntersectionTolerance = 0.001f;
	constexpr float BoundaryOffsetRadiusRetryScale = 0.5f;
	constexpr float BoundaryPaddingProjectionScale = 0.25f;
	constexpr int32 BoundarySegmentMinSampleCount = 8;
	constexpr int32 BoundarySegmentMaxSampleCount = 256;
	constexpr int32 BoundaryOffsetDirectionSampleCount = 32;
	constexpr int32 BoundaryOffsetRadiusRetryCount = 4;
	constexpr int32 BoundaryCrossingRefinementCount = 12;
	constexpr int32 MinimumPathRepairAttemptCount = 32;
	constexpr float DebugPathDrawDurationSeconds = 5.f;
	constexpr float DebugPathLineThickness = 8.f;
	constexpr float DebugUnrealPathLineThickness = 2.f;
	constexpr float DebugAdjustedPathLineThickness = 1.f;
	constexpr float DebugPathZOffset = 25.f;
	constexpr uint8 DebugPathDepthPriority = 0;

	struct FAnchorDetourCandidate
	{
		TArray<FVector> Points;
		float Length = TNumericLimits<float>::Max();
		bool bRespectsConstraints = false;
	};

	struct FBoundaryProjection
	{
		FVector2D Point = FVector2D::ZeroVector;
		float DistanceSquared = TNumericLimits<float>::Max();
		int32 SegmentIndex = INDEX_NONE;
	};

	struct FBoundaryIntersection
	{
		FVector2D Point = FVector2D::ZeroVector;
		float SegmentAlpha = 0.f;
		int32 BoundarySegmentIndex = INDEX_NONE;
	};

	struct FBoundaryDetourCandidate
	{
		TArray<FVector> Points;
		float Length = TNumericLimits<float>::Max();
		bool bIsValid = false;
	};

	FVector2D GetXY(const FVector& Location)
	{
		return FVector2D(Location.X, Location.Y);
	}

	void DrawDebugPathPointsWithOffset(const UWorld* World,
	                                   const TArray<FVector>& PathPoints,
	                                   const FColor& Color,
	                                   const float LineThickness)
	{
		if (not IsValid(World) || PathPoints.Num() < 2)
		{
			return;
		}

		for (int32 PathPointIndex = 0; PathPointIndex < PathPoints.Num() - 1; PathPointIndex++)
		{
			const FVector DebugSegmentStart = PathPoints[PathPointIndex] + FVector::UpVector * DebugPathZOffset;
			const FVector DebugSegmentEnd = PathPoints[PathPointIndex + 1] + FVector::UpVector * DebugPathZOffset;
			DrawDebugLine(
				World,
				DebugSegmentStart,
				DebugSegmentEnd,
				Color,
				false,
				DebugPathDrawDurationSeconds,
				DebugPathDepthPriority,
				LineThickness);
		}
	}

	void DebugDrawUnrealDivisionPath(const UWorld* World, const TArray<FVector>& PathPoints)
	{
		DrawDebugPathPointsWithOffset(World, PathPoints, FColor::Red, DebugUnrealPathLineThickness);
	}

	void DebugDrawAdjustedDivisionPath(const UWorld* World, const TArray<FVector>& PathPoints)
	{
		DrawDebugPathPointsWithOffset(World, PathPoints, FColor::Blue, DebugAdjustedPathLineThickness);
	}

	FVector BuildVectorFromXY(const FVector2D& Location, const float Z)
	{
		return FVector(Location.X, Location.Y, Z);
	}

	FVector BuildVectorFromAngle(const FVector2D& Center, const float AngleRadians, const float Radius, const float Z)
	{
		return FVector(
			Center.X + FMath::Cos(AngleRadians) * Radius,
			Center.Y + FMath::Sin(AngleRadians) * Radius,
			Z);
	}

	float GetCross2D(const FVector2D& Left, const FVector2D& Right)
	{
		return Left.X * Right.Y - Left.Y * Right.X;
	}

	float GetAnchorDetourRadius(const float AvoidanceRadius)
	{
		if (AvoidanceRadius <= 0.f)
		{
			return 0.f;
		}

		return AvoidanceRadius + FMath::Max(AnchorDetourClearanceMin, AvoidanceRadius * AnchorDetourClearanceScale);
	}

	FVector2D GetSafeAnchorDirection(const FVector2D& Point,
	                                 const FVector2D& Anchor,
	                                 const FVector2D& FallbackDirection)
	{
		FVector2D Direction = Point - Anchor;
		if (Direction.SizeSquared() > MinDirectionSizeSquared)
		{
			return Direction.GetSafeNormal();
		}

		if (FallbackDirection.SizeSquared() > MinDirectionSizeSquared)
		{
			return FallbackDirection.GetSafeNormal();
		}

		return FVector2D(1.f, 0.f);
	}

	FVector2D GetPolygonCentroid(const TArray<FVector2D>& Polygon)
	{
		FVector2D Centroid = FVector2D::ZeroVector;
		if (Polygon.Num() == 0)
		{
			return Centroid;
		}

		for (const FVector2D& Point : Polygon)
		{
			Centroid += Point;
		}

		return Centroid / static_cast<float>(Polygon.Num());
	}

	bool GetIsPointInsidePolygon(const FVector2D& Point, const TArray<FVector2D>& Polygon)
	{
		if (Polygon.Num() < 3)
		{
			return true;
		}

		bool bIsInside = false;
		int32 PreviousIndex = Polygon.Num() - 1;
		for (int32 CurrentIndex = 0; CurrentIndex < Polygon.Num(); CurrentIndex++)
		{
			const FVector2D& CurrentPoint = Polygon[CurrentIndex];
			const FVector2D& PreviousPoint = Polygon[PreviousIndex];
			const bool bDoesEdgeCrossY = (CurrentPoint.Y > Point.Y) != (PreviousPoint.Y > Point.Y);
			if (bDoesEdgeCrossY)
			{
				const float EdgeX = (PreviousPoint.X - CurrentPoint.X)
					* (Point.Y - CurrentPoint.Y)
					/ (PreviousPoint.Y - CurrentPoint.Y)
					+ CurrentPoint.X;
				if (Point.X < EdgeX)
				{
					bIsInside = not bIsInside;
				}
			}

			PreviousIndex = CurrentIndex;
		}

		return bIsInside;
	}

	FVector2D GetClosestPointOnSegment(const FVector2D& Point,
	                                   const FVector2D& SegmentStart,
	                                   const FVector2D& SegmentEnd)
	{
		const FVector2D Segment = SegmentEnd - SegmentStart;
		const float SegmentSizeSquared = Segment.SizeSquared();
		if (SegmentSizeSquared <= MinDirectionSizeSquared)
		{
			return SegmentStart;
		}

		const float Alpha = FVector2D::DotProduct(Point - SegmentStart, Segment) / SegmentSizeSquared;
		return SegmentStart + Segment * FMath::Clamp(Alpha, 0.f, 1.f);
	}

	float GetPolygonSignedArea(const TArray<FVector2D>& Polygon)
	{
		float SignedArea = 0.f;
		for (int32 PointIndex = 0; PointIndex < Polygon.Num(); PointIndex++)
		{
			const FVector2D& CurrentPoint = Polygon[PointIndex];
			const FVector2D& NextPoint = Polygon[(PointIndex + 1) % Polygon.Num()];
			SignedArea += CurrentPoint.X * NextPoint.Y - NextPoint.X * CurrentPoint.Y;
		}

		return SignedArea * 0.5f;
	}

	FVector2D GetBoundaryEdgeInwardNormal(const FVector2D& SegmentStart,
	                                      const FVector2D& SegmentEnd,
	                                      const bool bIsCounterClockwise)
	{
		const FVector2D Edge = SegmentEnd - SegmentStart;
		if (Edge.SizeSquared() <= MinDirectionSizeSquared)
		{
			return FVector2D::ZeroVector;
		}

		const FVector2D EdgeDirection = Edge.GetSafeNormal();
		return bIsCounterClockwise
			       ? FVector2D(-EdgeDirection.Y, EdgeDirection.X)
			       : FVector2D(EdgeDirection.Y, -EdgeDirection.X);
	}

	FBoundaryProjection FindClosestBoundaryProjection(const FVector2D& Point,
	                                                  const TArray<FVector2D>& BoundaryPolygon)
	{
		FBoundaryProjection ClosestProjection;
		for (int32 SegmentIndex = 0; SegmentIndex < BoundaryPolygon.Num(); SegmentIndex++)
		{
			const FVector2D& SegmentStart = BoundaryPolygon[SegmentIndex];
			const FVector2D& SegmentEnd = BoundaryPolygon[(SegmentIndex + 1) % BoundaryPolygon.Num()];
			const FVector2D CandidatePoint = GetClosestPointOnSegment(Point, SegmentStart, SegmentEnd);
			const float CandidateDistanceSquared = FVector2D::DistSquared(Point, CandidatePoint);
			if (CandidateDistanceSquared >= ClosestProjection.DistanceSquared)
			{
				continue;
			}

			ClosestProjection.Point = CandidatePoint;
			ClosestProjection.DistanceSquared = CandidateDistanceSquared;
			ClosestProjection.SegmentIndex = SegmentIndex;
		}

		return ClosestProjection;
	}

	bool TryBuildInsideOffsetPoint(const FVector2D& BoundaryPoint,
	                               const FVector2D& DirectionInside,
	                               const TArray<FVector2D>& BoundaryPolygon,
	                               const float Padding,
	                               const float Z,
	                               FVector& OutPoint)
	{
		if (DirectionInside.SizeSquared() <= MinDirectionSizeSquared)
		{
			return false;
		}

		const float SafePadding = FMath::Max(0.f, Padding);
		const FVector2D CandidatePoint = BoundaryPoint + DirectionInside.GetSafeNormal() * SafePadding;
		if (BoundaryPolygon.Num() >= 3 && not GetIsPointInsidePolygon(CandidatePoint, BoundaryPolygon))
		{
			return false;
		}

		OutPoint = BuildVectorFromXY(CandidatePoint, Z);
		return true;
	}

	bool TryBuildSampledInsideOffsetPoint(const FVector2D& BoundaryPoint,
	                                      const FVector2D& PreferredDirectionInside,
	                                      const TArray<FVector2D>& BoundaryPolygon,
	                                      const float Padding,
	                                      const float Z,
	                                      FVector& OutPoint)
	{
		if (PreferredDirectionInside.SizeSquared() <= MinDirectionSizeSquared || Padding <= 0.f)
		{
			return false;
		}

		const float PreferredAngle = FMath::Atan2(PreferredDirectionInside.Y, PreferredDirectionInside.X);
		float CurrentRadius = Padding;
		for (int32 RadiusAttemptIndex = 0; RadiusAttemptIndex < BoundaryOffsetRadiusRetryCount; RadiusAttemptIndex++)
		{
			float BestAngularDelta = TNumericLimits<float>::Max();
			FVector BestPoint = FVector::ZeroVector;
			for (int32 DirectionSampleIndex = 0;
			     DirectionSampleIndex < BoundaryOffsetDirectionSampleCount;
			     DirectionSampleIndex++)
			{
				const float CandidateAngle = PreferredAngle
					+ 2.f * UE_PI * static_cast<float>(DirectionSampleIndex)
					/ static_cast<float>(BoundaryOffsetDirectionSampleCount);
				const FVector2D Direction(FMath::Cos(CandidateAngle), FMath::Sin(CandidateAngle));
				const FVector2D CandidatePoint = BoundaryPoint + Direction * CurrentRadius;
				if (not GetIsPointInsidePolygon(CandidatePoint, BoundaryPolygon))
				{
					continue;
				}

				const float AngularDelta = FMath::Abs(FMath::FindDeltaAngleRadians(PreferredAngle, CandidateAngle));
				if (AngularDelta >= BestAngularDelta)
				{
					continue;
				}

				BestAngularDelta = AngularDelta;
				BestPoint = BuildVectorFromXY(CandidatePoint, Z);
			}

			if (BestAngularDelta < TNumericLimits<float>::Max())
			{
				OutPoint = BestPoint;
				return true;
			}

			CurrentRadius *= BoundaryOffsetRadiusRetryScale;
		}

		return false;
	}

	FVector BuildBoundaryOffsetPoint(const FVector2D& BoundaryPoint,
	                                 const int32 BoundarySegmentIndex,
	                                 const TArray<FVector2D>& BoundaryPolygon,
	                                 const float Padding,
	                                 const float Z)
	{
		if (not BoundaryPolygon.IsValidIndex(BoundarySegmentIndex))
		{
			return BuildVectorFromXY(BoundaryPoint, Z);
		}

		const bool bIsCounterClockwise = GetPolygonSignedArea(BoundaryPolygon) > 0.f;
		const FVector2D& SegmentStart = BoundaryPolygon[BoundarySegmentIndex];
		const FVector2D& SegmentEnd = BoundaryPolygon[(BoundarySegmentIndex + 1) % BoundaryPolygon.Num()];
		const FVector2D DirectionInside =
			GetBoundaryEdgeInwardNormal(SegmentStart, SegmentEnd, bIsCounterClockwise);

		FVector OffsetPoint = FVector::ZeroVector;
		if (TryBuildInsideOffsetPoint(BoundaryPoint, DirectionInside, BoundaryPolygon, Padding, Z, OffsetPoint))
		{
			return OffsetPoint;
		}

		const FVector2D CentroidDirection = GetPolygonCentroid(BoundaryPolygon) - BoundaryPoint;
		if (TryBuildInsideOffsetPoint(BoundaryPoint, CentroidDirection, BoundaryPolygon, Padding, Z, OffsetPoint))
		{
			return OffsetPoint;
		}

		if (TryBuildSampledInsideOffsetPoint(
			BoundaryPoint,
			DirectionInside,
			BoundaryPolygon,
			Padding,
			Z,
			OffsetPoint))
		{
			return OffsetPoint;
		}

		return BuildVectorFromXY(BoundaryPoint, Z);
	}

	FVector BuildBoundaryVertexOffsetPoint(const int32 BoundaryVertexIndex,
	                                       const TArray<FVector2D>& BoundaryPolygon,
	                                       const float Padding,
	                                       const float Z)
	{
		if (not BoundaryPolygon.IsValidIndex(BoundaryVertexIndex))
		{
			return FVector::ZeroVector;
		}

		const bool bIsCounterClockwise = GetPolygonSignedArea(BoundaryPolygon) > 0.f;
		const int32 PreviousSegmentIndex =
			(BoundaryVertexIndex - 1 + BoundaryPolygon.Num()) % BoundaryPolygon.Num();
		const int32 NextSegmentIndex = BoundaryVertexIndex;
		const FVector2D PreviousNormal = GetBoundaryEdgeInwardNormal(
			BoundaryPolygon[PreviousSegmentIndex],
			BoundaryPolygon[BoundaryVertexIndex],
			bIsCounterClockwise);
		const FVector2D NextNormal = GetBoundaryEdgeInwardNormal(
			BoundaryPolygon[BoundaryVertexIndex],
			BoundaryPolygon[(BoundaryVertexIndex + 1) % BoundaryPolygon.Num()],
			bIsCounterClockwise);
		const FVector2D BoundaryPoint = BoundaryPolygon[BoundaryVertexIndex];

		FVector OffsetPoint = FVector::ZeroVector;
		if (TryBuildInsideOffsetPoint(
			BoundaryPoint,
			PreviousNormal + NextNormal,
			BoundaryPolygon,
			Padding,
			Z,
			OffsetPoint))
		{
			return OffsetPoint;
		}

		if (TryBuildInsideOffsetPoint(BoundaryPoint, PreviousNormal, BoundaryPolygon, Padding, Z, OffsetPoint))
		{
			return OffsetPoint;
		}

		return BuildBoundaryOffsetPoint(BoundaryPoint, NextSegmentIndex, BoundaryPolygon, Padding, Z);
	}

	FVector ProjectPointInsideBoundary(const FVector& Point,
	                                   const TArray<FVector2D>& BoundaryPolygon,
	                                   const float Padding)
	{
		if (BoundaryPolygon.Num() < 3)
		{
			return Point;
		}

		const FBoundaryProjection ClosestProjection = FindClosestBoundaryProjection(GetXY(Point), BoundaryPolygon);
		const bool bIsInsideBoundary = GetIsPointInsidePolygon(GetXY(Point), BoundaryPolygon);
		const float RequiredPadding = Padding * BoundaryPaddingProjectionScale;
		if (bIsInsideBoundary
			&& (RequiredPadding <= 0.f || ClosestProjection.DistanceSquared >= FMath::Square(RequiredPadding)))
		{
			return Point;
		}

		return BuildBoundaryOffsetPoint(
			ClosestProjection.Point,
			ClosestProjection.SegmentIndex,
			BoundaryPolygon,
			Padding,
			Point.Z);
	}

	bool TryFindFirstOutsidePointOnSegment(const FVector& SegmentStart,
	                                       const FVector& SegmentEnd,
	                                       const TArray<FVector2D>& BoundaryPolygon,
	                                       FVector& OutOutsidePoint)
	{
		if (BoundaryPolygon.Num() < 3)
		{
			return false;
		}

		const float SegmentDistance = FVector2D::Distance(GetXY(SegmentStart), GetXY(SegmentEnd));
		const int32 SampleCount = FMath::Clamp(
			FMath::CeilToInt(SegmentDistance / BoundarySegmentSampleSpacing),
			BoundarySegmentMinSampleCount,
			BoundarySegmentMaxSampleCount);
		for (int32 SampleIndex = 1; SampleIndex <= SampleCount; SampleIndex++)
		{
			const float Alpha = static_cast<float>(SampleIndex) / static_cast<float>(SampleCount);
			const FVector Candidate = FMath::Lerp(SegmentStart, SegmentEnd, Alpha);
			if (not GetIsPointInsidePolygon(GetXY(Candidate), BoundaryPolygon))
			{
				OutOutsidePoint = Candidate;
				return true;
			}
		}

		return false;
	}

	bool GetDoesSegmentIntersectCircle(const FVector& SegmentStart,
	                                   const FVector& SegmentEnd,
	                                   const FVector& CircleCenter,
	                                   const float CircleRadius)
	{
		if (CircleRadius <= 0.f)
		{
			return false;
		}

		const FVector2D SegmentStartXY = GetXY(SegmentStart);
		const FVector2D SegmentEndXY = GetXY(SegmentEnd);
		const FVector2D CircleCenterXY = GetXY(CircleCenter);
		const FVector2D ClosestPoint = GetClosestPointOnSegment(CircleCenterXY, SegmentStartXY, SegmentEndXY);
		return FVector2D::DistSquared(ClosestPoint, CircleCenterXY) < FMath::Square(CircleRadius);
	}

	FVector ProjectPointAwayFromAnchors(const FVector& Point,
	                                    const TArray<AAnchorPoint*>& AnchorPoints,
	                                    const float AvoidanceRadius)
	{
		if (AvoidanceRadius <= 0.f)
		{
			return Point;
		}

		FVector ProjectedPoint = Point;
		for (const AAnchorPoint* AnchorPoint : AnchorPoints)
		{
			if (not IsValid(AnchorPoint))
			{
				continue;
			}

			const FVector2D AnchorXY = GetXY(AnchorPoint->GetActorLocation());
			const FVector2D PointXY = GetXY(ProjectedPoint);
			const float DistanceSquared = FVector2D::DistSquared(PointXY, AnchorXY);
			if (DistanceSquared >= FMath::Square(AvoidanceRadius))
			{
				continue;
			}

			FVector2D Direction = PointXY - AnchorXY;
			if (Direction.SizeSquared() <= MinDirectionSizeSquared)
			{
				Direction = FVector2D(1.f, 0.f);
			}

			ProjectedPoint = BuildVectorFromXY(AnchorXY + Direction.GetSafeNormal() * AvoidanceRadius, Point.Z);
		}

		return ProjectedPoint;
	}

	FVector FindBoundarySafeAnchorStandoffPoint(const FVector& Point,
	                                           const FVector& AnchorLocation,
	                                           const float AvoidanceRadius,
	                                           const TArray<FVector2D>& BoundaryPolygon)
	{
		const FVector2D AnchorXY = GetXY(AnchorLocation);
		const FVector2D PointXY = GetXY(Point);
		const FVector2D PreferredDirection = GetSafeAnchorDirection(PointXY, AnchorXY, FVector2D(1.f, 0.f));
		const float PreferredAngle = FMath::Atan2(PreferredDirection.Y, PreferredDirection.X);

		FVector BestCandidate = BuildVectorFromXY(AnchorXY + PreferredDirection * AvoidanceRadius, Point.Z);
		float BestAngularDelta = TNumericLimits<float>::Max();
		for (int32 SampleIndex = 0; SampleIndex < AnchorBoundaryStandoffSampleCount; SampleIndex++)
		{
			const float CandidateAngle = PreferredAngle
				+ 2.f * UE_PI * static_cast<float>(SampleIndex)
				/ static_cast<float>(AnchorBoundaryStandoffSampleCount);
			const FVector Candidate = BuildVectorFromAngle(AnchorXY, CandidateAngle, AvoidanceRadius, Point.Z);
			if (BoundaryPolygon.Num() >= 3 && not GetIsPointInsidePolygon(GetXY(Candidate), BoundaryPolygon))
			{
				continue;
			}

			const float AngularDelta = FMath::Abs(FMath::FindDeltaAngleRadians(PreferredAngle, CandidateAngle));
			if (AngularDelta >= BestAngularDelta)
			{
				continue;
			}

			BestAngularDelta = AngularDelta;
			BestCandidate = Candidate;
		}

		return BestCandidate;
	}

	FVector ProjectPointAwayFromAnchorsInsideBoundary(const FVector& Point,
	                                                  const TArray<AAnchorPoint*>& AnchorPoints,
	                                                  const float AvoidanceRadius,
	                                                  const TArray<FVector2D>& BoundaryPolygon)
	{
		if (AvoidanceRadius <= 0.f)
		{
			return Point;
		}

		FVector ProjectedPoint = Point;
		for (int32 ProjectionPassIndex = 0; ProjectionPassIndex < AnchorProjectionMaxPassCount; ProjectionPassIndex++)
		{
			bool bProjectedThisPass = false;
			for (const AAnchorPoint* AnchorPoint : AnchorPoints)
			{
				if (not IsValid(AnchorPoint))
				{
					continue;
				}

				const float DistanceSquared = FVector2D::DistSquared(
					GetXY(ProjectedPoint),
					GetXY(AnchorPoint->GetActorLocation()));
				if (DistanceSquared >= FMath::Square(AvoidanceRadius))
				{
					continue;
				}

				const FVector PreviousProjectedPoint = ProjectedPoint;
				ProjectedPoint = FindBoundarySafeAnchorStandoffPoint(
					ProjectedPoint,
					AnchorPoint->GetActorLocation(),
					AvoidanceRadius,
					BoundaryPolygon);
				bProjectedThisPass = bProjectedThisPass
					|| FVector::DistSquared(PreviousProjectedPoint, ProjectedPoint) > MinPathPointDistanceSquared;
			}

			if (not bProjectedThisPass)
			{
				break;
			}
		}

		return ProjectedPoint;
	}

	TArray<AAnchorPoint*> FindAnchorPoints(const UObject* WorldContextObject)
	{
		TArray<AActor*> AnchorActors;
		UGameplayStatics::GetAllActorsOfClass(WorldContextObject, AAnchorPoint::StaticClass(), AnchorActors);

		TArray<AAnchorPoint*> AnchorPoints;
		AnchorPoints.Reserve(AnchorActors.Num());
		for (AActor* AnchorActor : AnchorActors)
		{
			AAnchorPoint* AnchorPoint = Cast<AAnchorPoint>(AnchorActor);
			if (IsValid(AnchorPoint))
			{
				AnchorPoints.Add(AnchorPoint);
			}
		}

		return AnchorPoints;
	}

	TArray<FVector2D> BuildBoundaryPolygon(const UObject* WorldContextObject)
	{
		TArray<AActor*> BoundaryActors;
		UGameplayStatics::GetAllActorsOfClass(WorldContextObject, AWorldSplineBoundary::StaticClass(), BoundaryActors);

		for (AActor* BoundaryActor : BoundaryActors)
		{
			const AWorldSplineBoundary* Boundary = Cast<AWorldSplineBoundary>(BoundaryActor);
			if (not IsValid(Boundary))
			{
				continue;
			}

			TArray<FVector2D> BoundaryPolygon;
			Boundary->GetSampledPolygon2D(BoundarySampleSpacing, BoundaryPolygon);
			if (BoundaryPolygon.Num() >= 3)
			{
				return BoundaryPolygon;
			}
		}

		return TArray<FVector2D>();
	}

	FVector ProjectToValidMovePoint(const FVector& Point,
	                                const TArray<FVector2D>& BoundaryPolygon,
	                                const TArray<AAnchorPoint*>& AnchorPoints,
	                                const float AvoidanceRadius,
	                                const float BoundaryPadding)
	{
		/*
		 * Keep the operation ordered: boundary first, anchor standoff second, boundary again.
		 * The second boundary pass matters because pushing away from an anchor near the edge can otherwise place the
		 * point just outside the playable polygon.
		 */
		const float AnchorProjectionRadius = GetAnchorDetourRadius(AvoidanceRadius);
		const FVector BoundaryProjectedPoint = ProjectPointInsideBoundary(Point, BoundaryPolygon, BoundaryPadding);
		const FVector AnchorProjectedPoint = ProjectPointAwayFromAnchors(
			BoundaryProjectedPoint,
			AnchorPoints,
			AnchorProjectionRadius);
		const FVector BoundaryAndAnchorProjectedPoint =
			ProjectPointInsideBoundary(AnchorProjectedPoint, BoundaryPolygon, BoundaryPadding);
		return ProjectPointAwayFromAnchorsInsideBoundary(
			BoundaryAndAnchorProjectedPoint,
			AnchorPoints,
			AnchorProjectionRadius,
			BoundaryPolygon);
	}

	float GetPathLength(const FVector& SegmentStart,
	                    const TArray<FVector>& InsertedPathPoints,
	                    const FVector& SegmentEnd)
	{
		float Length = 0.f;
		FVector PreviousPoint = SegmentStart;
		for (const FVector& PathPoint : InsertedPathPoints)
		{
			Length += FVector2D::Distance(GetXY(PreviousPoint), GetXY(PathPoint));
			PreviousPoint = PathPoint;
		}

		return Length + FVector2D::Distance(GetXY(PreviousPoint), GetXY(SegmentEnd));
	}

	bool GetDoesSegmentRespectAnchorAvoidance(const FVector& SegmentStart,
	                                          const FVector& SegmentEnd,
	                                          const FVector& AnchorLocation,
	                                          const float AvoidanceRadius)
	{
		return not GetDoesSegmentIntersectCircle(SegmentStart, SegmentEnd, AnchorLocation, AvoidanceRadius);
	}

	bool GetDoesDetourStayInsideBoundary(const FVector& SegmentStart,
	                                     const TArray<FVector>& DetourPoints,
	                                     const FVector& SegmentEnd,
	                                     const TArray<FVector2D>& BoundaryPolygon);

	bool GetDoesDetourRespectConstraints(const FVector& SegmentStart,
	                                     const TArray<FVector>& DetourPoints,
	                                     const FVector& SegmentEnd,
	                                     const FVector& AnchorLocation,
	                                     const float AvoidanceRadius,
	                                     const TArray<FVector2D>& BoundaryPolygon)
	{
		if (not GetDoesDetourStayInsideBoundary(SegmentStart, DetourPoints, SegmentEnd, BoundaryPolygon))
		{
			return false;
		}

		FVector PreviousPoint = SegmentStart;
		for (const FVector& DetourPoint : DetourPoints)
		{
			if (not GetDoesSegmentRespectAnchorAvoidance(
				PreviousPoint,
				DetourPoint,
				AnchorLocation,
				AvoidanceRadius))
			{
				return false;
			}

			PreviousPoint = DetourPoint;
		}

		return GetDoesSegmentRespectAnchorAvoidance(
			PreviousPoint,
			SegmentEnd,
			AnchorLocation,
			AvoidanceRadius);
	}

	void BuildAnchorDetourRingPoints(const FVector& AnchorLocation,
	                                 const float DetourRadius,
	                                 const float PathZ,
	                                 const TArray<FVector2D>& BoundaryPolygon,
	                                 TArray<FVector>& OutRingPoints,
	                                 TArray<bool>& OutIsValidRingPoint)
	{
		OutRingPoints.SetNum(AnchorBoundaryStandoffSampleCount);
		OutIsValidRingPoint.Init(false, AnchorBoundaryStandoffSampleCount);

		const FVector2D AnchorXY = GetXY(AnchorLocation);
		for (int32 SampleIndex = 0; SampleIndex < AnchorBoundaryStandoffSampleCount; SampleIndex++)
		{
			const float Angle = 2.f * UE_PI * static_cast<float>(SampleIndex)
				/ static_cast<float>(AnchorBoundaryStandoffSampleCount);
			const FVector RingPoint = BuildVectorFromAngle(AnchorXY, Angle, DetourRadius, PathZ);
			OutRingPoints[SampleIndex] = RingPoint;

			if (BoundaryPolygon.Num() >= 3 && not GetIsPointInsidePolygon(GetXY(RingPoint), BoundaryPolygon))
			{
				continue;
			}

			OutIsValidRingPoint[SampleIndex] = true;
		}
	}

	bool TryBuildAnchorRingArcPoints(const TArray<FVector>& RingPoints,
	                                 const TArray<bool>& IsValidRingPoint,
	                                 const int32 EntryIndex,
	                                 const int32 ExitIndex,
	                                 const int32 DirectionStep,
	                                 TArray<FVector>& OutArcPoints)
	{
		OutArcPoints.Reset();
		const int32 RingPointCount = RingPoints.Num();
		if (RingPointCount <= 0)
		{
			return false;
		}

		int32 CurrentIndex = EntryIndex;
		for (int32 StepIndex = 0; StepIndex < RingPointCount; StepIndex++)
		{
			if (not IsValidRingPoint.IsValidIndex(CurrentIndex) || not IsValidRingPoint[CurrentIndex])
			{
				return false;
			}

			OutArcPoints.Add(RingPoints[CurrentIndex]);
			if (CurrentIndex == ExitIndex)
			{
				return true;
			}

			CurrentIndex = (CurrentIndex + DirectionStep + RingPointCount) % RingPointCount;
		}

		return false;
	}

	int32 GetBoundarySegmentSampleCount(const FVector& SegmentStart, const FVector& SegmentEnd)
	{
		const float SegmentDistance = FVector2D::Distance(GetXY(SegmentStart), GetXY(SegmentEnd));
		return FMath::Clamp(
			FMath::CeilToInt(SegmentDistance / BoundarySegmentSampleSpacing),
			BoundarySegmentMinSampleCount,
			BoundarySegmentMaxSampleCount);
	}

	FAnchorDetourCandidate BuildAnchorRingArcCandidate(const FVector& SegmentStart,
	                                                   const FVector& SegmentEnd,
	                                                   const TArray<FVector>& RingPoints,
	                                                   const TArray<bool>& IsValidRingPoint,
	                                                   const int32 EntryIndex,
	                                                   const int32 ExitIndex,
	                                                   const int32 DirectionStep,
	                                                   const FVector& AnchorLocation,
	                                                   const float AvoidanceRadius,
	                                                   const TArray<FVector2D>& BoundaryPolygon)
	{
		FAnchorDetourCandidate Candidate;
		if (not TryBuildAnchorRingArcPoints(
			RingPoints,
			IsValidRingPoint,
			EntryIndex,
			ExitIndex,
			DirectionStep,
			Candidate.Points))
		{
			return Candidate;
		}

		Candidate.bRespectsConstraints = GetDoesDetourRespectConstraints(
			SegmentStart,
			Candidate.Points,
			SegmentEnd,
			AnchorLocation,
			AvoidanceRadius,
			BoundaryPolygon);
		if (Candidate.bRespectsConstraints)
		{
			Candidate.Length = GetPathLength(SegmentStart, Candidate.Points, SegmentEnd);
		}

		return Candidate;
	}

	void TryUpdateBestAnchorDetourCandidate(const FAnchorDetourCandidate& Candidate,
	                                        FAnchorDetourCandidate& InOutBestCandidate)
	{
		if (not Candidate.bRespectsConstraints)
		{
			return;
		}

		if (Candidate.Length >= InOutBestCandidate.Length)
		{
			return;
		}

		InOutBestCandidate = Candidate;
	}

	void TryBuildBestAnchorDetourCandidateForEntry(const FVector& SegmentStart,
	                                               const FVector& SegmentEnd,
	                                               const TArray<FVector>& RingPoints,
	                                               const TArray<bool>& IsValidRingPoint,
	                                               const int32 EntryIndex,
	                                               const FVector& AnchorLocation,
	                                               const float AvoidanceRadius,
	                                               const TArray<FVector2D>& BoundaryPolygon,
	                                               FAnchorDetourCandidate& InOutBestCandidate)
	{
		for (int32 ExitIndex = 0; ExitIndex < RingPoints.Num(); ExitIndex++)
		{
			if (not IsValidRingPoint[ExitIndex]
				|| not GetDoesSegmentRespectAnchorAvoidance(
					RingPoints[ExitIndex],
					SegmentEnd,
					AnchorLocation,
					AvoidanceRadius))
			{
				continue;
			}

			TryUpdateBestAnchorDetourCandidate(
				BuildAnchorRingArcCandidate(
					SegmentStart,
					SegmentEnd,
					RingPoints,
					IsValidRingPoint,
					EntryIndex,
					ExitIndex,
					1,
					AnchorLocation,
					AvoidanceRadius,
					BoundaryPolygon),
				InOutBestCandidate);
			TryUpdateBestAnchorDetourCandidate(
				BuildAnchorRingArcCandidate(
					SegmentStart,
					SegmentEnd,
					RingPoints,
					IsValidRingPoint,
					EntryIndex,
					ExitIndex,
					-1,
					AnchorLocation,
					AvoidanceRadius,
					BoundaryPolygon),
				InOutBestCandidate);
		}
	}

	FAnchorDetourCandidate BuildBestAnchorDetourCandidate(const FVector& SegmentStart,
	                                                      const FVector& SegmentEnd,
	                                                      const FVector& AnchorLocation,
	                                                      const float AvoidanceRadius,
	                                                      const TArray<FVector2D>& BoundaryPolygon)
	{
		FAnchorDetourCandidate BestCandidate;
		TArray<FVector> RingPoints;
		TArray<bool> IsValidRingPoint;
		BuildAnchorDetourRingPoints(
			AnchorLocation,
			GetAnchorDetourRadius(AvoidanceRadius),
			SegmentStart.Z,
			BoundaryPolygon,
			RingPoints,
			IsValidRingPoint);

		for (int32 EntryIndex = 0; EntryIndex < RingPoints.Num(); EntryIndex++)
		{
			if (not IsValidRingPoint[EntryIndex]
				|| not GetDoesSegmentRespectAnchorAvoidance(
					SegmentStart,
					RingPoints[EntryIndex],
					AnchorLocation,
					AvoidanceRadius))
			{
				continue;
			}

			TryBuildBestAnchorDetourCandidateForEntry(
				SegmentStart,
				SegmentEnd,
				RingPoints,
				IsValidRingPoint,
				EntryIndex,
				AnchorLocation,
				AvoidanceRadius,
				BoundaryPolygon,
				BestCandidate);
		}

		return BestCandidate;
	}

	bool TryInsertAnchorDetour(TArray<FVector>& PathPoints,
	                           const TArray<AAnchorPoint*>& AnchorPoints,
	                           const float AvoidanceRadius,
	                           const TArray<FVector2D>& BoundaryPolygon)
	{
		if (AvoidanceRadius <= 0.f || PathPoints.Num() < 2)
		{
			return false;
		}

		bool bInsertedDetour = false;
		for (int32 PathIndex = 0; PathIndex < PathPoints.Num() - 1; PathIndex++)
		{
			for (const AAnchorPoint* AnchorPoint : AnchorPoints)
			{
				if (not IsValid(AnchorPoint))
				{
					continue;
				}

				if (not GetDoesSegmentIntersectCircle(
					PathPoints[PathIndex],
					PathPoints[PathIndex + 1],
					AnchorPoint->GetActorLocation(),
					AvoidanceRadius))
				{
					continue;
				}

				const FAnchorDetourCandidate DetourCandidate = BuildBestAnchorDetourCandidate(
					PathPoints[PathIndex],
					PathPoints[PathIndex + 1],
					AnchorPoint->GetActorLocation(),
					AvoidanceRadius,
					BoundaryPolygon);
				if (not DetourCandidate.bRespectsConstraints || DetourCandidate.Points.Num() <= 0)
				{
					continue;
				}

				for (int32 DetourPointIndex = DetourCandidate.Points.Num() - 1; DetourPointIndex >= 0;
				     DetourPointIndex--)
				{
					PathPoints.Insert(DetourCandidate.Points[DetourPointIndex], PathIndex + 1);
				}
				PathIndex += DetourCandidate.Points.Num();
				bInsertedDetour = true;
				break;
			}
		}

		return bInsertedDetour;
	}

	bool TryGetSegmentIntersectionAlpha(const FVector2D& SegmentStart,
	                                    const FVector2D& SegmentEnd,
	                                    const FVector2D& BoundaryStart,
	                                    const FVector2D& BoundaryEnd,
	                                    float& OutSegmentAlpha)
	{
		const FVector2D SegmentDirection = SegmentEnd - SegmentStart;
		const FVector2D BoundaryDirection = BoundaryEnd - BoundaryStart;
		const float Denominator = GetCross2D(SegmentDirection, BoundaryDirection);
		if (FMath::Abs(Denominator) <= BoundaryIntersectionTolerance)
		{
			return false;
		}

		const FVector2D StartDelta = BoundaryStart - SegmentStart;
		const float SegmentAlpha = GetCross2D(StartDelta, BoundaryDirection) / Denominator;
		const float BoundaryAlpha = GetCross2D(StartDelta, SegmentDirection) / Denominator;
		if (SegmentAlpha <= BoundaryIntersectionTolerance || SegmentAlpha >= 1.f - BoundaryIntersectionTolerance)
		{
			return false;
		}

		if (BoundaryAlpha < -BoundaryIntersectionTolerance || BoundaryAlpha > 1.f + BoundaryIntersectionTolerance)
		{
			return false;
		}

		OutSegmentAlpha = FMath::Clamp(SegmentAlpha, 0.f, 1.f);
		return true;
	}

	void AddUniqueBoundaryIntersection(const FBoundaryIntersection& Intersection,
	                                   TArray<FBoundaryIntersection>& InOutIntersections)
	{
		for (const FBoundaryIntersection& ExistingIntersection : InOutIntersections)
		{
			if (FMath::Abs(ExistingIntersection.SegmentAlpha - Intersection.SegmentAlpha)
				<= BoundaryIntersectionTolerance)
			{
				return;
			}
		}

		InOutIntersections.Add(Intersection);
	}

	void BuildBoundaryIntersectionsForSegment(const FVector& SegmentStart,
	                                          const FVector& SegmentEnd,
	                                          const TArray<FVector2D>& BoundaryPolygon,
	                                          TArray<FBoundaryIntersection>& OutIntersections)
	{
		OutIntersections.Reset();
		const FVector2D SegmentStartXY = GetXY(SegmentStart);
		const FVector2D SegmentEndXY = GetXY(SegmentEnd);
		for (int32 BoundaryIndex = 0; BoundaryIndex < BoundaryPolygon.Num(); BoundaryIndex++)
		{
			float SegmentAlpha = 0.f;
			const FVector2D& BoundaryStart = BoundaryPolygon[BoundaryIndex];
			const FVector2D& BoundaryEnd = BoundaryPolygon[(BoundaryIndex + 1) % BoundaryPolygon.Num()];
			if (not TryGetSegmentIntersectionAlpha(
				SegmentStartXY,
				SegmentEndXY,
				BoundaryStart,
				BoundaryEnd,
				SegmentAlpha))
			{
				continue;
			}

			FBoundaryIntersection Intersection;
			Intersection.Point = SegmentStartXY + (SegmentEndXY - SegmentStartXY) * SegmentAlpha;
			Intersection.SegmentAlpha = SegmentAlpha;
			Intersection.BoundarySegmentIndex = BoundaryIndex;
			AddUniqueBoundaryIntersection(Intersection, OutIntersections);
		}

		OutIntersections.Sort([](const FBoundaryIntersection& Left, const FBoundaryIntersection& Right)
		{
			return Left.SegmentAlpha < Right.SegmentAlpha;
		});
	}

	FBoundaryIntersection BuildBoundaryIntersectionAtSegmentAlpha(const FVector& SegmentStart,
	                                                              const FVector& SegmentEnd,
	                                                              const float SegmentAlpha,
	                                                              const TArray<FVector2D>& BoundaryPolygon)
	{
		FBoundaryIntersection Intersection;
		const FVector PointOnSegment = FMath::Lerp(SegmentStart, SegmentEnd, SegmentAlpha);
		const FBoundaryProjection BoundaryProjection =
			FindClosestBoundaryProjection(GetXY(PointOnSegment), BoundaryPolygon);
		Intersection.Point = BoundaryProjection.Point;
		Intersection.SegmentAlpha = FMath::Clamp(SegmentAlpha, 0.f, 1.f);
		Intersection.BoundarySegmentIndex = BoundaryProjection.SegmentIndex;
		return Intersection;
	}

	float RefineBoundaryTransitionAlpha(const FVector& SegmentStart,
	                                    const FVector& SegmentEnd,
	                                    const float KnownInsideAlpha,
	                                    const float KnownOutsideAlpha,
	                                    const TArray<FVector2D>& BoundaryPolygon)
	{
		float InsideAlpha = KnownInsideAlpha;
		float OutsideAlpha = KnownOutsideAlpha;
		for (int32 RefinementIndex = 0; RefinementIndex < BoundaryCrossingRefinementCount; RefinementIndex++)
		{
			const float CandidateAlpha = (InsideAlpha + OutsideAlpha) * 0.5f;
			const FVector CandidatePoint = FMath::Lerp(SegmentStart, SegmentEnd, CandidateAlpha);
			if (GetIsPointInsidePolygon(GetXY(CandidatePoint), BoundaryPolygon))
			{
				InsideAlpha = CandidateAlpha;
				continue;
			}

			OutsideAlpha = CandidateAlpha;
		}

		return (InsideAlpha + OutsideAlpha) * 0.5f;
	}

	bool TryBuildSampledBoundaryIntersectionsForSegment(const FVector& SegmentStart,
	                                                    const FVector& SegmentEnd,
	                                                    const TArray<FVector2D>& BoundaryPolygon,
	                                                    FBoundaryIntersection& OutExitIntersection,
	                                                    FBoundaryIntersection& OutReentryIntersection)
	{
		const int32 SampleCount = GetBoundarySegmentSampleCount(SegmentStart, SegmentEnd);
		int32 FirstOutsideSampleIndex = INDEX_NONE;
		int32 LastOutsideSampleIndex = INDEX_NONE;
		for (int32 SampleIndex = 0; SampleIndex <= SampleCount; SampleIndex++)
		{
			const float SampleAlpha = static_cast<float>(SampleIndex) / static_cast<float>(SampleCount);
			const FVector SamplePoint = FMath::Lerp(SegmentStart, SegmentEnd, SampleAlpha);
			if (GetIsPointInsidePolygon(GetXY(SamplePoint), BoundaryPolygon))
			{
				continue;
			}

			if (FirstOutsideSampleIndex == INDEX_NONE)
			{
				FirstOutsideSampleIndex = SampleIndex;
			}
			LastOutsideSampleIndex = SampleIndex;
		}

		if (FirstOutsideSampleIndex == INDEX_NONE)
		{
			return false;
		}

		const float ExitAlpha = FirstOutsideSampleIndex > 0
			                        ? RefineBoundaryTransitionAlpha(
				                        SegmentStart,
				                        SegmentEnd,
				                        static_cast<float>(FirstOutsideSampleIndex - 1) / static_cast<float>(SampleCount),
				                        static_cast<float>(FirstOutsideSampleIndex) / static_cast<float>(SampleCount),
				                        BoundaryPolygon)
			                        : 0.f;
		const float ReentryAlpha = LastOutsideSampleIndex < SampleCount
			                           ? RefineBoundaryTransitionAlpha(
				                           SegmentStart,
				                           SegmentEnd,
				                           static_cast<float>(LastOutsideSampleIndex + 1) / static_cast<float>(SampleCount),
				                           static_cast<float>(LastOutsideSampleIndex) / static_cast<float>(SampleCount),
				                           BoundaryPolygon)
			                           : 1.f;
		if (ReentryAlpha - ExitAlpha <= BoundaryIntersectionTolerance)
		{
			return false;
		}

		OutExitIntersection = BuildBoundaryIntersectionAtSegmentAlpha(
			SegmentStart,
			SegmentEnd,
			ExitAlpha,
			BoundaryPolygon);
		OutReentryIntersection = BuildBoundaryIntersectionAtSegmentAlpha(
			SegmentStart,
			SegmentEnd,
			ReentryAlpha,
			BoundaryPolygon);
		return OutExitIntersection.BoundarySegmentIndex != INDEX_NONE
			&& OutReentryIntersection.BoundarySegmentIndex != INDEX_NONE;
	}

	bool GetDoesDetourStayInsideBoundary(const FVector& SegmentStart,
	                                     const TArray<FVector>& DetourPoints,
	                                     const FVector& SegmentEnd,
	                                     const TArray<FVector2D>& BoundaryPolygon)
	{
		FVector OutsidePoint = FVector::ZeroVector;
		FVector PreviousPoint = SegmentStart;
		for (const FVector& DetourPoint : DetourPoints)
		{
			if (TryFindFirstOutsidePointOnSegment(PreviousPoint, DetourPoint, BoundaryPolygon, OutsidePoint))
			{
				return false;
			}

			PreviousPoint = DetourPoint;
		}

		return not TryFindFirstOutsidePointOnSegment(PreviousPoint, SegmentEnd, BoundaryPolygon, OutsidePoint);
	}

	bool TryBuildBoundaryArcPoints(const FBoundaryIntersection& ExitIntersection,
	                               const FBoundaryIntersection& ReentryIntersection,
	                               const int32 DirectionStep,
	                               const TArray<FVector2D>& BoundaryPolygon,
	                               const float BoundaryPadding,
	                               const float Z,
	                               TArray<FVector>& OutArcPoints)
	{
		OutArcPoints.Reset();
		const int32 BoundaryPointCount = BoundaryPolygon.Num();
		if (BoundaryPointCount < 3)
		{
			return false;
		}

		OutArcPoints.Add(BuildBoundaryOffsetPoint(
			ExitIntersection.Point,
			ExitIntersection.BoundarySegmentIndex,
			BoundaryPolygon,
			BoundaryPadding,
			Z));

		int32 CurrentVertexIndex = DirectionStep > 0
			                           ? (ExitIntersection.BoundarySegmentIndex + 1) % BoundaryPointCount
			                           : ExitIntersection.BoundarySegmentIndex;
		const int32 StopVertexIndex = DirectionStep > 0
			                              ? (ReentryIntersection.BoundarySegmentIndex + 1) % BoundaryPointCount
			                              : ReentryIntersection.BoundarySegmentIndex;
		for (int32 StepIndex = 0; StepIndex < BoundaryPointCount; StepIndex++)
		{
			if (CurrentVertexIndex == StopVertexIndex)
			{
				break;
			}

			OutArcPoints.Add(BuildBoundaryVertexOffsetPoint(
				CurrentVertexIndex,
				BoundaryPolygon,
				BoundaryPadding,
				Z));
			CurrentVertexIndex = (CurrentVertexIndex + DirectionStep + BoundaryPointCount) % BoundaryPointCount;
		}

		OutArcPoints.Add(BuildBoundaryOffsetPoint(
			ReentryIntersection.Point,
			ReentryIntersection.BoundarySegmentIndex,
			BoundaryPolygon,
			BoundaryPadding,
			Z));
		return OutArcPoints.Num() >= 2;
	}

	FBoundaryDetourCandidate BuildBoundaryArcCandidate(const FVector& SegmentStart,
	                                                   const FVector& SegmentEnd,
	                                                   const FBoundaryIntersection& ExitIntersection,
	                                                   const FBoundaryIntersection& ReentryIntersection,
	                                                   const int32 DirectionStep,
	                                                   const TArray<FVector2D>& BoundaryPolygon,
	                                                   const float BoundaryPadding)
	{
		FBoundaryDetourCandidate Candidate;
		if (not TryBuildBoundaryArcPoints(
			ExitIntersection,
			ReentryIntersection,
			DirectionStep,
			BoundaryPolygon,
			BoundaryPadding,
			SegmentStart.Z,
			Candidate.Points))
		{
			return Candidate;
		}

		Candidate.bIsValid = GetDoesDetourStayInsideBoundary(
			SegmentStart,
			Candidate.Points,
			SegmentEnd,
			BoundaryPolygon);
		if (Candidate.bIsValid)
		{
			Candidate.Length = GetPathLength(SegmentStart, Candidate.Points, SegmentEnd);
		}

		return Candidate;
	}

	void TryUpdateBestBoundaryDetourCandidate(const FBoundaryDetourCandidate& Candidate,
	                                          FBoundaryDetourCandidate& InOutBestCandidate)
	{
		if (not Candidate.bIsValid || Candidate.Length >= InOutBestCandidate.Length)
		{
			return;
		}

		InOutBestCandidate = Candidate;
	}

	bool TryBuildBestBoundaryDetourPoints(const FVector& SegmentStart,
	                                      const FVector& SegmentEnd,
	                                      const FBoundaryIntersection& ExitIntersection,
	                                      const FBoundaryIntersection& ReentryIntersection,
	                                      const TArray<FVector2D>& BoundaryPolygon,
	                                      const float BoundaryPadding,
	                                      TArray<FVector>& OutDetourPoints)
	{
		OutDetourPoints.Reset();
		FBoundaryDetourCandidate BestCandidate;
		TryUpdateBestBoundaryDetourCandidate(
			BuildBoundaryArcCandidate(
				SegmentStart,
				SegmentEnd,
				ExitIntersection,
				ReentryIntersection,
				1,
				BoundaryPolygon,
				BoundaryPadding),
			BestCandidate);
		TryUpdateBestBoundaryDetourCandidate(
			BuildBoundaryArcCandidate(
				SegmentStart,
				SegmentEnd,
				ExitIntersection,
				ReentryIntersection,
				-1,
				BoundaryPolygon,
				BoundaryPadding),
			BestCandidate);
		if (not BestCandidate.bIsValid)
		{
			return false;
		}

		OutDetourPoints = BestCandidate.Points;
		return true;
	}

	bool TryBuildBoundaryDetourPoints(const FVector& SegmentStart,
	                                  const FVector& SegmentEnd,
	                                  const TArray<FVector2D>& BoundaryPolygon,
	                                  const float BoundaryPadding,
	                                  TArray<FVector>& OutDetourPoints)
	{
		OutDetourPoints.Reset();
		FBoundaryIntersection SampledExitIntersection;
		FBoundaryIntersection SampledReentryIntersection;
		if (TryBuildSampledBoundaryIntersectionsForSegment(
			SegmentStart,
			SegmentEnd,
			BoundaryPolygon,
			SampledExitIntersection,
			SampledReentryIntersection)
			&& TryBuildBestBoundaryDetourPoints(
				SegmentStart,
				SegmentEnd,
				SampledExitIntersection,
				SampledReentryIntersection,
				BoundaryPolygon,
				BoundaryPadding,
				OutDetourPoints))
		{
			return true;
		}

		TArray<FBoundaryIntersection> Intersections;
		BuildBoundaryIntersectionsForSegment(SegmentStart, SegmentEnd, BoundaryPolygon, Intersections);
		if (Intersections.Num() >= 2
			&& TryBuildBestBoundaryDetourPoints(
				SegmentStart,
				SegmentEnd,
				Intersections[0],
				Intersections[1],
				BoundaryPolygon,
				BoundaryPadding,
				OutDetourPoints))
		{
			return true;
		}

		return false;
	}

	void AddDistinctFallbackPoint(const FVector& SegmentStart,
	                              const FVector& CandidatePoint,
	                              TArray<FVector>& InOutFallbackPoints)
	{
		const FVector& PreviousPoint = InOutFallbackPoints.Num() > 0
			                               ? InOutFallbackPoints.Last()
			                               : SegmentStart;
		if (FVector::DistSquared(PreviousPoint, CandidatePoint) <= MinPathPointDistanceSquared)
		{
			return;
		}

		InOutFallbackPoints.Add(CandidatePoint);
	}

	bool TryBuildSampledBoundaryFallbackPoints(const FVector& SegmentStart,
	                                           const FVector& SegmentEnd,
	                                           const TArray<FVector2D>& BoundaryPolygon,
	                                           const float BoundaryPadding,
	                                           TArray<FVector>& OutFallbackPoints)
	{
		OutFallbackPoints.Reset();
		const int32 SampleCount = GetBoundarySegmentSampleCount(SegmentStart, SegmentEnd);
		for (int32 SampleIndex = 1; SampleIndex < SampleCount; SampleIndex++)
		{
			const float Alpha = static_cast<float>(SampleIndex) / static_cast<float>(SampleCount);
			const FVector CandidatePoint = FMath::Lerp(SegmentStart, SegmentEnd, Alpha);
			if (GetIsPointInsidePolygon(GetXY(CandidatePoint), BoundaryPolygon))
			{
				continue;
			}

			AddDistinctFallbackPoint(
				SegmentStart,
				ProjectPointInsideBoundary(CandidatePoint, BoundaryPolygon, BoundaryPadding),
				OutFallbackPoints);
		}

		if (OutFallbackPoints.Num() > 0
			&& FVector::DistSquared(OutFallbackPoints.Last(), SegmentEnd) <= MinPathPointDistanceSquared)
		{
			OutFallbackPoints.RemoveAt(OutFallbackPoints.Num() - 1);
		}

		return OutFallbackPoints.Num() > 0;
	}

	bool GetDoesDetourMakeSegmentProgress(const FVector& SegmentStart,
	                                      const FVector& SegmentEnd,
	                                      const TArray<FVector>& DetourPoints)
	{
		if (DetourPoints.Num() <= 0)
		{
			return false;
		}

		const FVector2D SegmentStartXY = GetXY(SegmentStart);
		const FVector2D SegmentEndXY = GetXY(SegmentEnd);
		const FVector2D SegmentDirection = SegmentEndXY - SegmentStartXY;
		const float SegmentSizeSquared = SegmentDirection.SizeSquared();
		if (SegmentSizeSquared <= MinDirectionSizeSquared)
		{
			return false;
		}

		const float FirstPointAlpha =
			FVector2D::DotProduct(GetXY(DetourPoints[0]) - SegmentStartXY, SegmentDirection) / SegmentSizeSquared;
		const float LastPointAlpha =
			FVector2D::DotProduct(GetXY(DetourPoints.Last()) - SegmentStartXY, SegmentDirection) / SegmentSizeSquared;
		return FirstPointAlpha < 1.f - BoundaryIntersectionTolerance
			&& LastPointAlpha > BoundaryIntersectionTolerance;
	}

	bool TryBuildBoundaryEndpointArcDetourPoints(const FVector& SegmentStart,
	                                             const FVector& SegmentEnd,
	                                             const TArray<FVector2D>& BoundaryPolygon,
	                                             const float BoundaryPadding,
	                                             TArray<FVector>& OutDetourPoints)
	{
		OutDetourPoints.Reset();

		FVector OutsidePoint = FVector::ZeroVector;
		if (not TryFindFirstOutsidePointOnSegment(SegmentStart, SegmentEnd, BoundaryPolygon, OutsidePoint))
		{
			return false;
		}

		const FBoundaryProjection StartProjection = FindClosestBoundaryProjection(GetXY(SegmentStart), BoundaryPolygon);
		const FBoundaryProjection EndProjection = FindClosestBoundaryProjection(GetXY(SegmentEnd), BoundaryPolygon);
		if (StartProjection.SegmentIndex == INDEX_NONE || EndProjection.SegmentIndex == INDEX_NONE)
		{
			return false;
		}

		FBoundaryIntersection ExitIntersection;
		ExitIntersection.Point = StartProjection.Point;
		ExitIntersection.SegmentAlpha = 0.f;
		ExitIntersection.BoundarySegmentIndex = StartProjection.SegmentIndex;

		FBoundaryIntersection ReentryIntersection;
		ReentryIntersection.Point = EndProjection.Point;
		ReentryIntersection.SegmentAlpha = 1.f;
		ReentryIntersection.BoundarySegmentIndex = EndProjection.SegmentIndex;
		return TryBuildBestBoundaryDetourPoints(
			SegmentStart,
			SegmentEnd,
			ExitIntersection,
			ReentryIntersection,
			BoundaryPolygon,
			BoundaryPadding,
			OutDetourPoints);
	}

	bool TryBuildBoundaryDetourForSegment(const FVector& SegmentStart,
	                                      const FVector& SegmentEnd,
	                                      const TArray<FVector2D>& BoundaryPolygon,
	                                      const float BoundaryPadding,
	                                      TArray<FVector>& OutDetourPoints)
	{
		OutDetourPoints.Reset();
		if (TryBuildBoundaryDetourPoints(
			SegmentStart,
			SegmentEnd,
			BoundaryPolygon,
			BoundaryPadding,
			OutDetourPoints)
			&& GetDoesDetourMakeSegmentProgress(SegmentStart, SegmentEnd, OutDetourPoints))
		{
			return true;
		}

		if (TryBuildBoundaryEndpointArcDetourPoints(
			SegmentStart,
			SegmentEnd,
			BoundaryPolygon,
			BoundaryPadding,
			OutDetourPoints)
			&& GetDoesDetourMakeSegmentProgress(SegmentStart, SegmentEnd, OutDetourPoints))
		{
			return true;
		}

		if (TryBuildSampledBoundaryFallbackPoints(
			SegmentStart,
			SegmentEnd,
			BoundaryPolygon,
			BoundaryPadding,
			OutDetourPoints))
		{
			return true;
		}

		FVector OutsidePoint = FVector::ZeroVector;
		if (not TryFindFirstOutsidePointOnSegment(SegmentStart, SegmentEnd, BoundaryPolygon, OutsidePoint))
		{
			return false;
		}

		OutDetourPoints.Add(ProjectPointInsideBoundary(OutsidePoint, BoundaryPolygon, BoundaryPadding));
		return true;
	}

	void InsertDetourPointsAfterIndex(TArray<FVector>& PathPoints,
	                                  const int32 PathIndex,
	                                  const TArray<FVector>& DetourPoints)
	{
		for (int32 DetourPointIndex = DetourPoints.Num() - 1; DetourPointIndex >= 0; DetourPointIndex--)
		{
			PathPoints.Insert(DetourPoints[DetourPointIndex], PathIndex + 1);
		}
	}

	bool TryInsertBoundaryDetour(TArray<FVector>& PathPoints,
	                             const TArray<FVector2D>& BoundaryPolygon,
	                             const float BoundaryPadding)
	{
		if (BoundaryPolygon.Num() < 3 || PathPoints.Num() < 2)
		{
			return false;
		}

		bool bInsertedDetour = false;
		for (int32 PathIndex = 0; PathIndex < PathPoints.Num() - 1; PathIndex++)
		{
			TArray<FVector> DetourPoints;
			if (not TryBuildBoundaryDetourForSegment(
				PathPoints[PathIndex],
				PathPoints[PathIndex + 1],
				BoundaryPolygon,
				BoundaryPadding,
				DetourPoints))
			{
				continue;
			}

			InsertDetourPointsAfterIndex(PathPoints, PathIndex, DetourPoints);
			PathIndex += DetourPoints.Num();
			bInsertedDetour = true;
		}

		return bInsertedDetour;
	}

	void ProjectAllPathPoints(TArray<FVector>& PathPoints,
	                          const TArray<FVector2D>& BoundaryPolygon,
	                          const TArray<AAnchorPoint*>& AnchorPoints,
	                          const float AvoidanceRadius,
	                          const float BoundaryPadding)
	{
		for (FVector& PathPoint : PathPoints)
		{
			PathPoint = ProjectToValidMovePoint(
				PathPoint,
				BoundaryPolygon,
				AnchorPoints,
				AvoidanceRadius,
				BoundaryPadding);
		}
	}

	void RemoveDuplicatePathPoints(TArray<FVector>& PathPoints)
	{
		for (int32 PathIndex = PathPoints.Num() - 1; PathIndex > 0; PathIndex--)
		{
			if (FVector::DistSquared(PathPoints[PathIndex], PathPoints[PathIndex - 1])
				<= MinPathPointDistanceSquared)
			{
				PathPoints.RemoveAt(PathIndex);
			}
		}
	}

	void RepairPath(TArray<FVector>& PathPoints,
	                const TArray<FVector2D>& BoundaryPolygon,
	                const TArray<AAnchorPoint*>& AnchorPoints,
	                const float AvoidanceRadius,
	                const float BoundaryPadding,
	                const int32 MaxRepairAttempts)
	{
		/*
		 * Navigation can return useful high-level path points while still ignoring campaign-only constraints.
		 * This repair pass treats the nav path as a draft, then inserts small detours until segments stay inside the
		 * world boundary and outside anchor avoidance circles.
		 */
		ProjectAllPathPoints(PathPoints, BoundaryPolygon, AnchorPoints, AvoidanceRadius, BoundaryPadding);
		RemoveDuplicatePathPoints(PathPoints);

		for (int32 AttemptIndex = 0; AttemptIndex < MaxRepairAttempts; AttemptIndex++)
		{
			const bool bInsertedBoundaryDetour = TryInsertBoundaryDetour(
				PathPoints,
				BoundaryPolygon,
				BoundaryPadding);
			const bool bInsertedAnchorDetour = TryInsertAnchorDetour(
				PathPoints,
				AnchorPoints,
				AvoidanceRadius,
				BoundaryPolygon);
			if (not bInsertedBoundaryDetour && not bInsertedAnchorDetour)
			{
				break;
			}

			ProjectAllPathPoints(PathPoints, BoundaryPolygon, AnchorPoints, AvoidanceRadius, BoundaryPadding);
			RemoveDuplicatePathPoints(PathPoints);
		}
	}
}

AWorldDivisionBase::AWorldDivisionBase()
{
	PrimaryActorTick.bCanEverTick = false;

	M_MovementComponent = CreateDefaultSubobject<UWorldDivisionMovementComponent>(TEXT("WorldDivisionMovement"));
	M_InfluenceComponent = CreateDefaultSubobject<UWorldDivisionInfluenceComponent>(TEXT("WorldDivisionInfluence"));
}

void AWorldDivisionBase::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	(void)GetIsValidMovementComponent();
}

void AWorldDivisionBase::InitializeWorldDivision(const FGuid& DivisionKey,
                                                 const EWorldFieldDivisions DivisionType,
                                                 const int32 OwningPlayer,
                                                 const int32 MaxStrengthPercentage,
                                                 const FText& StrengthReasonText)
{
	M_DivisionKey = DivisionKey.IsValid() ? DivisionKey : FGuid::NewGuid();
	M_DivisionType = DivisionType;
	M_OwningPlayer = OwningPlayer;
	M_MaxStrengthPercentage = MaxStrengthPercentage;
	M_CurrentStrengthPercentage = MaxStrengthPercentage;
	M_StrengthReasonText = StrengthReasonText;
}

void AWorldDivisionBase::RestoreWorldDivisionSaveData(const FWorldDivisionSaveData& DivisionSaveData)
{
	M_DivisionKey = DivisionSaveData.DivisionKey;
	M_DivisionType = DivisionSaveData.DivisionType;
	M_OwningPlayer = DivisionSaveData.OwningPlayer;
	M_CurrentStrengthPercentage = DivisionSaveData.CurrentStrengthPercentage;
	M_PendingTargetLocation = DivisionSaveData.TargetLocation;
	M_PathPoints = DivisionSaveData.PathPoints;
	M_CurrentPathPointIndex = DivisionSaveData.CurrentPathPointIndex;
	bM_HasPendingMoveOrder = DivisionSaveData.bHasPendingMoveOrder
		&& M_PathPoints.IsValidIndex(M_CurrentPathPointIndex);
	SetActorLocation(DivisionSaveData.Location);
	RestoreSubtypeSaveData(DivisionSaveData);
}

FWorldDivisionSaveData AWorldDivisionBase::BuildWorldDivisionSaveData() const
{
	FWorldDivisionSaveData DivisionSaveData;
	DivisionSaveData.DivisionKey = M_DivisionKey;
	DivisionSaveData.DivisionType = M_DivisionType;
	DivisionSaveData.OwningPlayer = M_OwningPlayer;
	DivisionSaveData.Location = GetActorLocation();
	DivisionSaveData.CurrentStrengthPercentage = M_CurrentStrengthPercentage;
	DivisionSaveData.TargetLocation = M_PendingTargetLocation;
	DivisionSaveData.PathPoints = M_PathPoints;
	DivisionSaveData.CurrentPathPointIndex = M_CurrentPathPointIndex;
	DivisionSaveData.bHasPendingMoveOrder = bM_HasPendingMoveOrder;
	FillSubtypeSaveData(DivisionSaveData);
	return DivisionSaveData;
}

bool AWorldDivisionBase::IssueMoveOrderToPoint(const FVector& TargetWorldPoint)
{
	const TArray<FVector> PathPoints = BuildPathToTargetPoint(TargetWorldPoint);
	if (PathPoints.Num() < 2)
	{
		return false;
	}

	CachePendingMoveOrder(PathPoints.Last(), PathPoints);
	return true;
}

void AWorldDivisionBase::AdvanceTurn()
{
	/*
	 * This editor path intentionally bypasses interpolation. CallInEditor does not have the same predictable runtime
	 * ticking as the turn system, but designers still need the exact same distance-budget behavior for quick testing.
	 */
	if (AdvanceTurnTargetLocation.IsNearlyZero(AdvanceTurnTargetNearlyZeroTolerance)
		|| GetIsAdvanceTurnTargetReached()
		|| GetIsMoving())
	{
		return;
	}

#if WITH_EDITOR
	Modify();
#endif

	if (not bM_HasPendingMoveOrder
		|| FVector2D::DistSquared(GetXY(M_PendingTargetLocation), GetXY(AdvanceTurnTargetLocation))
		> AdvanceTurnTargetReachedDistanceSquared)
	{
		if (not IssueMoveOrderToPoint(AdvanceTurnTargetLocation))
		{
			return;
		}
	}

	TArray<FVector> MovementPathPoints;
	if (not ConsumeMovementBudget(M_DistanceTravelledPerTurn, MovementPathPoints))
	{
		return;
	}

	SetActorLocation(MovementPathPoints.Last());
}

void AWorldDivisionBase::DebugPath()
{
	TArray<FVector> PathPoints;

	/*
	 * Prefer the editable test target over cached orders. This lets a designer adjust the vector and inspect the
	 * repaired path without committing a move order or advancing the division.
	 */
	if (not AdvanceTurnTargetLocation.IsNearlyZero(AdvanceTurnTargetNearlyZeroTolerance))
	{
		PathPoints = BuildPathToTargetPoint(AdvanceTurnTargetLocation);
	}
	else if (bM_HasPendingMoveOrder && M_PathPoints.IsValidIndex(M_CurrentPathPointIndex))
	{
		PathPoints.Add(GetActorLocation());
		for (int32 PathPointIndex = M_CurrentPathPointIndex; PathPointIndex < M_PathPoints.Num(); PathPointIndex++)
		{
			PathPoints.Add(M_PathPoints[PathPointIndex]);
		}
	}

	DrawDebugPathPoints(PathPoints);
}

bool AWorldDivisionBase::MoveForTurnDistance(const float DistanceBudgetOverride)
{
	if (not bM_HasPendingMoveOrder || GetIsMoving())
	{
		return false;
	}

	const float DistanceBudget = DistanceBudgetOverride > 0.f
		                             ? DistanceBudgetOverride
		                             : M_DistanceTravelledPerTurn;
	TArray<FVector> MovementPathPoints;
	if (not ConsumeMovementBudget(DistanceBudget, MovementPathPoints))
	{
		return false;
	}

	if (M_Speed <= 0.f || not GetIsValidMovementComponent())
	{
		SetActorLocation(MovementPathPoints.Last());
		return false;
	}

	M_MovementComponent->StartMovement(MovementPathPoints, M_Speed);
	return true;
}

void AWorldDivisionBase::ApplyWorldDivisionDamage(const int32 DamageEntryCount)
{
	if (DamageEntryCount <= 0)
	{
		return;
	}

	RTSFunctionLibrary::ReportWarning(
		TEXT("ApplyWorldDivisionDamage called on AWorldDivisionBase. Derived divisions should remove subtype entries."));
}

void AWorldDivisionBase::SetOwningPlayer(const int32 OwningPlayer)
{
	M_OwningPlayer = OwningPlayer;
}

void AWorldDivisionBase::SetDivisionType(const EWorldFieldDivisions DivisionType)
{
	M_DivisionType = DivisionType;
}

void AWorldDivisionBase::SetCurrentStrengthPercentage(const int32 StrengthPercentage)
{
	M_CurrentStrengthPercentage = StrengthPercentage;
}

void AWorldDivisionBase::SetMaxStrengthPercentage(const int32 StrengthPercentage)
{
	M_MaxStrengthPercentage = StrengthPercentage;
}

void AWorldDivisionBase::SetStrengthReasonText(const FText& StrengthReasonText)
{
	M_StrengthReasonText = StrengthReasonText;
}

bool AWorldDivisionBase::GetIsMoving() const
{
	if (not IsValid(M_MovementComponent))
	{
		return false;
	}

	return M_MovementComponent->GetIsMoving();
}

void AWorldDivisionBase::RecalculateCurrentStrengthFromEntries(const int32 RemainingEntryCount,
                                                               const int32 StartingEntryCount)
{
	if (RemainingEntryCount <= 0 || StartingEntryCount <= 0 || M_MaxStrengthPercentage <= 0)
	{
		M_CurrentStrengthPercentage = 0;
		return;
	}

	const float RemainingAlpha = static_cast<float>(RemainingEntryCount) / static_cast<float>(StartingEntryCount);
	M_CurrentStrengthPercentage = FMath::RoundToInt(static_cast<float>(M_MaxStrengthPercentage) * RemainingAlpha);
}

int32 AWorldDivisionBase::EstimateStartingEntryCountFromCurrentStrength(const int32 RemainingEntryCount) const
{
	if (RemainingEntryCount <= 0 || M_CurrentStrengthPercentage <= 0 || M_MaxStrengthPercentage <= 0)
	{
		return RemainingEntryCount;
	}

	const float StrengthAlpha = static_cast<float>(M_CurrentStrengthPercentage)
		/ static_cast<float>(M_MaxStrengthPercentage);
	if (StrengthAlpha <= 0.f)
	{
		return RemainingEntryCount;
	}

	return FMath::Max(RemainingEntryCount, FMath::RoundToInt(static_cast<float>(RemainingEntryCount) / StrengthAlpha));
}

void AWorldDivisionBase::BroadcastDivisionStrengthChanged()
{
	M_OnDivisionStrengthChanged.Broadcast(this);
}

void AWorldDivisionBase::FillSubtypeSaveData(FWorldDivisionSaveData& OutDivisionSaveData) const
{
}

void AWorldDivisionBase::RestoreSubtypeSaveData(const FWorldDivisionSaveData& DivisionSaveData)
{
}

bool AWorldDivisionBase::GetIsValidMovementComponent() const
{
	if (IsValid(M_MovementComponent))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		TEXT("M_MovementComponent"),
		TEXT("AWorldDivisionBase::GetIsValidMovementComponent"),
		this);
	return false;
}

bool AWorldDivisionBase::ConsumeMovementBudget(const float DistanceBudget,
                                               TArray<FVector>& OutMovementPathPoints)
{
	/*
	 * The pending path remains authoritative across turns. This function consumes only enough of that path for the
	 * current turn and leaves M_CurrentPathPointIndex pointing at the next unconsumed path point.
	 */
	OutMovementPathPoints.Reset();
	if (not M_PathPoints.IsValidIndex(M_CurrentPathPointIndex))
	{
		bM_HasPendingMoveOrder = false;
		return false;
	}

	float RemainingDistance = FMath::Max(0.f, DistanceBudget);
	FVector CurrentLocation = GetActorLocation();
	OutMovementPathPoints.Add(CurrentLocation);

	while (RemainingDistance > 0.f && M_PathPoints.IsValidIndex(M_CurrentPathPointIndex))
	{
		FVector TargetLocation = M_PathPoints[M_CurrentPathPointIndex];
		TargetLocation.Z = CurrentLocation.Z;
		const FVector2D CurrentXY = GetXY(CurrentLocation);
		const FVector2D TargetXY = GetXY(TargetLocation);
		const float DistanceToTarget = FVector2D::Distance(CurrentXY, TargetXY);
		if (DistanceToTarget <= KINDA_SMALL_NUMBER)
		{
			M_CurrentPathPointIndex++;
			continue;
		}

		if (DistanceToTarget <= RemainingDistance)
		{
			CurrentLocation = TargetLocation;
			OutMovementPathPoints.Add(CurrentLocation);
			RemainingDistance -= DistanceToTarget;
			M_CurrentPathPointIndex++;
			continue;
		}

		const FVector2D Direction = (TargetXY - CurrentXY).GetSafeNormal();
		const FVector2D PartialXY = CurrentXY + Direction * RemainingDistance;
		CurrentLocation = FVector(PartialXY.X, PartialXY.Y, CurrentLocation.Z);
		OutMovementPathPoints.Add(CurrentLocation);
		RemainingDistance = 0.f;
	}

	if (not M_PathPoints.IsValidIndex(M_CurrentPathPointIndex))
	{
		bM_HasPendingMoveOrder = false;
	}

	return OutMovementPathPoints.Num() >= 2;
}

bool AWorldDivisionBase::GetIsAdvanceTurnTargetReached() const
{
	const FVector2D CurrentLocationXY = GetXY(GetActorLocation());
	const FVector2D TargetLocationXY = GetXY(AdvanceTurnTargetLocation);
	return FVector2D::DistSquared(CurrentLocationXY, TargetLocationXY)
		<= AdvanceTurnTargetReachedDistanceSquared;
}

TArray<FVector> AWorldDivisionBase::BuildPathToTargetPoint(const FVector& TargetWorldPoint) const
{
	/*
	 * The division asks Unreal navigation first, but the campaign map has extra rules that normal nav data may not know
	 * about: anchor standoff circles and AWorldSplineBoundary containment. Those are applied after the nav query.
	 */
	const UWorldCampaignSettings* Settings = UWorldCampaignSettings::Get();
	const float AvoidanceRadius = IsValid(Settings) ? Settings->WorldDivisionAnchorAvoidanceRadius : 0.f;
	const float BoundaryPadding = IsValid(Settings) ? Settings->WorldDivisionBoundaryProjectionPadding : 0.f;
	const int32 MaxRepairAttempts = IsValid(Settings) ? Settings->WorldDivisionPathRepairMaxAttempts : 1;

	const TArray<AAnchorPoint*> AnchorPoints = FindAnchorPoints(this);
	const TArray<FVector2D> BoundaryPolygon = BuildBoundaryPolygon(this);
	const FVector StartLocation = ProjectToValidMovePoint(
		GetActorLocation(),
		BoundaryPolygon,
		AnchorPoints,
		AvoidanceRadius,
		BoundaryPadding);
	const FVector TargetLocation = ProjectToValidMovePoint(
		TargetWorldPoint,
		BoundaryPolygon,
		AnchorPoints,
		AvoidanceRadius,
		BoundaryPadding);

	TArray<FVector> PathPoints;
	if (UWorld* World = GetWorld())
	{
		UNavigationSystemV1* NavigationSystem = UNavigationSystemV1::GetCurrent(World);
		UNavigationPath* NavigationPath = IsValid(NavigationSystem)
			                                  ? UNavigationSystemV1::FindPathToLocationSynchronously(
				                                  const_cast<AWorldDivisionBase*>(this),
				                                  StartLocation,
				                                  TargetLocation)
			                                  : nullptr;
		if (IsValid(NavigationPath) && NavigationPath->PathPoints.Num() >= 2)
		{
			PathPoints = NavigationPath->PathPoints;
			if constexpr (DeveloperSettings::Debugging::GWorldCampaign_DivisionPathing_Compile_DebugSymbols)
			{
				DebugDrawUnrealDivisionPath(World, PathPoints);
			}
		}
	}

	if (PathPoints.Num() < 2)
	{
		PathPoints.Add(StartLocation);
		PathPoints.Add(TargetLocation);
	}

	for (FVector& PathPoint : PathPoints)
	{
		PathPoint.Z = StartLocation.Z;
	}

	PathPoints[0] = StartLocation;
	PathPoints[PathPoints.Num() - 1] = TargetLocation;
	RepairPath(
		PathPoints,
		BoundaryPolygon,
		AnchorPoints,
		AvoidanceRadius,
		BoundaryPadding,
		FMath::Max(FMath::Max(MinimumPathRepairAttemptCount, MaxRepairAttempts), AnchorPoints.Num()));
	if constexpr (DeveloperSettings::Debugging::GWorldCampaign_DivisionPathing_Compile_DebugSymbols)
	{
		DebugDrawAdjustedDivisionPath(GetWorld(), PathPoints);
	}

	return PathPoints;
}

void AWorldDivisionBase::DrawDebugPathPoints(const TArray<FVector>& PathPoints) const
{
	DrawDebugPathPointsWithOffset(GetWorld(), PathPoints, FColor::Cyan, DebugPathLineThickness);
}

void AWorldDivisionBase::CachePendingMoveOrder(const FVector& TargetWorldPoint,
                                               const TArray<FVector>& PathPoints)
{
	M_PendingTargetLocation = TargetWorldPoint;
	M_PathPoints = PathPoints;
	M_CurrentPathPointIndex = M_PathPoints.Num() >= 2 ? 1 : 0;
	bM_HasPendingMoveOrder = M_PathPoints.Num() >= 2;
}
