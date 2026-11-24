// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * @brief Material parameter values of one country look, read from a material instance at runtime.
 * Plain data so the liberation blend can be tested without a material.
 */
struct RTS_SURVIVAL_API FWorldCountryMaterialParameterSet
{
	TMap<FName, FLinearColor> VectorValues;
	TMap<FName, float> ScalarValues;

	bool GetIsEmpty() const { return VectorValues.Num() == 0 && ScalarValues.Num() == 0; }
};

/**
 * @brief What one anchor inside a country means for the country's liberation value.
 */
struct RTS_SURVIVAL_API FWorldCountryAnchorTargetInput
{
	/** A mission or an enemy item (the enemy HQ and the wall included), or any anchor the player cleared. */
	bool bIsTarget = false;

	/** A pinned story mission; weighs more than other targets. */
	bool bIsStoryMission = false;

	bool bIsCleared = false;
};

/**
 * @brief Target counts of one country, split the way WorldCampaignRules::GetCountryLiberation expects them.
 */
struct RTS_SURVIVAL_API FWorldCountryTargetTally
{
	int32 StoryMissionsTotal = 0;
	int32 StoryMissionsCleared = 0;
	int32 OtherTargetsTotal = 0;
	int32 OtherTargetsCleared = 0;

	int32 GetTotalTargets() const { return StoryMissionsTotal + OtherTargetsTotal; }
	int32 GetClearedTargets() const { return StoryMissionsCleared + OtherTargetsCleared; }
};

/**
 * @brief One triangle of a country fill mesh flattened to world XY, with its XY bounds for a cheap reject.
 */
struct RTS_SURVIVAL_API FWorldCountryTriangle2D
{
	FVector2D A = FVector2D::ZeroVector;
	FVector2D B = FVector2D::ZeroVector;
	FVector2D C = FVector2D::ZeroVector;
	FBox2D Bounds = FBox2D(ForceInit);
};

/**
 * @brief Country liberation helpers as pure functions so counting, membership and the look blend are unit tested.
 */
namespace WorldCountryLiberationRules
{
	/**
	 * @brief Builds a flattened triangle; degenerate (zero-area) triangles are refused.
	 * @param OutTriangle Receives the triangle when it has area.
	 * @return false for a degenerate triangle.
	 */
	RTS_SURVIVAL_API bool TryMakeCountryTriangle2D(const FVector2D& A, const FVector2D& B, const FVector2D& C,
	                                               FWorldCountryTriangle2D& OutTriangle);

	/** @return true when the point lies inside or on the edge of the triangle; works for either winding. */
	RTS_SURVIVAL_API bool GetIsPointInTriangle2D(const FVector2D& Point, const FWorldCountryTriangle2D& Triangle);

	/** @return true when the point lies in any of the country's triangles. */
	RTS_SURVIVAL_API bool GetIsPointInCountryTriangles(const FVector2D& Point,
	                                                   const TArray<FWorldCountryTriangle2D>& CountryTriangles);

	/**
	 * @brief Counts a country's targets; anchors that are not targets are ignored.
	 * @param AnchorInputs One entry per anchor mapped to the country.
	 * @return Totals and cleared counts for story missions and other targets.
	 */
	RTS_SURVIVAL_API FWorldCountryTargetTally TallyCountryTargets(const TArray<FWorldCountryAnchorTargetInput>& AnchorInputs);

	/**
	 * @brief Blends the occupied look toward the liberated look. Only parameters present in both sets are blended,
	 * so a parameter missing from either material instance is left alone.
	 * @param OccupiedSet Values of the Soviet (occupied) instance.
	 * @param LiberatedSet Values of the Resistance (liberated) instance.
	 * @param Liberation 0 = fully occupied, 1 = fully liberated; clamped.
	 * @return The blended values to push into the country's dynamic material instance.
	 */
	RTS_SURVIVAL_API FWorldCountryMaterialParameterSet BlendParameterSets(const FWorldCountryMaterialParameterSet& OccupiedSet,
	                                                                      const FWorldCountryMaterialParameterSet& LiberatedSet,
	                                                                      float Liberation);
}
