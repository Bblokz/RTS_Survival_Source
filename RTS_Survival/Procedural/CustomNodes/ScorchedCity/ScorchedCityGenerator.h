// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "PCGScorchedCityTypes.h"

/**
 * @brief Building asset with its footprint already resolved (measured bounds or override),
 * so the generator never touches UObjects and stays deterministic and thread-agnostic.
 */
struct FScorchedResolvedBuilding
{
	// Index into the settings' Buildings array (used by the element to find the class to spawn).
	int32 SettingsIndex = INDEX_NONE;

	FVector2D FootprintHalfExtents = FVector2D(400.0, 400.0);

	// Offset from the actor pivot to the footprint center, in the building's local space.
	FVector2D PivotToFootprintCenter = FVector2D::ZeroVector;

	float Weight = 1.0f;
	float MinSpacing = 0.0f;
	float MaxSpacing = 300.0f;
	EScorchedBuildingRotationMode RotationMode = EScorchedBuildingRotationMode::FaceRoad;
	EScorchedBuildingSize SizeCategory = EScorchedBuildingSize::Medium;
	EScorchedZonePreference ZonePreference = EScorchedZonePreference::None;
};

/**
 * @brief Full parameter snapshot for one generation run. Filled by the PCG element from the
 * node settings and resolved assets; everything is in city-local space (pivot at origin).
 */
struct FScorchedCityGenParams
{
	// Derived by the element from the input area's bounds (no longer designer settings).
	double CityLengthX = 20000.0;
	double CityLengthY = 20000.0;
	int32 RandomSeed = 42;

	// 0..1; scales how many lots actually receive a building.
	double OverallDensity = 0.7;
	double GridLayoutAmount = 0.5;
	double CurvedRoadAmount = 0.5;

	double BuildingSpacingExtra = 150.0;
	double RoadWidth = 500.0;
	// Vertical lift applied after ground snapping roads and intersections.
	double RoadZOffset = 10.0;
	// Sidewalk / setback between road edge and the closest allowed building face.
	double RoadSetback = 200.0;
	double MinRoadBuildingDistance = 100.0;

	double GridBlockSize = 3200.0;
	// Every Nth grid line becomes a major road.
	int32 MajorRoadInterval = 3;

	// 0..1 mapped to a max turn per segment for curved road walkers.
	double CurvedRoadCurvature = 0.5;
	double CurvedRoadVariation = 0.5;
	double CurvedRoadBranchChance = 0.25;
	double CurvedRoadSegmentLength = 900.0;

	double LotWidth = 1600.0;
	double LotDepth = 2000.0;

	// Length of one road mesh chunk; road occupancy is rasterized at this granularity.
	double RoadMeshLength = 400.0;
	// Real footprint of the 4-way intersection asset along its local X and Y; the asset may be
	// non-square, so road splines are trimmed per exit direction against the matching axis.
	double IntersectionSizeX = 700.0;
	double IntersectionSizeY = 700.0;

	TArray<FScorchedResolvedBuilding> Buildings;

	TArray<FScorchedScatterProfile> ScatterProfiles;
	// Parallel to ScatterProfiles: approximate XY radius per mesh entry, for overlap checks.
	TArray<TArray<double>> ScatterMeshRadii;

	FScorchedPowerLineSettings PowerLines;
	FScorchedDecalPlacementSettings LotDecals;
	FScorchedDecalPlacementSettings BuildingFootprintDecals;
	FScorchedDecalPlacementSettings RoadDecals;
	FScorchedDecalPlacementSettings DestroyedPowerLineDecals;
	FScorchedDecalPlacementSettings PowerLineDecals;

	// Optional exclusion test in city-local space (fed by the node's Exclusion input pin).
	TFunction<bool(const FVector2D&)> IsExcluded;

	// Optional area test in city-local space (the input spline loop / spatial shape). When set,
	// generation is trimmed to this shape instead of filling the whole bounds rectangle;
	// the fill is a good-enough approximation, not an exact packing of the shape.
	TFunction<bool(const FVector2D&)> IsInsideArea;
};

