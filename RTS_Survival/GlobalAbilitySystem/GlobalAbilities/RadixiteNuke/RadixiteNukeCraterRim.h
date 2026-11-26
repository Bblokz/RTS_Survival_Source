// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

class ALandscapeProxy;
class UProceduralMeshComponent;
class UWorld;
struct FRadixiteNukeCrater;

/**
 * @brief Ground height for blast effects: the landscape's own heightfield where there is one, else the height of
 * ground zero. Never a trace, so wrecks, buildings and units under the blast do not lift the crater.
 */
class FRadixiteNukeGroundSampler
{
public:
	FRadixiteNukeGroundSampler(const UWorld& World, const FVector& GroundZero);

	float GetGroundHeight(const FVector2D& WorldXY) const;

private:
	TArray<TWeakObjectPtr<ALandscapeProxy>> M_Landscapes;
	float M_FallbackHeight = 0.f;
};

/**
 * @brief Builds the raised earth ring of a crater as one procedural mesh section around the component's location:
 * a polar grid whose every vertex sits on the ground sampled under it, so the rim follows hills, with skirts below the
 * ground on both feet. Called once by ARadixiteNuke at the blast.
 */
namespace RadixiteNukeCraterRim
{
	/**
	 * @brief Fills section 0 of the rim component; seeds its wobble from ground zero so a replay looks the same.
	 * @param RimComponent Component at ground zero, unrotated and unscaled.
	 * @param Crater Shape of the rim.
	 * @param Ground Height lookup under every vertex.
	 * @param bCreateCollision Cooks the mesh as collision, for the vehicle ramp.
	 */
	void BuildRimMesh(UProceduralMeshComponent& RimComponent, const FRadixiteNukeCrater& Crater,
	                  const FRadixiteNukeGroundSampler& Ground, const bool bCreateCollision);
}
