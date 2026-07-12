// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPtr.h"
#include "PCGForestBiomeTypes.generated.h"

class AActor;
class UStaticMesh;

// ---------------------------------------------------------------------------
// Designer-facing enums.
// ---------------------------------------------------------------------------

/** @brief Size class of an auxiliary prop; drives how strongly it is isolated from other auxiliaries. */
UENUM(BlueprintType)
enum class EForestAuxiliarySize : uint8
{
	Small,
	Medium,
	Large
};

/** @brief How an auxiliary prop is turned into world geometry. */
UENUM(BlueprintType)
enum class EForestAuxiliarySpawnType : uint8
{
	// Static meshes of the Blueprint are extracted and batched into shared Hierarchical Instanced
	// Static Mesh components (cheap; any non-mesh components and logic of the Blueprint are discarded).
	Instanced,
	// Spawned as its own Blueprint actor, unchanged (keeps all components and logic).
	Blueprint
};

// ---------------------------------------------------------------------------
// Designer-facing settings structs.
// ---------------------------------------------------------------------------

/**
 * @brief One Blueprint prop entry (a large tree, a regular tree or a bush). Large and regular
 * trees always spawn as their own Blueprint actors; bushes are instanced. The clearance radius
 * is measured from the Blueprint's bounds unless RadiusOverride supplies an explicit value.
 */
USTRUCT(BlueprintType)
struct FForestBlueprintEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Entry)
	TSoftClassPtr<AActor> BlueprintClass;

	/** @brief Relative pick chance among all entries in the same list. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Entry, meta = (ClampMin = "0"))
	float Weight = 1.0f;

	/** @brief When enabled, a uniform scale in [MinScale, MaxScale] is applied per placement. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Entry)
	bool bOverrideScale = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Entry,
		meta = (ClampMin = "0.01", EditCondition = "bOverrideScale"))
	float MinScale = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Entry,
		meta = (ClampMin = "0.01", EditCondition = "bOverrideScale"))
	float MaxScale = 1.0f;

	/**
	 * @brief Clearance radius (cm) reserved around this prop so nothing else spawns inside it.
	 * 0 = derive it from the Blueprint's measured bounds; lower it for tighter, overlapping canopies.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Entry, meta = (ClampMin = "0"))
	float RadiusOverride = 0.0f;
};

/**
 * @brief One foliage static mesh emitted on the Foliage output pin (wire it into a GPU Static
 * Mesh Spawner). Foliage is always scaled by a uniform value in [MinScale, MaxScale] and never
 * blocks other foliage, so it can be dense groundcover.
 */
USTRUCT(BlueprintType)
struct FForestFoliageEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Foliage)
	TSoftObjectPtr<UStaticMesh> Mesh;

	/** @brief Relative pick chance among all foliage entries. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Foliage, meta = (ClampMin = "0"))
	float Weight = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Foliage, meta = (ClampMin = "0.01"))
	float MinScale = 0.8f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Foliage, meta = (ClampMin = "0.01"))
	float MaxScale = 1.2f;
};

/**
 * @brief One auxiliary prop (rocks, wrecks, debris...) scattered into the space trees and bushes
 * leave free. Size drives how far it is kept from other auxiliaries; spawn type chooses instanced
 * meshes or a full Blueprint actor.
 */
USTRUCT(BlueprintType)
struct FForestAuxiliaryEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Auxiliary)
	TSoftClassPtr<AActor> AuxiliaryClass;

	/** @brief Relative pick chance among all auxiliary entries. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Auxiliary, meta = (ClampMin = "0"))
	float Weight = 1.0f;

	/** @brief Larger sizes are pushed further from other auxiliaries via the section's size multipliers. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Auxiliary)
	EForestAuxiliarySize Size = EForestAuxiliarySize::Medium;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Auxiliary)
	EForestAuxiliarySpawnType SpawnType = EForestAuxiliarySpawnType::Instanced;

	/**
	 * @brief Base spacing (cm) kept clear of other auxiliaries before the per-size multiplier is
	 * applied; the final isolation is this value times the size multiplier of this entry's Size.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Auxiliary, meta = (ClampMin = "0"))
	float PreferredDistanceFromOtherAuxiliaries = 400.0f;

	/** @brief When enabled, a uniform scale in [MinScale, MaxScale] is applied per placement. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Auxiliary)
	bool bOverrideScale = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Auxiliary,
		meta = (ClampMin = "0.01", EditCondition = "bOverrideScale"))
	float MinScale = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Auxiliary,
		meta = (ClampMin = "0.01", EditCondition = "bOverrideScale"))
	float MaxScale = 1.0f;

	/** @brief Clearance radius (cm) reserved around this prop; 0 = derive it from the Blueprint bounds. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Auxiliary, meta = (ClampMin = "0"))
	float RadiusOverride = 0.0f;
};

// ---------------------------------------------------------------------------
// Internal occupancy geometry (not exposed to designers).
// ---------------------------------------------------------------------------

/** @brief What a reserved circle belongs to; used to mask which categories a query tests against. */
enum class EForestOccupancy : uint8
{
	LargeTree = 0,
	RegularTree,
	Bush,
	Auxiliary,
	Count
};

constexpr uint8 ForestOccupancyMask(const EForestOccupancy Type)
{
	return static_cast<uint8>(1 << static_cast<uint8>(Type));
}

/** @return Mask of the solid props (large trees, regular trees, bushes) everything else must avoid. */
constexpr uint8 ForestSolidPropMask()
{
	return ForestOccupancyMask(EForestOccupancy::LargeTree)
		| ForestOccupancyMask(EForestOccupancy::RegularTree)
		| ForestOccupancyMask(EForestOccupancy::Bush);
}

/**
 * @brief Spatial hash of reserved circles so overlap and isolation queries stay O(local) even for
 * dense biomes. Each circle is registered in every cell its bounding box covers; queries only
 * distance-test the circles found in the cells the query reaches.
 */
class FForestOccupancyGrid
{
public:
	explicit FForestOccupancyGrid(double InCellSize);

	/** @brief Reserves a circle of the given type. */
	void Add(const FVector2D& Center, double Radius, EForestOccupancy Type);

	/**
	 * @brief Tests whether a candidate circle, separated by an extra clearance, is too close to any
	 * reserved circle whose type is in TypeMask.
	 * @param TypeMask Bitmask built from ForestOccupancyMask() / ForestSolidPropMask().
	 * @param ExtraClearance Extra separation required beyond the sum of the two radii (isolation).
	 * @return True when the candidate is closer than (Radius + stored radius + ExtraClearance) to a masked circle.
	 */
	bool OverlapsAny(const FVector2D& Center, double Radius, uint8 TypeMask, double ExtraClearance) const;

private:
	struct FEntry
	{
		FVector2D Center = FVector2D::ZeroVector;
		double Radius = 50.0;
		EForestOccupancy Type = EForestOccupancy::LargeTree;
	};

	double M_CellSize = 500.0;
	TArray<FEntry> M_Entries;
	TMap<FIntPoint, TArray<int32>> M_Cells;

	FIntPoint CellOf(const FVector2D& Position) const;
	void ForEachCellOf(const FBox2D& AABB, TFunctionRef<void(const FIntPoint&)> Visitor) const;
};
