// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPtr.h"
#include "PCGScorchedCityTypes.generated.h"

class AActor;
class UMaterialInterface;
class UStaticMesh;

// ---------------------------------------------------------------------------
// Designer-facing settings types.
// ---------------------------------------------------------------------------

UENUM(BlueprintType)
enum class EScorchedBuildingSize : uint8
{
	Small,
	Medium,
	Large
};

/** @brief Where a building asset prefers to be placed; biases the weighted selection per lot. */
UENUM(BlueprintType)
enum class EScorchedZonePreference : uint8
{
	None,
	GridRoads,
	CurvedRoads,
	Corners,
	DenseZones,
	SparseZones
};

/** @brief How a building entry is turned into world geometry. */
UENUM(BlueprintType)
enum class EScorchedBuildingCategory : uint8
{
	// Scorch ruin made of many small static mesh components: all placements of these are
	// batched into ONE actor of shared Hierarchical Instanced Static Mesh components.
	// Big optimization; any non-mesh components/logic of the Blueprint are discarded.
	ScorchBuilding,
	// Spawned as its Blueprint actor, unchanged (keeps components and logic).
	BlueprintBuilding
};

/** @brief How a building may deviate from facing its road. */
UENUM(BlueprintType)
enum class EScorchedBuildingRotationMode : uint8
{
	// Always faces the nearest road exactly.
	FaceRoad,
	// Faces the road plus a random multiple of 90 degrees.
	FaceRoad90Steps,
	// Faces the road plus a random multiple of 45 degrees.
	FaceRoad45Steps,
	// Any yaw; still deterministic from the seed.
	RandomYaw
};

/**
 * @brief One building prefab entry the generator can pick from. Footprints come from the
 * Blueprint's real component bounds unless bUseFootprintOverride supplies an explicit size.
 */
USTRUCT(BlueprintType)
struct FScorchedBuildingAssetSettings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Building)
	TSoftClassPtr<AActor> BuildingClass;

	/**
	 * @brief ScorchBuilding batches every placement of this entry into one shared HISM actor
	 * (use for mesh-heavy ruins); BlueprintBuilding spawns the Blueprint actor as-is.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Building)
	EScorchedBuildingCategory Category = EScorchedBuildingCategory::ScorchBuilding;

	/** @brief Relative pick chance among all candidates that fit a lot. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Building, meta = (ClampMin = "0"))
	float Weight = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Building)
	bool bUseFootprintOverride = false;

	/** @brief Full footprint size (X, Y) in cm; used instead of measured bounds when enabled. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Building,
		meta = (EditCondition = "bUseFootprintOverride"))
	FVector2D FootprintOverride = FVector2D(800.0, 800.0);

	/** @brief Extra clearance range added around this building; a value is picked per placement. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Building, meta = (ClampMin = "0"))
	float MinSpacing = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Building, meta = (ClampMin = "0"))
	float MaxSpacing = 300.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Building)
	EScorchedBuildingRotationMode RotationMode = EScorchedBuildingRotationMode::FaceRoad;

	/** @brief Larger categories are biased toward major roads and corner lots. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Building)
	EScorchedBuildingSize SizeCategory = EScorchedBuildingSize::Medium;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Building)
	EScorchedZonePreference ZonePreference = EScorchedZonePreference::None;
};

USTRUCT(BlueprintType)
struct FScorchedScatterMeshEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Scatter)
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Scatter, meta = (ClampMin = "0"))
	float Weight = 1.0f;
};

/**
 * @brief One scatter pass around buildings (debris, ruins, props...). Multiple profiles let
 * different mesh families use different densities, distances and rules in the same city.
 */
