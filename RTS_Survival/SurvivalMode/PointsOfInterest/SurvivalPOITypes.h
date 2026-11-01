// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "SurvivalPOITypes.generated.h"

RTS_SURVIVAL_API DECLARE_LOG_CATEGORY_EXTERN(LogSurvivalPOI, Log, All);

class AActor;
class UMaterialInterface;
class UNiagaraSystem;
class UStaticMesh;

/** @brief What a point of interest is; landmarks are the fixed one-per-sector types. */
UENUM(BlueprintType)
enum class ESurvivalPOIType : uint8
{
	SmallBattlefield,
	RuinedHangar,
	// Landmarks: one fixed type per sector between two neighbouring starts.
	CrashSite,
	DeadConvoy,
	TankGraveyard,
	PumpStation,
	RelayStation,
	None UMETA(Hidden)
};

/** @brief Why an actor stands in a point of interest; the manifest and the critic read it. */
UENUM(BlueprintType)
enum class ESurvivalPOIRole : uint8
{
	// The largest object, the one the story is told around.
	Hero,
	Scavenge,
	Capture,
	Cover,
	Debris,
	Decal,
	Crater
};

/** @brief How a spawned object is put on the ground. */
UENUM(BlueprintType)
enum class ESurvivalPOISeatMode : uint8
{
	// Lowest visible mesh point on the ground, tilted to the ground under its footprint (wrecks, props).
	AlignToGround,
	// Lowest visible mesh point on the ground, kept upright (buildings, stacks).
	UprightOnGround,
	// The pivot on the ground: dug-in pits, trenches and decal Blueprints whose meshes reach below ground on purpose.
	PivotOnGround
};

UENUM(BlueprintType)
enum class ESurvivalPOISeedMode : uint8
{
	RandomPerMatch,
	Fixed
};

UENUM(BlueprintType)
enum class ESurvivalWildlifeKind : uint8
{
	// AWanderer Blueprints walking around their spawn point.
	GroundHerd,
	// ABirdFlock Blueprints circling an ellipse.
	BirdFlock,
	// A Niagara system that draws the whole flock itself.
	NiagaraFlock
};

UENUM(BlueprintType)
enum class ESurvivalWildlifeAnchor : uint8
{
	// On the outer bank of a map exclusion zone marked as water (the oasis), never inside it.
	WaterBank,
	// Over a point of interest of one of the listed types (carrion birds over wrecks).
	OverPointOfInterest,
	// High over the open basin floor, one per sector.
	OverSector
};

/**
 * @brief One spawnable thing of a point of interest: a Blueprint, a static mesh (shown by ASurvivalPOIProp) or a
 * decal material. The first one set is used.
 */
USTRUCT(BlueprintType)
struct FSurvivalPOIAsset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	TSoftClassPtr<AActor> ActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	TSoftObjectPtr<UMaterialInterface> DecalMaterial;

	// Decals only: projection depth (X) and half size on the ground (Y, Z), in cm before scale.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	FVector DecalSize = FVector(200.0, 400.0, 400.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI", meta = (ClampMin = "0.0"))
	float Weight = 1.f;

	// Radius the object keeps free around itself when the composition is laid out.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI", meta = (ClampMin = "0.0"))
	float FootprintRadius = 400.f;

	// Long objects (hangars): half size along the asset's X and Y. When set, the layout keeps a row of circles along X
	// free instead of one circle of FootprintRadius, so props can stand along the walls.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	FVector2D HalfExtent = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	ESurvivalPOISeatMode SeatMode = ESurvivalPOISeatMode::AlignToGround;

	// Ground steeper than this under the object rejects the spot.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI", meta = (ClampMin = "0.0", ClampMax = "60.0"))
	float MaxSlopeDegrees = 15.f;

	// Added to the composition's yaw: turns the asset so its front matches the composition's forward (+X).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	float YawOffsetDegrees = 0.f;

	// Meshes only: carve the navmesh under the mesh (the navmesh only changes through nav modifiers).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	bool bBlocksNavigation = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	FVector2D ScaleRange = FVector2D(1.0, 1.0);

	// Scavengeable objects only: rewards written into the object after it spawns (ERTSResourceType names, e.g.
	// Resource_Metal), for Blueprints that carry none of their own. Empty keeps the Blueprint's rewards.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	TMap<FName, int32> RewardOverride;

	// Scavengeable wrecks only: an ETankSubtype name (Tank_ZilAmmo); after spawning, the wreck's salvage is set up as that
	// vehicle's (AScavengeableObject::SetupAsDestroyedVehicle). Empty keeps the Blueprint's own rewards.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	FName RewardTankSubtype;

	bool GetIsSet() const { return not ActorClass.IsNull() or not Mesh.IsNull() or not DecalMaterial.IsNull(); }
	bool GetIsDecal() const { return ActorClass.IsNull() and Mesh.IsNull() and not DecalMaterial.IsNull(); }
	FSoftObjectPath GetLoadPath() const;
	FString GetDisplayPath() const;
};

