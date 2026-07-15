// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "PCGEnemyBaseSetupTypes.h"

/**
 * @brief One Blueprint entry with its footprint already resolved (measured bounds or override),
 * so the placement library never touches UObjects and stays deterministic and thread-agnostic.
 * The element keeps parallel class / measured-local-bounds arrays indexed the same way.
 */
struct FEnemyResolvedEntry
{
	// Index into the owning settings array (used by the element to find the class to spawn).
	int32 SettingsIndex = INDEX_NONE;

	// Index into the category's resolved class / local-bounds arrays used for spawning and output.
	// Lets several placement lists (e.g. one per decorator profile) share one output asset array.
	int32 OutputAssetIndex = INDEX_NONE;

	FVector2D FootprintHalfExtents = FVector2D(200.0, 200.0);

	// Offset from the actor pivot to the footprint center, in the Blueprint's local space.
	FVector2D PivotToFootprintCenter = FVector2D::ZeroVector;

	float Weight = 1.0f;
	bool bOverrideScale = false;
	float MinScale = 1.0f;
	float MaxScale = 1.0f;
};

/**
 * @brief An oriented placement frame in world XY: an origin and the yaw the setup faces (toward
 * the threat). Every belt, wall and building yaw is derived from this frame.
 */
struct FEnemyDefenseFrame
{
	FVector2D Origin = FVector2D::ZeroVector;
	double FacingYaw = 0.0;

	/** @return Unit vector along the facing direction (toward the threat). */
	FVector2D Forward() const;

	/** @return Unit vector 90 degrees counter-clockwise from Forward (the setup's right/left axis). */
	FVector2D Right() const;
};

/** @return Mask of the solid categories every structural placement must stay clear of. */
constexpr uint8 EnemySolidCategoryMask()
{
	return EnemyOccupancyMask(EEnemyPlacementCategory::Bunker)
		| EnemyOccupancyMask(EEnemyPlacementCategory::Sandbag)
		| EnemyOccupancyMask(EEnemyPlacementCategory::Hedgehog)
		| EnemyOccupancyMask(EEnemyPlacementCategory::BarbedWire);
}

/**
 * @brief Deterministic, UObject-free placement engine shared by every enemy-base-setup node.
 * Every node resolves its assets, then drives this builder to lay bunkers, obstacle belts, barbed
 * wire, sandbag walls and decorator / foliage / decal scatter into a shared occupancy grid so
 * nothing overlaps. Positions are world XY; the element ground-projects and spawns afterwards.
 */
class FEnemyBaseSetupBuilder
{
public:
	FEnemyBaseSetupBuilder(
		FRandomStream& InRandom,
		FEnemyOccupancyGrid& InOccupancy,
		FEnemyBaseSetupResult& InResult);

	/** @brief Sets an optional world-XY test that rejects blocked positions (exclusion / outside area). */
	void SetBlockedPredicate(TFunction<bool(const FVector2D&)> InBlocked);

	/** @return A fresh cluster index; all props of one setup instance share it for per-cluster bounds. */
	int32 BeginCluster();

	/**
	 * @brief Lays a row of bunkers centered on the frame origin, spread along the frame's right
	 * axis and all facing the frame direction.
	 * @param LineLength Total length (cm) of the row along the right axis.
	 * @param Spacing Distance (cm) between consecutive bunker centers.
	 */
	void PlaceBunkerRow(
		const TArray<FEnemyResolvedEntry>& Bunkers,
		const FEnemyDefenseFrame& Frame,
		double LineLength,
		double Spacing,
		double ExtraSpacing,
		int32 Cluster);

	/**
	 * @brief Places one weighted building at the frame origin, facing the frame.
	 * @param OutBuildingFootprint Receives the reserved footprint (for sandbag walls) on success.
	 * @return True when a building fit and was reserved.
	 */
	bool PlaceSingleBuilding(
		const TArray<FEnemyResolvedEntry>& Buildings,
		const FEnemyDefenseFrame& Frame,
		double ExtraSpacing,
		int32 Cluster,
		FEnemyFootprint& OutBuildingFootprint);

	/**
	 * @brief Lays an anti-tank obstacle belt in front of a defended line.
	 * @param FrontDistance Distance (cm) from the frame origin to the line's front edge.
	 * @param HalfWidth Half the belt length (cm) along the right axis before side overhang.
	 */
	void PlaceObstacleBelt(
		const TArray<FEnemyResolvedEntry>& Obstacles,
		const FEnemyObstacleBeltSettings& Settings,
		const FEnemyDefenseFrame& Frame,
		double FrontDistance,
		double HalfWidth,
		int32 Cluster);