USTRUCT(BlueprintType)
struct FScorchedScatterProfile
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Scatter)
	TArray<FScorchedScatterMeshEntry> Meshes;

	/** @brief Average number of meshes spawned per building for this profile. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Scatter, meta = (ClampMin = "0"))
	float MeshesPerBuilding = 6.0f;

	/** @brief Distance range measured from the building's footprint edge, not its center. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Scatter, meta = (ClampMin = "0"))
	float MinDistanceFromBuildingBounds = 50.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Scatter, meta = (ClampMin = "0"))
	float MaxDistanceFromBuildingBounds = 900.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Scatter, meta = (ClampMin = "0.01"))
	float MinScale = 0.8f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Scatter, meta = (ClampMin = "0.01"))
	float MaxScale = 1.3f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Scatter)
	bool bRandomYaw = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Scatter, meta = (ClampMin = "0", ClampMax = "1"))
	float ChancePerBuilding = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Scatter)
	bool bAvoidRoads = true;

	/** @brief When false, debris is allowed to visibly intersect buildings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Scatter)
	bool bAvoidBuildings = true;

	/** @brief Line-traces each point down onto the world and aligns it to the ground normal. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Scatter)
	bool bAlignToGround = true;

	/** @brief 0 = uniform ring around the building, 1 = everything grouped into clusters. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Scatter, meta = (ClampMin = "0", ClampMax = "1"))
	float ClusterAmount = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Scatter, meta = (ClampMin = "1"))
	float ClusterRadius = 250.0f;
};

USTRUCT(BlueprintType)
struct FScorchedPowerLineSettings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = PowerLines)
	bool bEnablePowerLines = true;

	/** @brief Asset with two connected poles; should span roughly PowerLineSpacing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = PowerLines)
	TSoftClassPtr<AActor> TwoPoleAsset;

	/** @brief Single pole used for endpoints, isolated poles and broken sections. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = PowerLines)
	TSoftClassPtr<AActor> SinglePoleAsset;

	/** @brief Distance between consecutive pole positions along a road. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = PowerLines, meta = (ClampMin = "100"))
	float PowerLineSpacing = 1200.0f;

	/** @brief Chance that a section is broken: a single pole is used and the span is skipped. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = PowerLines, meta = (ClampMin = "0", ClampMax = "1"))
	float BrokenSectionChance = 0.15f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = PowerLines, meta = (ClampMin = "0"))
	float OffsetFromRoadEdge = 150.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = PowerLines)
	bool bAvoidBuildingOverlap = true;

	/** @brief When true, poles are yawed along the road; otherwise they get a random yaw. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = PowerLines)
	bool bFollowRoadDirection = true;

	/** @brief Half-size of the square footprint reserved around each pole. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = PowerLines, meta = (ClampMin = "1"))
	float PoleClearance = 80.0f;
};

/** @brief One decal material with its size range; used by all three decal systems. */
USTRUCT(BlueprintType)
struct FScorchedDecalEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Decals)
	TSoftObjectPtr<UMaterialInterface> Material;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Decals, meta = (ClampMin = "0"))
	float Weight = 1.0f;

	/** @brief Minimum world size (cm) of the projected decal. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Decals, meta = (ClampMin = "1"))
	float MinScale = 200.0f;

	/** @brief Maximum world size (cm) of the projected decal. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Decals, meta = (ClampMin = "1"))
	float MaxScale = 400.0f;
};

/**
 * @brief Shared settings for one decal subsection. Density units depend on the subsection
 * (per lot/building/pole/intersection, or per 10 m of road spline).
 */
USTRUCT(BlueprintType)
struct FScorchedDecalPlacementSettings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Decals)
	TArray<FScorchedDecalEntry> Decals;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Decals, meta = (ClampMin = "0"))
	float Density = 0.0f;

	/** @brief 0 allows tight packing, 1 asks for extra spacing between decals. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Decals, meta = (ClampMin = "0", ClampMax = "1"))
	float Scatter = 0.5f;
};

// ---------------------------------------------------------------------------
// Internal geometry / occupancy types (not exposed to designers).
// ---------------------------------------------------------------------------

/** @brief What an occupancy footprint belongs to; used to mask which overlaps are forbidden. */
enum class EScorchedOccupancy : uint8
{
	Road = 0,
	Intersection,
	Building,
	PowerPole,
	Decal,
	Count
};

constexpr uint8 ScorchedOccupancyMask(const EScorchedOccupancy Type)
{
	return static_cast<uint8>(1 << static_cast<uint8>(Type));
}

constexpr uint8 ScorchedOccupancyMaskAll()
{
	return static_cast<uint8>((1 << static_cast<uint8>(EScorchedOccupancy::Count)) - 1);
}

/**
 * @brief Oriented 2D rectangle on the XY plane; the unit every placement test works with.
 * All overlap checks in the generator are SAT tests between these, never center points.
 */
struct FScorchedFootprint
{
	FVector2D Center = FVector2D::ZeroVector;
	FVector2D HalfExtents = FVector2D(50.0, 50.0);

	// Yaw (radians) of the local X axis on the XY plane.
	double YawRadians = 0.0;

	/** @return Copy of this footprint grown by Amount on all sides (spacing inflation). */
	FScorchedFootprint Inflated(double Amount) const;

	void GetAxes(FVector2D& OutAxisX, FVector2D& OutAxisY) const;
	void GetCorners(FVector2D OutCorners[4]) const;
	FBox2D GetAABB() const;

	/** @return Half-size of this footprint projected onto WorldDirection (support radius). */
	double SupportRadius(const FVector2D& WorldDirection) const;

	/** @brief Separating-axis test between two oriented rectangles. */
	bool Overlaps(const FScorchedFootprint& Other) const;
};

/**
 * @brief Spatial hash grid over occupancy footprints so overlap queries stay O(local) even for
 * very large cities. Cells map to entry indices; queries SAT-test only nearby footprints.
 */
class FScorchedSpatialHashGrid
{
public:
	explicit FScorchedSpatialHashGrid(double InCellSize);

	int32 Add(const FScorchedFootprint& Footprint, EScorchedOccupancy Type);

