// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#pragma once

#include "PCGSettings.h"
#include "PCGContext.h"
#include "PCGScorchedCityTypes.h"
#include "PCGScorchedCity.generated.h"

class UStaticMesh;

/**
 * @brief Settings for the ScorchedCity node. Generates a scorched city inside the input
 * spatial area (typically a closed spline loop): mixed grid/curved road network, bounds-aware
 * building placement on generated lots, power lines along roads and scatter debris around
 * buildings. Everything placed is tested with oriented 2D footprints (real bounds or
 * overrides) so nothing overlaps unless allowed. The city size derives from the area's
 * bounds and the layout is trimmed to the shape (a good-enough fill, not an exact packing).
 * @note Roads, intersections, buildings and power lines are spawned as PCG-managed actors.
 * @note Scatter is emitted as points with a "Mesh" attribute: wire the Scatter pin into a
 * Static Mesh Spawner with mesh selection "By Attribute".
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UPCGScorchedCitySettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPCGScorchedCitySettings();

	// ~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spawner; }
#endif

	/** @brief The city is deterministic from the RandomSeed property, not the PCG seed. */
	virtual bool UseSeed() const override { return false; }

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	// ~End UPCGSettings interface

public:
	// --- City ---
	// The city rectangle is derived from the input area's bounds; its shape trims the layout.

	/** @brief All placement, rotation and selection derives deterministically from this seed. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "City")
	int32 RandomSeed = 42;

	/** @brief 0..1; scales how many lots receive a building and how much scatter appears. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "City", meta = (ClampMin = "0", ClampMax = "1"))
	float OverallDensity = 0.7f;

	/** @brief Relative amount of square/grid city zones (ratio against CurvedRoadAmount). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "City", meta = (ClampMin = "0"))
	float GridLayoutAmount = 0.5f;

	/** @brief Relative amount of curved/organic road zones (ratio against GridLayoutAmount). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "City", meta = (ClampMin = "0"))
	float CurvedRoadAmount = 0.5f;

	// --- Roads ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roads")
	TSoftObjectPtr<UStaticMesh> RoadMesh;

	/** @brief 4-way road section placed at intersections with 4+ connections. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roads")
	TSoftObjectPtr<UStaticMesh> IntersectionMesh;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roads", meta = (ClampMin = "100"))
	float RoadWidth = 500.0f;

	/** @brief Vertical lift applied after ground snapping roads and intersections. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roads")
	float RoadZOffset = 10.0f;

	/** @brief Sidewalk/setback distance between road edge and anything placed beside it. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roads", meta = (ClampMin = "0"))
	float RoadSetback = 200.0f;

	/** @brief Extra minimum distance between roads and building footprints. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roads", meta = (ClampMin = "0"))
	float MinRoadBuildingDistance = 100.0f;

	/** @brief Street spacing of grid zones; also the macro cell size used for zoning. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roads", meta = (ClampMin = "1000"))
	float GridBlockSize = 3200.0f;

	/** @brief Every Nth grid street becomes a major road (bigger lots, larger buildings). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roads", meta = (ClampMin = "1"))
	int32 MajorRoadInterval = 3;

	/** @brief 0..1; how sharply curved roads may turn per segment. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roads", meta = (ClampMin = "0", ClampMax = "1"))
	float CurvedRoadCurvature = 0.5f;

	/** @brief 0..1; length/heading variation of curved road segments and branches. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roads", meta = (ClampMin = "0", ClampMax = "1"))
	float CurvedRoadVariation = 0.5f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roads", meta = (ClampMin = "0", ClampMax = "1"))
	float CurvedRoadBranchChance = 0.25f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roads", meta = (ClampMin = "200"))
	float CurvedRoadSegmentLength = 900.0f;

	/** @brief 0 = derive from the road mesh bounds (its local X length). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roads", meta = (ClampMin = "0"))
	float RoadMeshLengthOverride = 0.0f;

	/** @brief 0 = derive from the intersection mesh bounds. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Roads", meta = (ClampMin = "0"))
	float IntersectionSizeOverride = 0.0f;

	// --- Buildings ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Buildings")
	TArray<FScorchedBuildingAssetSettings> Buildings;

	/** @brief Extra clearance added around every building footprint before overlap tests. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Buildings", meta = (ClampMin = "0"))
	float BuildingSpacingExtra = 150.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Buildings", meta = (ClampMin = "400"))
	float LotWidth = 1600.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Buildings", meta = (ClampMin = "400"))
	float LotDepth = 2000.0f;

	// --- Scatter & power lines ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Scatter")
	TArray<FScorchedScatterProfile> ScatterProfiles;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "PowerLines")
	FScorchedPowerLineSettings PowerLineSettings;

	// --- Decals ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Decals|Lot Decals")
	FScorchedDecalPlacementSettings LotDecals;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Decals|Building Footprint Decals")
	FScorchedDecalPlacementSettings BuildingFootprintDecals;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Decals|Road Decals")
	FScorchedDecalPlacementSettings RoadDecals;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Decals|Destroyed Power Line Decals")
	FScorchedDecalPlacementSettings DestroyedPowerLineDecals;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Decals|Power Line Decals")
	FScorchedDecalPlacementSettings PowerLineDecals;
};

/**
 * @brief Execution element for ScorchedCity. Resolves assets and their real bounds, runs the
 * deterministic layout generator, spawns roads/intersections/buildings/poles as managed
 * actors and emits Scatter, OccupiedBounds and Lots point data.
 */
class FPCGScorchedCityElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

	/** @brief Spawns actors and measures Blueprint bounds; must run on the game thread. */
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }

	/** @brief Spawning actors is a side effect; results must never come from the cache. */
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
};