/**
 * @brief Deterministic scorched-city layout generator: zones, grid + curved road network,
 * lots, bounds-aware building/pole placement and scatter candidates. Consumes no UObjects;
 * the PCG element resolves assets before and spawns actors after.
 */
class FScorchedCityGenerator
{
public:
	explicit FScorchedCityGenerator(const FScorchedCityGenParams& InParams);

	void Generate(FScorchedCityGenResult& OutResult);

private:
	const FScorchedCityGenParams& M_Params;
	FRandomStream M_Random;
	FScorchedSpatialHashGrid M_Occupancy;
	FScorchedSpatialHashGrid M_DecalOccupancy;

	// Zone flags per macro cell (key = cell coordinates on the GridBlockSize lattice).
	TMap<FIntPoint, uint8> M_ZoneFlags;
	FIntPoint M_ZoneCellMin = FIntPoint::ZeroValue;
	FIntPoint M_ZoneCellMax = FIntPoint::ZeroValue;

	TArray<FScorchedRoadNode> M_Nodes;
	TArray<FScorchedRoadEdge> M_Edges;
	TArray<FScorchedLot> M_Lots;

	// --- Zoning ---
	void BuildZones();
	bool IsGridCell(const FIntPoint& Cell) const;
	bool IsDenseAt(const FVector2D& Position) const;
	bool IsGridZoneAt(const FVector2D& Position) const;

	// --- Road network ---
	void BuildGridRoads();
	void BuildCurvedRoads();
	void GrowCurvedRoad(int32 StartNodeIndex, double StartHeading, int32 Depth, int32& WalkerBudget);
	int32 FindOrAddNode(const FVector2D& Position, double SnapRadius, bool bGridNode);
	int32 FindNearestNode(const FVector2D& Position, double SearchRadius) const;
	int32 CommitEdge(int32 NodeAIndex, int32 NodeBIndex, TArray<FVector2D>&& Points, bool bCurved, bool bMajor);
	void RasterizeSegment(const FVector2D& From, const FVector2D& To);
	void AddIntersectionFootprints();
	bool IsSegmentBlockedByRoads(const FVector2D& From, const FVector2D& To) const;
	bool IsConnectionSegmentBlockedByRoads(const FVector2D& From, const FVector2D& To) const;

	/**
	 * @brief Replaces degree-2 corners of two perpendicular straight streets with a curved
	 * fillet edge, so rim streets bend naturally into their flanking roads instead of butting.
	 */
	void SmoothCornerNodes();

	/**
	 * @brief Pairs nearby degree-1 road endings with natural curves where the path is clear;
	 * unconnectable endings at the city rim are exported as OuterOrphanRoads.
	 */
	void ConnectOrphanRoadEnds(FScorchedCityGenResult& OutResult);

	/** @return Normalized direction from the node into the edge's polyline. */
	FVector2D EdgeDirectionAwayFromNode(const FScorchedRoadEdge& Edge, int32 NodeIndex) const;

	/** @brief Moves one endpoint of an edge onto a different node (used by corner fillets). */
	void RetargetEdgeEndpoint(int32 EdgeIndex, int32 OldNodeIndex, int32 NewNodeIndex,
		const FVector2D& NewEndpointPosition);

	/** @return Yaw (radians) the intersection mesh uses at this node; footprints/trims match it. */
	double ComputeIntersectionYawAtNode(int32 NodeIndex) const;

	/**
	 * @brief How far the intersection piece at this edge end reaches along the edge; 0 when the
	 * node has no intersection. Uncapped: callers placing content must cap it per edge so large
	 * 4-way assets can never consume entire streets (which would silence buildings/power lines).
	 * @return Support of the (possibly non-square) intersection footprint along the exit direction.
	 */
	double ComputeEdgeEndClearance(const FScorchedRoadEdge& Edge, int32 NodeIndex) const;