	/**
	 * @brief Tests a candidate against all stored footprints whose type is in TypeMask.
	 * @param Footprint Candidate, already inflated by whatever spacing the caller requires.
	 * @param TypeMask Bitmask built from ScorchedOccupancyMask().
	 * @return True if any masked footprint overlaps the candidate.
	 */
	bool OverlapsAny(const FScorchedFootprint& Footprint, uint8 TypeMask) const;

	struct FEntry
	{
		FScorchedFootprint Footprint;
		EScorchedOccupancy Type = EScorchedOccupancy::Road;
	};

	const TArray<FEntry>& GetEntries() const { return M_Entries; }

private:
	double M_CellSize = 1000.0;
	TArray<FEntry> M_Entries;
	TMap<FIntPoint, TArray<int32>> M_Cells;

	FIntPoint CellOf(const FVector2D& Position) const;
	void ForEachCellOf(const FBox2D& AABB, TFunctionRef<void(const FIntPoint&)> Visitor) const;
};

// ---------------------------------------------------------------------------
// Road network / lot / result types.
// ---------------------------------------------------------------------------

struct FScorchedRoadNode
{
	FVector2D Position = FVector2D::ZeroVector;
	TArray<int32> Edges;
	bool bGridNode = false;
};

struct FScorchedRoadEdge
{
	int32 NodeA = INDEX_NONE;
	int32 NodeB = INDEX_NONE;

	// Polyline from NodeA to NodeB inclusive; straight grid streets have exactly 2 points.
	TArray<FVector2D> Points;

	bool bCurved = false;

	// Major roads get bigger lots and attract larger buildings.
	bool bMajor = false;

	double Length() const;
};

struct FScorchedLot
{
	FVector2D Center = FVector2D::ZeroVector;
	FVector2D HalfExtents = FVector2D::ZeroVector;

	// Yaw (radians) of the lot's local X axis; local X runs along the road.
	double YawRadians = 0.0;

	// Yaw (radians) a building must use to face the road this lot belongs to.
	double FacingYawRadians = 0.0;

	int32 EdgeIndex = INDEX_NONE;
	bool bCorner = false;
	bool bDense = false;
	bool bOnGridRoad = false;
	bool bMajorRoad = false;
	bool bUsed = false;
};

// --- Generation results (all positions in city-local space unless noted) ---

struct FScorchedBuildingSpawn
{
	// Index into the resolved building asset array.
	int32 AssetIndex = INDEX_NONE;

	FVector2D ActorPosition = FVector2D::ZeroVector;
	double YawRadians = 0.0;

	// World-testable footprint actually reserved for this building (uninflated).
	FScorchedFootprint Footprint;
};

struct FScorchedPoleSpawn
{
	bool bTwoPole = false;
	FVector2D Position = FVector2D::ZeroVector;
	FVector2D SecondPosition = FVector2D::ZeroVector;
	double YawRadians = 0.0;
};

struct FScorchedRoadSplineResult
{
	TArray<FVector2D> Points;
	bool bCurved = false;
};

struct FScorchedIntersectionSpawn
{
	FVector2D Position = FVector2D::ZeroVector;
	double YawRadians = 0.0;
};

/** @brief Which decal system produced a spawn; indexes the matching material array. */
enum class EScorchedDecalSet : uint8
{
	Lot,
	BuildingFootprint,
	Road,
	DestroyedPowerLine,
	PowerLine
};

struct FScorchedDecalSpawn
{
	EScorchedDecalSet Set = EScorchedDecalSet::Lot;
	int32 EntryIndex = INDEX_NONE;
	FVector2D Position = FVector2D::ZeroVector;
	double YawRadians = 0.0;
	// Full world size (cm) of the projected decal.
	double Size = 200.0;
};

struct FScorchedScatterCandidate
{
	int32 ProfileIndex = INDEX_NONE;
	int32 MeshIndex = INDEX_NONE;
	FVector2D Position = FVector2D::ZeroVector;
	double YawRadians = 0.0;
	double UniformScale = 1.0;
	bool bAlignToGround = false;
};

/**
 * @brief A road ending at the city rim that could not be connected to another road with a
 * natural curve. Exported so other PCG logic can pick these up and connect to them.
 */
struct FScorchedOrphanRoadEnd
{
	// Final outer point of the road.
	FVector2D Position = FVector2D::ZeroVector;

	// Yaw (radians) pointing outward, continuing the road past its ending.
	double YawRadians = 0.0;
};

struct FScorchedCityGenResult
{
	TArray<FScorchedRoadSplineResult> RoadSplines;
	TArray<FScorchedIntersectionSpawn> Intersections;
	TArray<FScorchedBuildingSpawn> Buildings;
	TArray<FScorchedPoleSpawn> Poles;
	TArray<FScorchedScatterCandidate> Scatter;
	TArray<FScorchedDecalSpawn> Decals;
	TArray<FScorchedLot> Lots;
	TArray<FScorchedSpatialHashGrid::FEntry> OccupiedFootprints;
	TArray<FScorchedOrphanRoadEnd> OuterOrphanRoads;
};
