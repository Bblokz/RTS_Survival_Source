// Copyright (C) Bas Blokzijl - All rights reserved.

#include "WorldCampaign/WorldMapObjects/Connection/Connection.h"

#include "DrawDebugHelpers.h"
#include "WorldCampaign/WorldMapObjects/AnchorPoint/AnchorPoint.h"

AConnection::AConnection()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AConnection::InitializeConnection(AAnchorPoint* AnchorA, AAnchorPoint* AnchorB)
{
	if (not IsValid(AnchorA))
	{
		return;
	}

	if (not IsValid(AnchorB))
	{
		return;
	}

	SetConnectedAnchors(AnchorA, AnchorB);
	bM_IsThreeWayConnection = false;

	const FVector AnchorLocationA = AnchorA->GetActorLocation();
	const FVector AnchorLocationB = AnchorB->GetActorLocation();
	constexpr float HalfValue = 0.5f;
	SetActorLocation((AnchorLocationA + AnchorLocationB) * HalfValue);
	M_JunctionLocation = GetActorLocation();
}

bool AConnection::TryAddThirdAnchor(AAnchorPoint* AnchorPoint)
{
	if (bM_IsThreeWayConnection)
	{
		return false;
	}

	if (not IsValid(AnchorPoint))
	{
		return false;
	}

	if (M_ConnectedAnchors.Num() < 2)
	{
		return false;
	}

	M_JunctionLocation = GetProjectedPointOnBaseSegment(AnchorPoint->GetActorLocation());
	M_ConnectedAnchors.Add(AnchorPoint);
	bM_IsThreeWayConnection = true;
	return true;
}

void AConnection::DebugDrawBaseConnection(const FColor& Color, float Duration) const
{
	if (M_ConnectedAnchors.Num() < 2)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	constexpr float LineThickness = 5.f;
	DrawDebugLine(World, GetAnchorLocationSafe(M_ConnectedAnchors[0]), GetAnchorLocationSafe(M_ConnectedAnchors[1]),
		Color, false, Duration, 0, LineThickness);
}

void AConnection::DebugDrawThirdConnection(const FColor& Color, float Duration) const
{
	if (not bM_IsThreeWayConnection)
	{
		return;
	}

	if (M_ConnectedAnchors.Num() < 3)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	constexpr float LineThickness = 5.f;
	DrawDebugLine(World, M_JunctionLocation, GetAnchorLocationSafe(M_ConnectedAnchors[2]),
		Color, false, Duration, 0, LineThickness);
}

void AConnection::SetConnectedAnchors(AAnchorPoint* AnchorA, AAnchorPoint* AnchorB)
{
	M_ConnectedAnchors.Reset();
	M_ConnectedAnchors.Add(AnchorA);
	M_ConnectedAnchors.Add(AnchorB);
}

FVector AConnection::GetAnchorLocationSafe(const AAnchorPoint* AnchorPoint) const
{
	if (not IsValid(AnchorPoint))
	{
		return GetActorLocation();
	}

	return AnchorPoint->GetActorLocation();
}

FVector AConnection::GetProjectedPointOnBaseSegment(const FVector& AnchorLocation) const
{
	if (M_ConnectedAnchors.Num() < 2)
	{
		return GetActorLocation();
	}

	const FVector AnchorLocationA = GetAnchorLocationSafe(M_ConnectedAnchors[0]);
	const FVector AnchorLocationB = GetAnchorLocationSafe(M_ConnectedAnchors[1]);

	const FVector2D AnchorLocation2D(AnchorLocation.X, AnchorLocation.Y);
	const FVector2D AnchorLocationA2D(AnchorLocationA.X, AnchorLocationA.Y);
	const FVector2D AnchorLocationB2D(AnchorLocationB.X, AnchorLocationB.Y);

	const FVector2D SegmentVector = AnchorLocationB2D - AnchorLocationA2D;
	const FVector2D AnchorVector = AnchorLocation2D - AnchorLocationA2D;
	const float SegmentLengthSquared = SegmentVector.SizeSquared();
	if (SegmentLengthSquared <= KINDA_SMALL_NUMBER)
	{
		return AnchorLocationA;
	}

	const float Projection = FVector2D::DotProduct(AnchorVector, SegmentVector) / SegmentLengthSquared;
	const float ClampedProjection = FMath::Clamp(Projection, 0.f, 1.f);
	const FVector2D ProjectedPoint2D = AnchorLocationA2D + SegmentVector * ClampedProjection;

	constexpr float HalfValue = 0.5f;
	const float JunctionZ = (AnchorLocationA.Z + AnchorLocationB.Z) * HalfValue;
	return FVector(ProjectedPoint2D.X, ProjectedPoint2D.Y, JunctionZ);
}