/** @brief A named list of interchangeable assets; compositions pick from pools by name. */
USTRUCT(BlueprintType)
struct FSurvivalPOIAssetPool
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	FName PoolName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	TArray<FSurvivalPOIAsset> Assets;
};

/** @brief How a point of interest type looks for ground. */
USTRUCT(BlueprintType)
struct FSurvivalPOITypeRules
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	ESurvivalPOIType Type = ESurvivalPOIType::SmallBattlefield;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	FText DisplayName;

	// The composition stays inside this radius.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI", meta = (ClampMin = "500.0"))
	float SiteRadius = 3500.f;

	// Highest minus lowest ground inside the site.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI", meta = (ClampMin = "0.0"))
	float MaxHeightRange = 250.f;

	// Steepest ground slope (degrees) measured over one height grid cell inside the site.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI", meta = (ClampMin = "0.0"))
	float MaxSlopeDegrees = 8.f;

	// Preferred distance from the start this POI belongs to; sites outside are rejected.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	FVector2D OwnerStartDistance = FVector2D(19000.0, 30000.0);

	// HeavyTank drive length from the owning start's deploy centre the site must have (chained nav): keeps every start's
	// nearest POIs at comparable driving distance, whatever cliffs stand between.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	FVector2D OwnerNavDistance = FVector2D(20000.0, 34000.0);
};

/** @brief The fixed landmark of the sector between two named starts (the order of the two names does not matter). */
USTRUCT(BlueprintType)
struct FSurvivalPOISectorLandmark
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	FString StartA;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	FString StartB;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	ESurvivalPOIType Landmark = ESurvivalPOIType::CrashSite;
};

/** @brief A map-specific disc nothing may stand in (the oasis basin plus its rim). */
USTRUCT(BlueprintType)
struct FSurvivalPOIExclusionZone
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	FVector2D Center = FVector2D::ZeroVector;

	// The feature itself (the oasis basin outline's farthest point from Center).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI", meta = (ClampMin = "0.0"))
	float Radius = 6000.f;

	// Extra distance points of interest keep; animals only keep Radius + WaterBankMinDistance.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI", meta = (ClampMin = "0.0"))
	float PropRim = 3000.f;

	// Water: grazing animals live on its outer bank.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	bool bIsWater = false;
};

/** @brief A ramp or track of the map layout: a line nothing may stand on, with its gate foot at FootPoint. */
USTRUCT(BlueprintType)
struct FSurvivalPOILayoutLine
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	TArray<FVector2D> Points;

	// The end farthest from its start: the gate foot, which keeps a wider clearance.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	FVector2D FootPoint = FVector2D::ZeroVector;
};

/** @brief A pad or basin outline of the map layout. */
USTRUCT(BlueprintType)
struct FSurvivalPOILayoutArea
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	TArray<FVector2D> Outline;

	// Extra distance kept outside the outline (the oasis basin's 3 000 rim; pads keep the site off their rims).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	float Clearance = 0.f;
};

/**
 * @brief The map layout's ramps, pads and basins, copied from the level tools' layout JSON by
 * Tools/poi_sync_layout.py (the runtime cannot see them in the landscape). LayoutMD5 tells when the copy is stale.
 */
USTRUCT(BlueprintType)
struct FSurvivalPOILayoutExclusions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	FString LayoutFile;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	FString LayoutMD5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	TArray<FSurvivalPOILayoutLine> Ramps;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	TArray<FSurvivalPOILayoutArea> Areas;

	// Point obstacles of the map (power line pylons): XY and, in Z, the radius points of interest keep clear.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	TArray<FVector> Obstacles;

	// Half width of the ramp corridor nothing may enter.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	float RampHalfWidth = 1100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	float GateFootClearance = 3000.f;
};

/** @brief Where points of interest may go; all distances in cm. */
USTRUCT(BlueprintType)
struct FSurvivalPOIPlacementRules
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	float MinDistanceFromStart = 18000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	float MinDistanceFromLandscapeEdge = 9000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	float MinDistanceFromEdgeSpawnZone = 6000.f;

	// Added to the bounds of every Soviet base PCG volume.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	float SovietBaseClearance = 2000.f;

	// Free ground between two sites, on top of both radii.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	float MinGapBetweenSites = 6000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI", meta = (ClampMin = "300.0"))
	float HeightGridSpacing = 750.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI", meta = (ClampMin = "500.0"))
	float CandidateSpacing = 1500.f;

	// A site's centre may stand this far above the sector's basin floor (the 25th percentile of its ground).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	float MaxHeightAboveBasinFloor = 450.f;

	// Ring sampled around a site: no cliff, pad rim or ramp may rise next to it.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	float RimCheckDistance = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	float MaxRimRise = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	float MaxRimDrop = 400.f;

	// The best this fraction of candidates is drawn from at random, so seeds move the sites.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float CandidateTopFraction = 0.3f;

	// Candidate grid points tested per step (the site search runs over several frames).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI", meta = (ClampMin = "100"))
	int32 HeightSamplesPerStep = 4000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	TArray<FSurvivalPOIExclusionZone> ExclusionZones;

	// Free ground around every start's ReservedContentSpots (its ramps and gate aprons), on top of their radius.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	float ReservedSpotClearance = 3000.f;

	// The play area when it is smaller than the landscape (a cropped map); unused while Max is not above Min.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	FVector2D PlayableAreaMin = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	FVector2D PlayableAreaMax = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "POI")
	FSurvivalPOILayoutExclusions Layout;
};