	/**
	 * @brief Lays one or more chained barbed-wire runs in front of a defended line.
	 * @param FrontDistance Distance (cm) from the frame origin to the line's front edge.
	 * @param HalfWidth Half the run length (cm) along the right axis before side overhang.
	 */
	void PlaceBarbedWire(
		const TArray<FEnemyResolvedEntry>& Wire,
		const FEnemyBarbedWireSettings& Settings,
		const FEnemyDefenseFrame& Frame,
		double FrontDistance,
		double HalfWidth,
		int32 Cluster);

	/** @brief Wraps a sandbag wall around the covered sides of a placed building footprint. */
	void PlaceSandbagWall(
		const TArray<FEnemyResolvedEntry>& Sandbags,
		const FEnemySandbagSettings& Settings,
		const FEnemyFootprint& Building,
		int32 Cluster);

	/** @brief Scatters decorator props (ammo, fuel...) in a ring around the cluster's combined bounds. */
	void ScatterDecorators(
		const TArray<FEnemyResolvedEntry>& Decorators,
		const FEnemyDecoratorProfile& Profile,
		int32 Cluster);

	/** @brief Emits foliage points in a ring around the cluster's combined bounds (no reservation). */
	void ScatterFoliage(const FEnemyFoliageSettings& Settings, int32 Cluster);

	/** @brief Emits decal spawns in a ring around the cluster's combined bounds (no reservation). */
	void ScatterDecals(const FEnemyDecalSettings& Settings, int32 Cluster);

	/** @return Axis-aligned union of every reserved footprint belonging to the cluster. */
	FBox2D ComputeClusterBounds(int32 Cluster) const;

private:
	FRandomStream& M_Random;
	FEnemyOccupancyGrid& M_Occupancy;
	FEnemyBaseSetupResult& M_Result;
	TFunction<bool(const FVector2D&)> M_IsBlocked;

	int32 PickWeightedEntry(const TArray<FEnemyResolvedEntry>& Entries) const;
	double PickUniformScale(const FEnemyResolvedEntry& Entry) const;

	/** @brief Builds the world-XY oriented footprint an entry reserves at a pose (pivot offset applied). */
	FEnemyFootprint MakeFootprint(
		const FEnemyResolvedEntry& Entry,
		const FVector2D& Position,
		double YawRadians,
		double Scale) const;

	/**
	 * @brief Reserves a weighted-picked entry at a pose when it is clear of AvoidMask and not blocked.
	 * @return True when the item was reserved and appended to the result.
	 */
	bool TryReserveItem(
		EEnemyPlacementCategory Category,
		const TArray<FEnemyResolvedEntry>& Entries,
		int32 EntryIndex,
		const FVector2D& Position,
		double YawRadians,
		double ExtraSpacing,
		uint8 AvoidMask,
		int32 Cluster);

	/** @brief Marches along a straight run and reserves a weighted item at each step, facing ItemYaw. */
	void PlaceChainAlong(
		EEnemyPlacementCategory Category,
		const TArray<FEnemyResolvedEntry>& Entries,
		const FVector2D& Center,
		const FVector2D& Direction,
		double HalfLength,
		double Step,
		double ItemYaw,
		double ExtraSpacing,
		uint8 AvoidMask,
		int32 Cluster);

	// --- Obstacle belt formations ---
	void PlaceStraightObstacleRow(
		const TArray<FEnemyResolvedEntry>& Obstacles,
		const FEnemyObstacleBeltSettings& Settings,
		const FEnemyDefenseFrame& Frame,
		double ForwardOffset,
		double HalfWidth,
		int32 Cluster);
	void PlaceArcObstacleRow(
		const TArray<FEnemyResolvedEntry>& Obstacles,
		const FEnemyObstacleBeltSettings& Settings,
		const FEnemyDefenseFrame& Frame,
		double ForwardOffset,
		double HalfWidth,
		int32 Cluster);

	// --- Sandbag sides ---
	void PlaceSandbagSide(
		const TArray<FEnemyResolvedEntry>& Sandbags,
		const FEnemySandbagSettings& Settings,
		const FEnemyFootprint& Building,
		const FVector2D& OutwardDir,
		double AlongExtent,
		double OutwardExtent,
		int32 Cluster);

	/** @return A random ring position around Bounds, at a distance in [MinDistance, MaxDistance] from its edge. */
	FVector2D SampleRingPosition(const FBox2D& Bounds, double MinDistance, double MaxDistance) const;
};
