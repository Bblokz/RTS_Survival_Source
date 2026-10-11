// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTSVehicleFootprint.h"

#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"

namespace RTSVehicleFootprintPrivate
{
	float GetProjectedHalfSize(
		const FVector2D& Forward,
		const FRTSVehicleFootprint& Footprint,
		const FVector2D& Axis)
	{
		const FVector2D Right(-Forward.Y, Forward.X);
		return Footprint.HalfLength * FMath::Abs(FVector2D::DotProduct(Forward, Axis)) +
			Footprint.HalfWidth * FMath::Abs(FVector2D::DotProduct(Right, Axis));
	}
}

float FRTSVehicleFootprint::GetReachAlong(const FVector2D& Forward, const FVector2D& Direction) const
{
	return RTSVehicleFootprintPrivate::GetProjectedHalfSize(Forward, *this, Direction);
}

FVector2D FRTSVehicleFootprint::GetWorldCenter(const FVector2D& Location, const FVector2D& Forward) const
{
	const FVector2D Right(-Forward.Y, Forward.X);
	return Location + Forward * LocalCenter.X + Right * LocalCenter.Y;
}

FBox RTSVehicleFootprint::GetChassisCollisionBox(
	const USkeletalMeshComponent* ChassisMesh,
	const FTransform& MeshToSpace)
{
	FBox CollisionBox(ForceInit);
	if (not IsValid(ChassisMesh))
	{
		return CollisionBox;
	}
	if (const UPhysicsAsset* PhysicsAsset = ChassisMesh->GetPhysicsAsset())
	{
		CollisionBox = PhysicsAsset->CalcAABB(ChassisMesh, MeshToSpace);
	}
	if (not CollisionBox.IsValid)
	{
		CollisionBox = ChassisMesh->CalcBounds(MeshToSpace).GetBox();
	}
	return CollisionBox;
}

FRTSVehicleFootprint RTSVehicleFootprint::BuildFromChassisMesh(const USkeletalMeshComponent* ChassisMesh)
{
	FRTSVehicleFootprint Footprint;
	if (not IsValid(ChassisMesh))
	{
		return Footprint;
	}

	const FTransform ScaleOnly(FQuat::Identity, FVector::ZeroVector, ChassisMesh->GetComponentScale());
	const FBox CollisionBox = GetChassisCollisionBox(ChassisMesh, ScaleOnly);
	const FVector Extent = CollisionBox.GetExtent();
	Footprint.HalfLength = Extent.X;
	Footprint.HalfWidth = Extent.Y;
	Footprint.LocalCenter = FVector2D(CollisionBox.GetCenter());
	return Footprint;
}

float RTSVehicleFootprint::GetFreeTravelDistance(
	const FVector2D& Center,
	const FVector2D& DriveDirection,
	const FRTSVehicleFootprint& Footprint,
	const FVector2D& OtherCenter,
	const FVector2D& OtherForward,
	const FRTSVehicleFootprint& OtherFootprint,
	const float MaxDistance)
{
	// Hulls that rest side by side overlap the path by a sliver; narrower than the hull keeps them out of it.
	constexpr float SideSliver = 20.f;
	constexpr int32 SearchSteps = 6;

	FRTSVehicleFootprint SweptFootprint = Footprint;
	SweptFootprint.HalfWidth = FMath::Max(Footprint.HalfWidth - SideSliver, 1.f);
	// The hull stretched forward by Distance is the ground it covers while driving that far.
	const auto GetIsOtherWithinDistance = [&](const float Distance)
	{
		SweptFootprint.HalfLength = Footprint.HalfLength + Distance * 0.5f;
		return GetPenetrationDepth2D(
			Center + DriveDirection * (Distance * 0.5f),
			DriveDirection,
			SweptFootprint,
			OtherCenter,
			OtherForward,
			OtherFootprint) > 0.f;
	};

	if (not GetIsOtherWithinDistance(MaxDistance))
	{
		return MaxDistance;
	}
	if (GetIsOtherWithinDistance(0.f))
	{
		// Already against it: it only stops the vehicle when its middle lies ahead.
		const bool bIsAhead = FVector2D::DotProduct(OtherCenter - Center, DriveDirection) > Footprint.HalfLength;
		return bIsAhead ? 0.f : MaxDistance;
	}

	float FreeDistance = 0.f;
	float BlockedDistance = MaxDistance;
	for (int32 Step = 0; Step < SearchSteps; ++Step)
	{
		const float TestDistance = (FreeDistance + BlockedDistance) * 0.5f;
		if (GetIsOtherWithinDistance(TestDistance))
		{
			BlockedDistance = TestDistance;
		}
		else
		{
			FreeDistance = TestDistance;
		}
	}
	return FreeDistance;
}

float RTSVehicleFootprint::GetPenetrationDepth2D(
	const FVector2D& CenterA,
	const FVector2D& ForwardA,
	const FRTSVehicleFootprint& FootprintA,
	const FVector2D& CenterB,
	const FVector2D& ForwardB,
	const FRTSVehicleFootprint& FootprintB)
{
	using namespace RTSVehicleFootprintPrivate;
	const FVector2D CenterDelta = CenterB - CenterA;
	const FVector2D SeparatingAxes[] = {
		ForwardA,
		FVector2D(-ForwardA.Y, ForwardA.X),
		ForwardB,
		FVector2D(-ForwardB.Y, ForwardB.X)
	};

	float SmallestOverlap = MAX_flt;
	for (const FVector2D& Axis : SeparatingAxes)
	{
		const float Overlap = GetProjectedHalfSize(ForwardA, FootprintA, Axis) +
			GetProjectedHalfSize(ForwardB, FootprintB, Axis) -
			FMath::Abs(FVector2D::DotProduct(CenterDelta, Axis));
		if (Overlap <= 0.f)
		{
			return 0.f;
		}
		SmallestOverlap = FMath::Min(SmallestOverlap, Overlap);
	}
	return SmallestOverlap;
}