	double IntersectionMaxHalfExtent() const
	{
		return 0.5 * FMath::Max(M_Params.IntersectionSizeX, M_Params.IntersectionSizeY);
	}

	/** @brief Road-overlap test along a polyline, ignoring stretches near both endpoints. */
	bool IsPolylineBlockedByRoads(const TArray<FVector2D>& Points, double EndpointIgnoreDistance) const;

	// --- Lots & buildings ---
	void BuildLots();
	void BuildLotsAlongEdge(int32 EdgeIndex);
	void TryCreateLot(int32 EdgeIndex, const FVector2D& RoadPoint, const FVector2D& RoadDirection, double Side);
	void PlaceBuildings(FScorchedCityGenResult& OutResult);
	bool TryPlaceBuildingOnLot(const FScorchedLot& Lot, const FScorchedResolvedBuilding& Building,
		FScorchedBuildingSpawn& OutSpawn);
	double ComputeBuildingWeightForLot(const FScorchedResolvedBuilding& Building, const FScorchedLot& Lot) const;
	int32 PickWeightedBuildingForLot(const FScorchedLot& Lot);

	// --- Power lines & scatter ---
	void PlacePowerLines(FScorchedCityGenResult& OutResult);
	void PlacePowerLinesAlongEdge(const FScorchedRoadEdge& Edge, FScorchedCityGenResult& OutResult);
	void BuildScatter(const FScorchedCityGenResult& ResultSoFar, FScorchedCityGenResult& OutResult);
	void BuildScatterForBuilding(const FScorchedBuildingSpawn& Building, int32 ProfileIndex,
		FScorchedCityGenResult& OutResult);
	void BuildDecals(FScorchedCityGenResult& OutResult);
	void BuildLotDecals(FScorchedCityGenResult& OutResult);
	void BuildBuildingFootprintDecals(FScorchedCityGenResult& OutResult);
	void BuildRoadDecals(FScorchedCityGenResult& OutResult);
	void BuildPowerLineDecals(FScorchedCityGenResult& OutResult);
	void BuildDecalsInFootprint(
		const FScorchedDecalPlacementSettings& Settings,
		EScorchedDecalSet Set,
		const FScorchedFootprint& Area,
		double UnitCount,
		double BaseYawRadians,
		FScorchedCityGenResult& OutResult);
	void BuildDecalsAtPoint(
		const FScorchedDecalPlacementSettings& Settings,
		EScorchedDecalSet Set,
		const FVector2D& Position,
		double BaseYawRadians,
		FScorchedCityGenResult& OutResult);
	void BuildRoadSplineDecals(
		const FScorchedDecalPlacementSettings& Settings,
		const FScorchedRoadSplineResult& Road,
		FScorchedCityGenResult& OutResult);
	int32 PickWeightedDecalEntry(const FScorchedDecalPlacementSettings& Settings);
	int32 ComputeDecalCount(double Density, double UnitCount);
	bool TryReserveDecal(
		const FScorchedDecalPlacementSettings& Settings,
		EScorchedDecalSet Set,
		int32 EntryIndex,
		const FVector2D& Position,
		double YawRadians,
		double Size,
		FScorchedCityGenResult& OutResult);

	// --- Export & helpers ---
	void ExportRoads(FScorchedCityGenResult& OutResult) const;
	bool IsInsideCity(const FVector2D& Position, double Margin) const;
	bool IsFootprintInsideCity(const FScorchedFootprint& Footprint, double Margin) const;
	bool IsExcludedAt(const FVector2D& Position) const;

	/** @return Deterministic 0..1 value from the seed and integer cell coordinates. */
	double CellNoise(int32 Salt, const FIntPoint& Cell) const;

	/**
	 * @brief Walks a polyline by arc length.
	 * @return False when Distance is beyond the end of the polyline.
	 */
	static bool SamplePolyline(const TArray<FVector2D>& Points, double Distance,
		FVector2D& OutPosition, FVector2D& OutDirection);
};
