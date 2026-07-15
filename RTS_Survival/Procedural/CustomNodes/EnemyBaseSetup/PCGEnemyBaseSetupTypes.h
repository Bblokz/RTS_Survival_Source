// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPtr.h"
#include "PCGEnemyBaseSetupTypes.generated.h"

class AActor;
class UMaterialInterface;
class UStaticMesh;

// ---------------------------------------------------------------------------
// Designer-facing enums.
//
// These drive the "pick a setup, then get the options that fit it" behaviour the
// nodes expose: enum choice + EditCondition on the dependent properties, so the
// designer only ever sees the settings relevant to the layout they picked.
// ---------------------------------------------------------------------------

/**
 * @brief How a setup decides the world direction it faces (toward the threat it defends against).
 * All obstacle belts, sandbag fronts and building yaws are derived from this facing.
 */
UENUM(BlueprintType)
enum class EEnemyFacingMode : uint8
{
	// Face along the input point's own rotation (local +X of the point).
	UsePointRotation,
	// Face a fixed designer-authored yaw (degrees), ignoring the point rotation.
	FixedYaw,
	// Face toward the nearest point on the AimTarget input (the position to defend / attack).
	AimTowardsTarget,
	// Face directly away from the nearest point on the AimTarget input.
	AwayFromTarget
};

/**
 * @brief Shape of an obstacle belt (anti-tank hedgehogs, dragon's teeth...) laid in front of a line.
 * The selected value decides which of the belt's extra options are relevant (see EditConditions).
 */
UENUM(BlueprintType)
enum class EEnemyObstacleFormation : uint8
{
	// A single straight belt running parallel to the defended line.
	StraightLine,
	// A convex arc bowing toward the threat, so the belt wraps the approach.
	Arc,
	// Two straight rows, the second staggered by half the spacing (deeper belt).
	DoubleRow
};

/** @brief Which sides of a building get a protective sandbag wall. */
UENUM(BlueprintType)
enum class EEnemySandbagCoverage : uint8
{
	// Left and right flanks only (classic enfilade cover).
	FlanksOnly,
	// Front + both flanks; the rear stays open (horseshoe).
	FrontAndFlanks,
	// A full ring around the building footprint.
	Surround
};

/** @brief How symmetrical defense positions are chosen around a defended center. */
UENUM(BlueprintType)
enum class EEnemyDefenseFormation : uint8
{
	// Four-way mirror about the center: every pick is matched by its ±X / ±Y reflections.
	RectangularSymmetrical,
	// Evenly spaced positions on a ring around the center (angular symmetry).
	RadialSymmetrical,
	// Mirror across a single axis through the center (the axis faces the AimTarget).
	AxisMirrored
};

/** @brief How the macro node wraps bunker lines around the base area. */
UENUM(BlueprintType)
enum class EEnemyPerimeterLayout : uint8
{
	// A bunker line on each of the four sides of the area bounds.
	FullPerimeter,
	// Only the side facing the AimTarget (a single front line).
	FrontLine,
	// Short strongpoint lines at the four corners of the area.
	Corners
};

/** @brief Which placed category a point / footprint belongs to. Also the occupancy mask index. */
UENUM(BlueprintType)
enum class EEnemyPlacementCategory : uint8
{
	Bunker,
	Sandbag,
	Hedgehog,
	BarbedWire,
	Decorator,
	Count UMETA(Hidden)
};

// ---------------------------------------------------------------------------
// Designer-facing settings structs (shared by every node).
// ---------------------------------------------------------------------------

/**
 * @brief One Blueprint prop the placement picks from (a bunker, a hedgehog, a sandbag segment,
 * a decorator...). Its footprint comes from the Blueprint's measured bounds unless an override
 * is supplied; an optional uniform scale range is applied per placement.
 */
USTRUCT(BlueprintType)
struct FEnemyBlueprintEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Entry)
	TSoftClassPtr<AActor> BlueprintClass;

	/** @brief Relative pick chance among all entries in the same list. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Entry, meta = (ClampMin = "0"))
	float Weight = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Entry)
	bool bUseFootprintOverride = false;

	/** @brief Full footprint size (X, Y) in cm; used instead of measured bounds when enabled. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Entry,
		meta = (EditCondition = "bUseFootprintOverride"))
	FVector2D FootprintOverride = FVector2D(400.0, 400.0);

	/** @brief When enabled, a uniform scale in [MinScale, MaxScale] is applied per placement. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Entry)
	bool bOverrideScale = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Entry,
		meta = (ClampMin = "0.01", EditCondition = "bOverrideScale"))
	float MinScale = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Entry,
		meta = (ClampMin = "0.01", EditCondition = "bOverrideScale"))
	float MaxScale = 1.0f;
};

/**
 * @brief A belt of anti-tank obstacles (hedgehogs, dragon's teeth) placed in front of a defended
 * line. The Formation enum decides which extra options apply: Arc exposes the arc span/bulge,
 * DoubleRow uses the row settings for a deeper belt.
 */