/** @brief One kind of animal group or flock and where it lives. */
USTRUCT(BlueprintType)
struct FSurvivalWildlifeEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	ESurvivalWildlifeKind Kind = ESurvivalWildlifeKind::GroundHerd;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	ESurvivalWildlifeAnchor Anchor = ESurvivalWildlifeAnchor::WaterBank;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	TSoftClassPtr<AActor> ActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	TSoftObjectPtr<UNiagaraSystem> NiagaraSystem;

	// Groups (herds or flocks) of this entry per match; for OverPointOfInterest the most, one per matching POI.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife", meta = (ClampMin = "0"))
	int32 GroupCount = 1;

	// Chance that a matching POI gets a flock (OverPointOfInterest only).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ChancePerPointOfInterest = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	TArray<ESurvivalPOIType> OverPointOfInterestTypes;

	// Animals per herd or birds per flock.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	FIntPoint GroupSize = FIntPoint(2, 5);

	// --- Ground herds ---

	// Each animal walks within this radius of where it spawned.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	float WanderRadius = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	float FleeDistance = 900.f;

	// Any unit (not the player's camera) this close to the herd makes it bolt; 0 = only when shot (the Wanderer default).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	float AlarmRadius = 1500.f;

	// Animals of a herd spawn within this radius of the herd centre.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	float HerdSpread = 450.f;

	// From the water zone's Radius: the band of the bank herds use.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	FVector2D WaterBankDistance = FVector2D(2600.0, 4200.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	float MaxGroundSlopeDegrees = 20.f;

	// Animals never come closer to a start than this (plus wander and flee distance).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	float MinDistanceFromStart = 9000.f;

	// --- Flocks ---

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	FVector2D FlightHeight = FVector2D(1500.0, 2200.0);

	// Semi axes of the circled ellipse.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	FVector2D SemiAxisX = FVector2D(1800.0, 2600.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	FVector2D SemiAxisY = FVector2D(1100.0, 1700.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wildlife")
	float FlightSpeed = 650.f;
};

// --- Plan data (not reflected): what the PCG nodes pass to each other as points. ---

/** @brief One chosen point of interest. */
struct FSurvivalPOISite
{
	int32 SiteIndex = INDEX_NONE;
	ESurvivalPOIType Type = ESurvivalPOIType::None;
	bool bIsLandmark = false;
	int32 Sector = INDEX_NONE;
	// Index into the director's start locations of the start this POI belongs to (the nearer one of its sector).
	int32 OwnerStart = INDEX_NONE;
	FVector Center = FVector::ZeroVector;
	float YawDegrees = 0.f;
	float Radius = 0.f;
	int32 SubLayout = 0;
	int32 Seed = 0;
};

/** @brief One object a composition wants spawned. AssetKey = pool index * AssetKeyStride + asset index. */
struct FSurvivalPOIPlacedObject
{
	static constexpr int32 AssetKeyStride = 1000;

	int32 SiteIndex = INDEX_NONE;
	int32 AssetKey = INDEX_NONE;
	ESurvivalPOIRole Role = ESurvivalPOIRole::Debris;
	FVector Location = FVector::ZeroVector;
	float YawDegrees = 0.f;
	float Scale = 1.f;
	// Planned sink below the ground contact (half buried in drifted sand); 0 = sits on the sand.
	float SinkCm = 0.f;
};

/** @brief One herd or flock. */
struct FSurvivalWildlifeGroup
{
	int32 EntryIndex = INDEX_NONE;
	int32 SiteIndex = INDEX_NONE;
	FVector Center = FVector::ZeroVector;
	int32 Count = 0;
	float YawDegrees = 0.f;
	float FlightHeight = 0.f;
	FVector2D SemiAxes = FVector2D::ZeroVector;
	int32 Seed = 0;
};

namespace SurvivalPOINames
{
	/** @return Short stable name of a type for logs, manifests and file names. */
	RTS_SURVIVAL_API FString GetTypeName(ESurvivalPOIType Type);
	RTS_SURVIVAL_API FString GetRoleName(ESurvivalPOIRole Role);
	RTS_SURVIVAL_API bool GetIsLandmarkType(ESurvivalPOIType Type);
}
