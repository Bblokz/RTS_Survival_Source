// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "PCGForestBiomeTypes.h"

/**
 * @brief A tree or bush entry with its clearance radius already resolved (measured bounds or
 * override), so the generator never touches UObjects and stays deterministic and thread-agnostic.
 */
struct FForestResolvedProp
{
	// Index into the matching settings array (LargeTrees / RegularTrees / Bushes).
	int32 SettingsIndex = INDEX_NONE;

	double Radius = 200.0;
	float Weight = 1.0f;
	bool bOverrideScale = false;
	float MinScale = 1.0f;
	float MaxScale = 1.0f;
};

/** @brief A foliage entry with its clearance radius resolved from the static mesh bounds. */
struct FForestResolvedFoliage
{
	// Index into the settings' Foliage array.
	int32 SettingsIndex = INDEX_NONE;

	double Radius = 50.0;
	float Weight = 1.0f;
	float MinScale = 0.8f;
	float MaxScale = 1.2f;
};

/**
 * @brief An auxiliary entry with its radius resolved and its size-scaled isolation distance
 * precomputed by the element, so the generator only has to compare distances.
 */
struct FForestResolvedAuxiliary
{
	// Index into the settings' Auxiliaries array.
	int32 SettingsIndex = INDEX_NONE;

	double Radius = 150.0;
	float Weight = 1.0f;
	EForestAuxiliarySize Size = EForestAuxiliarySize::Medium;

	// PreferredDistanceFromOtherAuxiliaries times the per-size multiplier (larger size = larger value).
	double IsolationDistance = 400.0;

	bool bOverrideScale = false;
	float MinScale = 1.0f;
	float MaxScale = 1.0f;
};

/** @brief One resolved placement in biome-local XY space; consumed by the element when spawning. */
struct FForestPlacement
{
	// Index into the matching resolved entry array (which category is implied by the result field).
	int32 EntryIndex = INDEX_NONE;

	// Actor / instance pivot in biome-local space (origin at the input area's bounds center).
	FVector2D Position = FVector2D::ZeroVector;
	double YawRadians = 0.0;
	double UniformScale = 1.0;
};

/** @brief Everything one generation run produced, grouped by category. */
struct FForestBiomeGenResult
{
	TArray<FForestPlacement> LargeTrees;
	TArray<FForestPlacement> RegularTrees;
	TArray<FForestPlacement> Bushes;
	TArray<FForestPlacement> Foliage;
	TArray<FForestPlacement> Auxiliaries;
};

/**
 * @brief Full parameter snapshot for one generation run. Filled by the PCG element from the node
 * settings and resolved assets; every length is in biome-local space (pivot at the origin).
 */
struct FForestBiomeGenParams
{
	// Derived by the element from the input area's bounds.
	double BiomeLengthX = 20000.0;
	double BiomeLengthY = 20000.0;
	int32 RandomSeed = 42;

	// Areal densities: average count per 1000 x 1000 unit (10 m x 10 m) tile of the biome area.
	double LargeTreesPer1000 = 0.5;
	double RegularTreesPer1000 = 2.0;
	double BushesPer1000 = 3.0;
	double FoliagePer1000 = 8.0;
	double AuxiliariesPer1000 = 1.0;
	double GlobalDensityMultiplier = 1.0;

	// Extra spacing (cm) kept between neighbouring trees/bushes on top of their clearance radii.
	double PropSpacing = 100.0;

	TArray<FForestResolvedProp> LargeTrees;
	TArray<FForestResolvedProp> RegularTrees;
	TArray<FForestResolvedProp> Bushes;
	TArray<FForestResolvedFoliage> Foliage;
	TArray<FForestResolvedAuxiliary> Auxiliaries;

	// Optional exclusion test in biome-local space (fed by the node's Exclusion input pin).
	TFunction<bool(const FVector2D&)> IsExcluded;

	// Optional area test in biome-local space (the input spawn volume / spline shape). When set,
	// scattering is trimmed to this shape instead of filling the whole bounds rectangle.
	TFunction<bool(const FVector2D&)> IsInsideArea;
};

/**
 * @brief Deterministic forest scatter generator. Places large trees, regular trees, bushes,
 * foliage and auxiliaries in that order into a shared occupancy grid, so later categories only
 * take space earlier ones left free. Consumes no UObjects; the element resolves assets before and
 * spawns actors / instances / points after.
 */
class FForestBiomeGenerator
{
public:
	explicit FForestBiomeGenerator(const FForestBiomeGenParams& InParams);

	void Generate(FForestBiomeGenResult& OutResult);

private:
	const FForestBiomeGenParams& M_Params;
	FRandomStream M_Random;
	FForestOccupancyGrid M_Occupancy;

	// Fraction (0..1) of the bounding rectangle that is actually spawnable (inside the area shape and
	// not excluded), estimated once by Monte Carlo. Makes densities count per tile of real forest
	// floor rather than per tile of the bounding box, so non-box volumes are not over-populated.
	double M_InsideFraction = 1.0;

	/** @brief Estimates M_InsideFraction by sampling the area and exclusion tests. */
	void ComputeInsideFraction();

	/** @return Target placement count for an areal density over the spawnable biome area. */
	int32 ComputeTargetCount(double DensityPer1000) const;

	/** @return Rejection-sampling attempt budget for a target, scaled up for small spawnable fractions. */
	int32 ComputeAttemptBudget(int32 TargetCount) const;

	/**
	 * @brief Scatters one solid-prop category (large trees, regular trees or bushes), reserving a
	 * circle for each placement so later categories avoid it.
	 * @param ReserveType Occupancy type stored for each placement.
	 * @param AvoidMask Mask of already-placed categories a candidate must not overlap.
	 */
	void ScatterProps(const TArray<FForestResolvedProp>& Entries, int32 TargetCount,
		EForestOccupancy ReserveType, uint8 AvoidMask, TArray<FForestPlacement>& OutPlacements);

	/** @brief Scatters groundcover foliage: avoids tree/bush trunks but never blocks other foliage. */
	void ScatterFoliage(int32 TargetCount, TArray<FForestPlacement>& OutPlacements);

	/** @brief Scatters auxiliaries largest-first so wider isolations are claimed before smaller props fill in. */
	void ScatterAuxiliaries(FForestBiomeGenResult& OutResult);
	void ScatterAuxiliaryEntry(int32 ResolvedIndex, int32 TargetCount, TArray<FForestPlacement>& OutPlacements);

	// --- Sampling helpers ---
	FVector2D RandomLocalPoint();
	bool IsAllowedPoint(const FVector2D& Point) const;
	double PickScale(bool bOverride, float MinScale, float MaxScale);
	int32 PickWeightedProp(const TArray<FForestResolvedProp>& Entries);
	int32 PickWeightedFoliage(const TArray<FForestResolvedFoliage>& Entries);
};