USTRUCT(BlueprintType)
struct FEnemyObstacleBeltSettings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Obstacle Belt")
	bool bEnabled = true;

	/** @brief Obstacle Blueprints (hedgehogs etc.); one is weighted-picked per placement. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Obstacle Belt",
		meta = (EditCondition = "bEnabled"))
	TArray<FEnemyBlueprintEntry> Obstacles;

	/** @brief Straight belt, arc bowing toward the threat, or a deeper staggered double row. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Obstacle Belt",
		meta = (EditCondition = "bEnabled"))
	EEnemyObstacleFormation Formation = EEnemyObstacleFormation::StraightLine;

	/** @brief Distance (cm) from the defended line's front edge to the belt. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Obstacle Belt",
		meta = (ClampMin = "0", EditCondition = "bEnabled"))
	float DistanceFromLine = 600.0f;

	/** @brief Extra length (cm) added to each side of the belt beyond the defended line. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Obstacle Belt",
		meta = (ClampMin = "0", EditCondition = "bEnabled"))
	float SideOverhang = 400.0f;

	/** @brief Spacing (cm) between consecutive obstacles along the belt. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Obstacle Belt",
		meta = (ClampMin = "50", EditCondition = "bEnabled"))
	float SpacingBetweenObstacles = 350.0f;

	/** @brief Arc-only: angular span (degrees) the belt bows toward the threat. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Obstacle Belt",
		meta = (ClampMin = "10", ClampMax = "180",
			EditCondition = "bEnabled && Formation == EEnemyObstacleFormation::Arc"))
	float ArcSpanDegrees = 90.0f;

	/** @brief Arc-only: 0 derives the arc radius from the belt length; larger flattens the arc. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Obstacle Belt",
		meta = (ClampMin = "0",
			EditCondition = "bEnabled && Formation == EEnemyObstacleFormation::Arc"))
	float ArcRadiusOverride = 0.0f;

	/** @brief DoubleRow-only: distance (cm) between the two rows. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Obstacle Belt",
		meta = (ClampMin = "50",
			EditCondition = "bEnabled && Formation == EEnemyObstacleFormation::DoubleRow"))
	float RowSpacing = 300.0f;
};

/**
 * @brief Barbed-wire runs placed in front of a defended line: one wire Blueprint spans
 * SegmentLength, chained end to end across the width so the wire looks continuous.
 */
USTRUCT(BlueprintType)
struct FEnemyBarbedWireSettings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Barbed Wire")
	bool bEnabled = false;

	/** @brief Wire Blueprints; one is weighted-picked per segment. Author it ~SegmentLength long on +X. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Barbed Wire",
		meta = (EditCondition = "bEnabled"))
	TArray<FEnemyBlueprintEntry> WireSegments;

	/** @brief Length (cm) each wire Blueprint spans along its local +X; drives the chaining step. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Barbed Wire",
		meta = (ClampMin = "50", EditCondition = "bEnabled"))
	float SegmentLength = 400.0f;

	/** @brief Distance (cm) from the defended line's front edge to the wire run. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Barbed Wire",
		meta = (ClampMin = "0", EditCondition = "bEnabled"))
	float DistanceFromLine = 300.0f;

	/** @brief Extra length (cm) added to each side of the run beyond the defended line. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Barbed Wire",
		meta = (ClampMin = "0", EditCondition = "bEnabled"))
	float SideOverhang = 200.0f;

	/** @brief Number of parallel wire runs; extra runs are pushed further toward the threat. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Barbed Wire",
		meta = (ClampMin = "1", ClampMax = "6", EditCondition = "bEnabled"))
	int32 Rows = 1;

	/** @brief Distance (cm) between consecutive wire runs when Rows > 1. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Barbed Wire",
		meta = (ClampMin = "50", EditCondition = "bEnabled && Rows > 1"))
	float RowSpacing = 250.0f;
};

/**
 * @brief A sandbag wall wrapped around a building's flanks (and optionally front / all sides).
 * One sandbag Blueprint spans SegmentLength and is chained along each covered side.
 */
