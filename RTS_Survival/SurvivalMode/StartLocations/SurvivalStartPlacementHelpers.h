// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

class ALandscapeProxy;
class UWorld;
struct FSurvivalWeightedActorClass;
struct FSurvivalWeightedMesh;

/** @brief A landscape hit: where the ground is and which way it faces. */
struct FSurvivalGroundHit
{
	FVector Location = FVector::ZeroVector;
	FVector Normal = FVector::UpVector;
};

/** @brief A horizontal circle that other start content must stay out of. */
struct FSurvivalReservedCircle
{
	FVector2D Center = FVector2D::ZeroVector;
	float Radius = 0.f;
};

/**
 * @brief Projects XY positions onto the landscape only (never onto rocks, buildings or spawned actors), the same
 * way the campaign's radixite/metal PCG nodes do. Gathers the landscape proxies once per planning pass.
 */
class FSurvivalGroundProjector
{
public:
	FSurvivalGroundProjector(UWorld& World, const float TraceUp, const float TraceDown);

	/** @return False when no landscape covers this XY (outside the map). */
	bool TryProject(const FVector2D& Position, FSurvivalGroundHit& OutHit) const;

	/** @return False when no landscape covers this XY; cheaper than TryProject (no trace, no normal). */
	bool TryGetHeight(const FVector2D& Position, float& OutHeight) const;

	bool HasLandscape() const { return not M_Landscapes.IsEmpty(); }

	static float GetSlopeDegrees(const FVector& Normal);

private:
	TArray<TWeakObjectPtr<ALandscapeProxy>> M_Landscapes;
	float M_TraceUp = 20000.f;
	float M_TraceDown = 50000.f;
};

/**
 * @brief The circles placed content occupies; every placement asks it whether a footprint is still free.
 */
class FSurvivalOccupancy
{
public:
	void Add(const FVector2D& Center, const float Radius);

	/** @return True when a circle of Radius at Center keeps Clearance from every occupied circle. */
	bool IsFree(const FVector2D& Center, const float Radius, const float Clearance = 0.f) const;

	const TArray<FSurvivalReservedCircle>& GetCircles() const { return M_Circles; }

private:
	TArray<FSurvivalReservedCircle> M_Circles;
};

namespace SurvivalStartPlacement
{
	/** @return Index of a weighted pick from entries with a valid soft class and weight > 0; INDEX_NONE if none. */
	int32 PickWeightedActorIndex(const TArray<FSurvivalWeightedActorClass>& Entries, FRandomStream& Random);

	/** @return Index of a weighted pick from entries with a mesh and weight > 0; INDEX_NONE if none. */
	int32 PickWeightedMeshIndex(const TArray<FSurvivalWeightedMesh>& Entries, FRandomStream& Random);

	/** @return A point uniformly distributed (by area) in the ring between MinRadius and MaxRadius. */
	FVector2D RandomPointInAnnulus(
		const FVector2D& Center,
		const float MinRadius,
		const float MaxRadius,
		FRandomStream& Random);

	/**
	 * @brief Builds the spawn rotation: upright or tilted to the ground normal, with a yaw.
	 * @param GroundNormal Landscape normal at the spawn location.
	 * @param YawDegrees World yaw of the actor's forward axis.
	 * @param bAlignToGround Tilt to the normal (wrecks) instead of staying upright (buildings, pits).
	 */
	FQuat MakeGroundRotation(const FVector& GroundNormal, const float YawDegrees, const bool bAlignToGround);
}
