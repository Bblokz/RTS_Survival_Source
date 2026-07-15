// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnemyBaseSetupPlacement.h"
#include "PCGEnemyBaseSetupTypes.h"

struct FPCGContext;
class UMaterialInterface;
class UWorld;

/**
 * @brief Resolved spawn assets for one placement category: the Blueprint classes to spawn and the
 * measured local bounds for the point-with-bounds outputs, indexed by FEnemyPlacedActor::AssetIndex.
 */
struct FEnemyCategoryAssets
{
	TArray<UClass*> Classes;
	TArray<FBox> LocalBounds;
};

/** @brief All runtime-resolved assets one node needs to spawn its result and emit its outputs. */
struct FEnemyBaseSetupAssets
{
	FEnemyCategoryAssets Categories[static_cast<int32>(EEnemyPlacementCategory::Count)];
	TArray<FSoftObjectPath> FoliageMeshPaths;
	TArray<UMaterialInterface*> DecalMaterials;

	FEnemyCategoryAssets& Category(const EEnemyPlacementCategory InCategory)
	{
		return Categories[static_cast<int32>(InCategory)];
	}
	const FEnemyCategoryAssets& Category(const EEnemyPlacementCategory InCategory) const
	{
		return Categories[static_cast<int32>(InCategory)];
	}
};

/**
 * @brief Shared UObject-side helpers for every enemy-base-setup node: asset resolution with real
 * bounds measurement, input / aim-target reading, facing resolution, managed actor spawning and
 * the categorized point-with-bounds / total-bounds / occupancy / foliage / decal outputs. Keeping
 * this here lets each node's element stay a thin wrapper around the shared placement builder.
 */
namespace EnemyBaseSetupShared
{
	// --- Common pin and attribute names shared by all setup nodes ---
	namespace Pins
	{
		// Common outputs.
		extern const FName TotalBounds;
		extern const FName Bunkers;
		extern const FName Sandbags;
		extern const FName Obstacles;
		extern const FName BarbedWire;
		extern const FName Decorators;
		extern const FName Foliage;
		extern const FName OccupiedBounds;

		// Common inputs.
		extern const FName AimTarget;
		extern const FName Exclusion;

		// Attributes.
		extern const FName AssetIndexAttr;
		extern const FName ClusterAttr;
		extern const FName CategoryAttr;
		extern const FName MeshAttr;
	}

	/** @brief One world-space input point: its XY position and the yaw of its own rotation. */
	struct FEnemyInputPoint
	{
		FVector2D Position = FVector2D::ZeroVector;
		double YawRadians = 0.0;
	};

	// --- Asset resolution ---

	/**
	 * @brief Measures each Blueprint entry (real bounds unless overridden) and appends its class and
	 * local bounds to InOutAssets, filling OutEntries with placement entries whose OutputAssetIndex
	 * points back at the appended slot. Append semantics let several lists share one output array.
	 */
	void ResolveBlueprintList(
		FPCGContext* Context,
		UWorld& World,
		const TArray<FEnemyBlueprintEntry>& List,
		FEnemyCategoryAssets& InOutAssets,
		TArray<FEnemyResolvedEntry>& OutEntries);

	void ResolveFoliage(const TArray<FEnemyFoliageEntry>& List, TArray<FSoftObjectPath>& OutPaths);
	void ResolveDecals(const TArray<FEnemyDecalEntry>& List, TArray<UMaterialInterface*>& OutMaterials);

	// --- Inputs ---

	TArray<FEnemyInputPoint> CollectInputPoints(FPCGContext* Context, FName Pin);

	/** @return World-XY centers of every aim target on the pin (point positions, or spatial bounds centers). */
	TArray<FVector2D> CollectAimTargets(FPCGContext* Context, FName Pin);

	TArray<FBox> CollectSpatialBounds(FPCGContext* Context, FName Pin);

	/** @return A world-XY predicate that is true where the pin's spatial inputs sample density > 0. */
	TFunction<bool(const FVector2D&)> BuildExclusionPredicate(FPCGContext* Context, FName Pin);

	/**
	 * @brief Resolves the world yaw a setup faces from its facing mode.
	 * @param PointYaw The input point's own rotation yaw (used by UsePointRotation).
	 * @param AimTargets Candidate targets; the nearest is used by the Aim modes.
	 */
	double ComputeFacingYaw(
		EEnemyFacingMode Mode,
		const FVector2D& Origin,
		double PointYaw,
		double FixedYawDegrees,
		const TArray<FVector2D>& AimTargets);

	// --- Spawning (managed actors) ---

	void SpawnSetupActors(
		UWorld& World,
		const FEnemyBaseSetupResult& Result,
		const FEnemyBaseSetupAssets& Assets,
		TArray<AActor*>& OutSpawnedActors);

	void SpawnDecalsActor(
		UWorld& World,
		const FEnemyBaseSetupResult& Result,
		const FEnemyBaseSetupAssets& Assets,
		TArray<AActor*>& OutSpawnedActors);

	// --- Outputs ---

	/** @brief One ground-projected point per placed actor of Category, its bounds the measured local bounds. */
	void EmitCategoryOutput(
		FPCGContext* Context,
		UWorld& World,
		const FEnemyBaseSetupResult& Result,
		const FEnemyBaseSetupAssets& Assets,
		EEnemyPlacementCategory Category,
		FName PinLabel,
		int32 RandomSeed,
		const TArray<AActor*>& ActorsToIgnore);

	/** @brief One point per cluster whose bounds enclose every reserved footprint of that cluster. */
	void EmitTotalBoundsOutput(
		FPCGContext* Context,
		UWorld& World,
		const FEnemyBaseSetupResult& Result,
		FName PinLabel,
		int32 RandomSeed,
		const TArray<AActor*>& ActorsToIgnore);

	/** @brief One oriented point-with-bounds per reserved footprint (all categories), with a Category attribute. */
	void EmitOccupiedBoundsOutput(
		FPCGContext* Context,
		UWorld& World,
		const FEnemyBaseSetupResult& Result,
		FName PinLabel,
		int32 RandomSeed,
		const TArray<AActor*>& ActorsToIgnore);

	/** @brief Foliage points with a "Mesh" soft-path attribute for a Static Mesh Spawner (By Attribute). */
	void EmitFoliageOutput(
		FPCGContext* Context,
		UWorld& World,
		const FEnemyBaseSetupResult& Result,
		const FEnemyBaseSetupAssets& Assets,
		FName PinLabel,
		int32 RandomSeed,
		const TArray<AActor*>& ActorsToIgnore);

	/** @brief Registers every spawned actor with the PCG component so regeneration cleans them up. */
	void RegisterManagedActors(FPCGContext* Context, const TArray<AActor*>& SpawnedActors);
}