USTRUCT(BlueprintType)
struct FEnemySandbagSettings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Sandbags")
	bool bEnabled = true;

	/** @brief Sandbag Blueprints; one is weighted-picked per segment. Author it ~SegmentLength long on +X. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Sandbags",
		meta = (EditCondition = "bEnabled"))
	TArray<FEnemyBlueprintEntry> SandbagUnits;

	/** @brief Which sides of the building the wall covers. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Sandbags",
		meta = (EditCondition = "bEnabled"))
	EEnemySandbagCoverage Coverage = EEnemySandbagCoverage::FlanksOnly;

	/** @brief Length (cm) each sandbag Blueprint spans along its local +X; drives the chaining step. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Sandbags",
		meta = (ClampMin = "50", EditCondition = "bEnabled"))
	float SegmentLength = 300.0f;

	/** @brief Gap (cm) between the building footprint edge and the sandbag wall. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Sandbags",
		meta = (ClampMin = "0", EditCondition = "bEnabled"))
	float DistanceFromBuilding = 150.0f;

	/** @brief Extra length (cm) each flank wall extends beyond the building's front, forming wings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Sandbags",
		meta = (ClampMin = "0", EditCondition = "bEnabled"))
	float FlankExtension = 200.0f;
};

/**
 * @brief One profile of decorator props (ammo crates, fuel tanks...) scattered in a ring around
 * each placed setup. Multiple profiles let ammo and fuel use different counts and distances.
 */
USTRUCT(BlueprintType)
struct FEnemyDecoratorProfile
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Decorator")
	TArray<FEnemyBlueprintEntry> Decorators;

	/** @brief Average number placed around each setup cluster (bunker line / building). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Decorator", meta = (ClampMin = "0"))
	float CountPerSetup = 2.0f;

	/** @brief Ring distance range (cm) measured from the setup's combined footprint edge. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Decorator", meta = (ClampMin = "0"))
	float MinDistanceFromBounds = 80.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Decorator", meta = (ClampMin = "0"))
	float MaxDistanceFromBounds = 500.0f;

	/** @brief Seeded chance the whole profile is placed for a given setup cluster. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Decorator",
		meta = (ClampMin = "0", ClampMax = "1"))
	float ChancePerSetup = 1.0f;

	/** @brief When true, retries a few nearby positions if a placement overlaps existing content. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Decorator")
	bool bAvoidOverlap = true;
};

/** @brief One foliage static mesh emitted on the Foliage output pin (for a Static Mesh Spawner). */
USTRUCT(BlueprintType)
struct FEnemyFoliageEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Foliage")
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Foliage", meta = (ClampMin = "0"))
	float Weight = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Foliage", meta = (ClampMin = "0.01"))
	float MinScale = 0.8f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Foliage", meta = (ClampMin = "0.01"))
	float MaxScale = 1.2f;
};

/**
 * @brief Foliage scattered in a ring around each setup, emitted as points with a "Mesh" attribute
 * (wire the Foliage pin into a Static Mesh Spawner, mesh selection "By Attribute").
 */
USTRUCT(BlueprintType)
struct FEnemyFoliageSettings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Foliage")
	TArray<FEnemyFoliageEntry> Foliage;

	/** @brief Average number of foliage meshes scattered around each setup cluster. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Foliage", meta = (ClampMin = "0"))
	float CountPerSetup = 6.0f;

	/** @brief Ring distance range (cm) measured from the setup's combined footprint edge. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Foliage", meta = (ClampMin = "0"))
	float MinDistanceFromBounds = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Foliage", meta = (ClampMin = "0"))
	float MaxDistanceFromBounds = 700.0f;

	/** @brief When true, foliage may overlap placed props (it is groundcover, not an obstacle). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Foliage")
	bool bAllowOverlap = true;
};

/** @brief One decal material with the world size range it may be projected at. */
USTRUCT(BlueprintType)
struct FEnemyDecalEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Decal")
	TSoftObjectPtr<UMaterialInterface> Material;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Decal", meta = (ClampMin = "0"))
	float Weight = 1.0f;

	/** @brief Minimum projected world size (cm). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Decal", meta = (ClampMin = "1"))
	float MinScale = 200.0f;

	/** @brief Maximum projected world size (cm). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Decal", meta = (ClampMin = "1"))
	float MaxScale = 400.0f;
};

/** @brief Ground decals (scorch marks, tracks...) scattered around each setup, projected straight down. */
USTRUCT(BlueprintType)
struct FEnemyDecalSettings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Decal")
	TArray<FEnemyDecalEntry> Decals;

	/** @brief Average number of decals projected around each setup cluster. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Decal", meta = (ClampMin = "0"))
	float CountPerSetup = 3.0f;

	/** @brief Ring distance range (cm) measured from the setup's combined footprint edge. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Decal", meta = (ClampMin = "0"))
	float MinDistanceFromBounds = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Decal", meta = (ClampMin = "0"))
	float MaxDistanceFromBounds = 500.0f;
};

// ---------------------------------------------------------------------------
// Internal geometry / occupancy types (not exposed to designers).
// SAT footprint + spatial hash: same proven approach as the Scorched City node.
// ---------------------------------------------------------------------------

constexpr uint8 EnemyOccupancyMask(const EEnemyPlacementCategory Type)
{
	return static_cast<uint8>(1 << static_cast<uint8>(Type));
}

constexpr uint8 EnemyOccupancyMaskAll()
{
	return static_cast<uint8>((1 << static_cast<uint8>(EEnemyPlacementCategory::Count)) - 1);
}

/**
 * @brief Oriented 2D rectangle on the XY plane; the unit every placement test works with.
 * All overlap checks are SAT tests between these, never center points.
 */
