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
	constexpr float DetourPaddingScale = 2.f;
	constexpr float DebugPathDrawDurationSeconds = 5.f;
	constexpr float DebugPathLineThickness = 8.f;
	constexpr float DebugUnrealPathLineThickness = 2.f;
	constexpr float DebugAdjustedPathLineThickness = 1.f;
	constexpr float DebugPathZOffset = 25.f;
	constexpr int32 SegmentSampleCount = 24;
	constexpr uint8 DebugPathDepthPriority = 0;

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

	float GetCross2D(const FVector2D& Left, const FVector2D& Right)
	{
		return Left.X * Right.Y - Left.Y * Right.X;
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

	FVector ProjectPointInsideBoundary(const FVector& Point,
	                                   const TArray<FVector2D>& BoundaryPolygon,
	                                   const float Padding)
	{
		if (BoundaryPolygon.Num() < 3 || GetIsPointInsidePolygon(GetXY(Point), BoundaryPolygon))
		{
			return Point;
		}

		const FVector2D PointXY = GetXY(Point);
		const FVector2D Centroid = GetPolygonCentroid(BoundaryPolygon);
		FVector2D ClosestBoundaryPoint = BoundaryPolygon[0];
		float ClosestDistanceSquared = TNumericLimits<float>::Max();
		for (int32 Index = 0; Index < BoundaryPolygon.Num(); Index++)
		{
			const FVector2D& SegmentStart = BoundaryPolygon[Index];
			const FVector2D& SegmentEnd = BoundaryPolygon[(Index + 1) % BoundaryPolygon.Num()];
			const FVector2D Candidate = GetClosestPointOnSegment(PointXY, SegmentStart, SegmentEnd);
			const float CandidateDistanceSquared = FVector2D::DistSquared(PointXY, Candidate);
			if (CandidateDistanceSquared < ClosestDistanceSquared)
			{
				ClosestDistanceSquared = CandidateDistanceSquared;
				ClosestBoundaryPoint = Candidate;
			}
		}

		const FVector2D DirectionInside = (Centroid - ClosestBoundaryPoint).GetSafeNormal();
		const FVector2D ProjectedPoint = ClosestBoundaryPoint + DirectionInside * Padding;
		return BuildVectorFromXY(ProjectedPoint, Point.Z);
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

		for (int32 SampleIndex = 1; SampleIndex <= SegmentSampleCount; SampleIndex++)
		{
			const float Alpha = static_cast<float>(SampleIndex) / static_cast<float>(SegmentSampleCount);
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
		return FVector2D::DistSquared(ClosestPoint, CircleCenterXY) <= FMath::Square(CircleRadius);
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
		const FVector BoundaryProjectedPoint = ProjectPointInsideBoundary(Point, BoundaryPolygon, BoundaryPadding);
		const FVector AnchorProjectedPoint = ProjectPointAwayFromAnchors(
			BoundaryProjectedPoint,
			AnchorPoints,
			AvoidanceRadius);
		return ProjectPointInsideBoundary(AnchorProjectedPoint, BoundaryPolygon, BoundaryPadding);
	}

	FVector BuildAnchorDetourPoint(const FVector& SegmentStart,
	                               const FVector& SegmentEnd,
	                               const FVector& AnchorLocation,
	                               const float AvoidanceRadius,
	                               const TArray<FVector2D>& BoundaryPolygon,
	                               const float BoundaryPadding)
	{
		const FVector2D Direction = (GetXY(SegmentEnd) - GetXY(SegmentStart)).GetSafeNormal();
		FVector2D Perpendicular(-Direction.Y, Direction.X);
		if (Perpendicular.SizeSquared() <= MinDirectionSizeSquared)
		{
			Perpendicular = FVector2D(1.f, 0.f);
		}

		const float Side = GetCross2D(Direction, GetXY(AnchorLocation) - GetXY(SegmentStart)) >= 0.f ? -1.f : 1.f;
		const FVector2D AnchorXY = GetXY(AnchorLocation);
		FVector Candidate = BuildVectorFromXY(AnchorXY + Perpendicular * Side * AvoidanceRadius, SegmentStart.Z);
		Candidate = ProjectPointInsideBoundary(Candidate, BoundaryPolygon, BoundaryPadding);
		if (GetIsPointInsidePolygon(GetXY(Candidate), BoundaryPolygon))
		{
			return Candidate;
		}

		Candidate = BuildVectorFromXY(AnchorXY - Perpendicular * Side * AvoidanceRadius, SegmentStart.Z);
		return ProjectPointInsideBoundary(Candidate, BoundaryPolygon, BoundaryPadding);
	}

	bool TryInsertAnchorDetour(TArray<FVector>& PathPoints,
	                           const TArray<AAnchorPoint*>& AnchorPoints,
	                           const float AvoidanceRadius,
	                           const TArray<FVector2D>& BoundaryPolygon,
	                           const float BoundaryPadding)
	{
		if (AvoidanceRadius <= 0.f || PathPoints.Num() < 2)
		{
			return false;
		}

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

				const FVector DetourPoint = BuildAnchorDetourPoint(
					PathPoints[PathIndex],
					PathPoints[PathIndex + 1],
					AnchorPoint->GetActorLocation(),
					AvoidanceRadius,
					BoundaryPolygon,
					BoundaryPadding);
				PathPoints.Insert(DetourPoint, PathIndex + 1);
				return true;
			}
		}

		return false;
	}

	bool TryInsertBoundaryDetour(TArray<FVector>& PathPoints,
	                             const TArray<FVector2D>& BoundaryPolygon,
	                             const float BoundaryPadding)
	{
		if (BoundaryPolygon.Num() < 3 || PathPoints.Num() < 2)
		{
			return false;
		}

		for (int32 PathIndex = 0; PathIndex < PathPoints.Num() - 1; PathIndex++)
		{
			FVector OutsidePoint = FVector::ZeroVector;
			if (not TryFindFirstOutsidePointOnSegment(
				PathPoints[PathIndex],
				PathPoints[PathIndex + 1],
				BoundaryPolygon,
				OutsidePoint))
			{
				continue;
			}

			FVector ProjectedPoint = ProjectPointInsideBoundary(OutsidePoint, BoundaryPolygon, BoundaryPadding);
			const FVector2D SegmentDirection = (GetXY(PathPoints[PathIndex + 1]) - GetXY(PathPoints[PathIndex]))
				.GetSafeNormal();
			const FVector2D Perpendicular(-SegmentDirection.Y, SegmentDirection.X);
			ProjectedPoint += BuildVectorFromXY(Perpendicular * BoundaryPadding * DetourPaddingScale, 0.f);
			ProjectedPoint = ProjectPointInsideBoundary(ProjectedPoint, BoundaryPolygon, BoundaryPadding);
			PathPoints.Insert(ProjectedPoint, PathIndex + 1);
			return true;
		}

		return false;
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
			const bool bInsertedAnchorDetour = not bInsertedBoundaryDetour
				                                  && TryInsertAnchorDetour(
					                                  PathPoints,
					                                  AnchorPoints,
					                                  AvoidanceRadius,
					                                  BoundaryPolygon,
					                                  BoundaryPadding);
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
		FMath::Max(1, MaxRepairAttempts));
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
