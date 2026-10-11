// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

class USkeletalMeshComponent;

/**
 * @brief Flat rectangle a vehicle takes up on the ground, measured once from its chassis collision.
 * Avoidance reasons about these rectangles instead of formation radii, which are tighter than the hull.
 */
struct FRTSVehicleFootprint
{
	float HalfLength = 0.f;
	float HalfWidth = 0.f;
	// The collision is rarely centred on the vehicle's origin: X along the hull, Y to its right.
	FVector2D LocalCenter = FVector2D::ZeroVector;

	bool GetIsValid() const { return HalfLength > 0.f && HalfWidth > 0.f; }

	/** @return Radius of the circle through the corners; nothing of the hull lies outside it. */
	float GetCornerRadius() const { return FMath::Sqrt(FMath::Square(HalfLength) + FMath::Square(HalfWidth)); }

	/** @return How far the hull reaches from its centre towards a direction given in world space. */
	float GetReachAlong(const FVector2D& Forward, const FVector2D& Direction) const;

	/** @return Ground location of the middle of the hull for a vehicle at Location facing Forward. */
	FVector2D GetWorldCenter(const FVector2D& Location, const FVector2D& Forward) const;
};

namespace RTSVehicleFootprint
{
	/**
	 * @brief Measures the box around all collision bodies of a chassis.
	 * @param ChassisMesh Mesh whose physics asset is measured.
	 * @param MeshToSpace Identity for chassis space, or a scale to get real sizes.
	 * @return The box in the asked space; invalid while the mesh has no bodies yet.
	 */
	FBox GetChassisCollisionBox(const USkeletalMeshComponent* ChassisMesh, const FTransform& MeshToSpace);

	/** @return The footprint of the chassis collision, or an invalid footprint while the mesh has no bodies yet. */
	FRTSVehicleFootprint BuildFromChassisMesh(const USkeletalMeshComponent* ChassisMesh);

	/**
	 * @brief Tells how deep two vehicle rectangles are inside each other on the ground plane.
	 * @param CenterA World location of the first vehicle.
	 * @param ForwardA Normalized ground facing of the first vehicle.
	 * @param FootprintA Rectangle of the first vehicle.
	 * @param CenterB World location of the second vehicle.
	 * @param ForwardB Normalized ground facing of the second vehicle.
	 * @param FootprintB Rectangle of the second vehicle.
	 * @return Smallest distance one has to move to separate them; zero when they do not touch.
	 */
	float GetPenetrationDepth2D(
		const FVector2D& CenterA,
		const FVector2D& ForwardA,
		const FRTSVehicleFootprint& FootprintA,
		const FVector2D& CenterB,
		const FVector2D& ForwardB,
		const FRTSVehicleFootprint& FootprintB);

	/**
	 * @brief Finds how far a hull can drive straight on before it meets another hull, whatever way that one is turned.
	 * A hull that only touches the side of the path does not count: it is beside the vehicle, not ahead of it.
	 * @param Center Middle of the driving hull.
	 * @param DriveDirection Normalized direction it drives in: its facing, or the opposite while reversing.
	 * @param Footprint Rectangle of the driving hull.
	 * @param OtherCenter Middle of the other hull.
	 * @param OtherForward Normalized ground facing of the other hull.
	 * @param OtherFootprint Rectangle of the other hull.
	 * @param MaxDistance How far ahead to look.
	 * @return Free distance in front of the hull, or MaxDistance when the other hull is not in the way.
	 */
	float GetFreeTravelDistance(
		const FVector2D& Center,
		const FVector2D& DriveDirection,
		const FRTSVehicleFootprint& Footprint,
		const FVector2D& OtherCenter,
		const FVector2D& OtherForward,
		const FRTSVehicleFootprint& OtherFootprint,
		float MaxDistance);
}