struct FEnemyFootprint
{
	FVector2D Center = FVector2D::ZeroVector;
	FVector2D HalfExtents = FVector2D(50.0, 50.0);

	// Yaw (radians) of the local X axis on the XY plane.
	double YawRadians = 0.0;

	/** @return Copy of this footprint grown by Amount on all sides (spacing inflation). */
	FEnemyFootprint Inflated(double Amount) const;

	void GetAxes(FVector2D& OutAxisX, FVector2D& OutAxisY) const;
	void GetCorners(FVector2D OutCorners[4]) const;
	FBox2D GetAABB() const;

	/** @return Half-size of this footprint projected onto WorldDirection (support radius). */
	double SupportRadius(const FVector2D& WorldDirection) const;

	/** @brief Separating-axis test between two oriented rectangles. */
	bool Overlaps(const FEnemyFootprint& Other) const;
};

/**
 * @brief Spatial hash grid over occupancy footprints so overlap queries stay O(local).
 * Cells map to entry indices; queries SAT-test only nearby footprints of the masked types.
 */
class FEnemyOccupancyGrid
{
public:
	explicit FEnemyOccupancyGrid(double InCellSize);

	int32 Add(const FEnemyFootprint& Footprint, EEnemyPlacementCategory Type);

	/**
	 * @brief Tests a candidate against all stored footprints whose type is in TypeMask.
	 * @param Footprint Candidate, already inflated by whatever spacing the caller requires.
	 * @param TypeMask Bitmask built from EnemyOccupancyMask().
	 * @return True if any masked footprint overlaps the candidate.
	 */
	bool OverlapsAny(const FEnemyFootprint& Footprint, uint8 TypeMask) const;

	struct FEntry
	{
		FEnemyFootprint Footprint;
		EEnemyPlacementCategory Type = EEnemyPlacementCategory::Bunker;
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
// Generation result types (all positions in world XY unless noted).
// ---------------------------------------------------------------------------

/**
 * @brief One placed prop (bunker, sandbag, hedgehog, wire or decorator). AssetIndex indexes the
 * resolved list for its Category; ClusterIndex groups all props of one setup instance so the
 * element can emit per-cluster total bounds.
 */
struct FEnemyPlacedActor
{
	EEnemyPlacementCategory Category = EEnemyPlacementCategory::Bunker;
	int32 AssetIndex = INDEX_NONE;
	int32 ClusterIndex = 0;

	FVector2D Position = FVector2D::ZeroVector;
	double YawRadians = 0.0;
	double UniformScale = 1.0;

	// Reserved (uninflated) footprint; used for occupancy output and total-bounds unions.
	FEnemyFootprint Footprint;
};

struct FEnemyFoliageSpawn
{
	int32 MeshEntryIndex = INDEX_NONE;
	int32 ClusterIndex = 0;
	FVector2D Position = FVector2D::ZeroVector;
	double YawRadians = 0.0;
	double UniformScale = 1.0;
	bool bAlignToGround = true;
};

struct FEnemyDecalSpawn
{
	int32 EntryIndex = INDEX_NONE;
	int32 ClusterIndex = 0;
	FVector2D Position = FVector2D::ZeroVector;
	double YawRadians = 0.0;
	// Full projected world size (cm).
	double Size = 200.0;
};

/** @brief Everything one node's generator produced, ready for the element to spawn and emit. */
struct FEnemyBaseSetupResult
{
	TArray<FEnemyPlacedActor> Actors;
	TArray<FEnemyFoliageSpawn> Foliage;
	TArray<FEnemyDecalSpawn> Decals;

	// Number of distinct setup clusters (one per building / bunker line / defense position).
	int32 NumClusters = 0;
};
