// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "PCGCreateBattlefieldArea.generated.h"

class AActor;
class UMaterialInterface;

/**
 * @brief Declares the authored length axis of a modular battlefield prop.
 * Barbed-wire runs use this axis to place measured pieces end to end while keeping their pivots centered.
 */
UENUM(BlueprintType)
enum class EBattlefieldLineDirection : uint8
{
	PositiveX UMETA(DisplayName = "+X"),
	NegativeX UMETA(DisplayName = "-X"),
	PositiveY UMETA(DisplayName = "+Y"),
	NegativeY UMETA(DisplayName = "-Y")
};

/**
 * @brief Configures a bounds-measured Blueprint used in one battlefield section.
 * The optional footprint overrides let designers correct decorative or unusually authored bounds.
 */
USTRUCT(BlueprintType)
struct FBattlefieldActorEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset")
	TSoftClassPtr<AActor> ActorClass;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset", meta = (ClampMin = "0.01"))
	float Weight = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bounds",
		meta = (ClampMin = "0", Units = "cm", DisplayName = "Footprint Size X (0 = Measure)"))
	float FootprintOverrideX = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bounds",
		meta = (ClampMin = "0", Units = "cm", DisplayName = "Footprint Size Y (0 = Measure)"))
	float FootprintOverrideY = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Placement", meta = (ClampMin = "0.01"))
	float MinUniformScale = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Placement", meta = (ClampMin = "0.01"))
	float MaxUniformScale = 1.0f;

	/** @brief Offsets the grounded visual mesh along the Landscape normal, or world Z when unaligned. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Placement", meta = (Units = "cm"))
	float ZOffset = 0.0f;

	/** @brief Added after generated facing for assets whose visual forward direction is not local +X. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Placement", meta = (Units = "deg"))
	float YawOffsetDegrees = 0.0f;
};

/**
 * @brief Configures a modular prop that is placed end to end in a battlefield run.
 * Its real Blueprint bounds supply length and width unless either override is non-zero.
 */
USTRUCT(BlueprintType)
struct FBattlefieldLineActorEntry : public FBattlefieldActorEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Line Placement")
	EBattlefieldLineDirection AuthoredLengthDirection = EBattlefieldLineDirection::PositiveX;
};

/**
 * @brief Configures a ground-projected decal and its authored default world size.
 * GlobalDecalSizeMultiplier scales both axes without requiring duplicate material entries.
 */
USTRUCT(BlueprintType)
struct FBattlefieldDecalEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset")
	TSoftObjectPtr<UMaterialInterface> Material;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset", meta = (ClampMin = "0.01"))
	float Weight = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Size", meta = (ClampMin = "1", Units = "cm"))
	FVector2D DefaultSize = FVector2D(500.0, 500.0);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Size", meta = (ClampMin = "1", Units = "cm"))
	float ProjectionDepth = 500.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Placement", meta = (Units = "cm"))
	float SurfaceOffset = 4.0f;
};

/**
 * @brief Creates non-convex, spline-carved battlefield areas split into Soviet, no-man's-land, and
 * German slices. It places opposing weaponry, grouped obstacles, faction spoils, trees, and decals
 * with shared bounds-aware overlap prevention and per-placement landscape snapping.
 * @note Connect candidate point data to In and optional blocking spatial data to Excluded Bounds.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UPCGCreateBattlefieldAreaSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spawner; }
#endif

	virtual bool UseSeed() const override { return false; }

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;

public:
	// --- Areas ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Areas")
	int32 RandomSeed = 1945;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Areas", meta = (ClampMin = "0"))
	int32 MinAreasToCreate = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Areas", meta = (ClampMin = "0"))
	int32 MaxAreasToCreate = 3;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Areas", meta = (ClampMin = "1000", Units = "cm"))
	float MinAreaLength = 10000.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Areas", meta = (ClampMin = "1000", Units = "cm"))
	float MaxAreaLength = 18000.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Areas", meta = (ClampMin = "1000", Units = "cm"))
	float MinAreaWidth = 7000.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Areas", meta = (ClampMin = "1000", Units = "cm"))
	float MaxAreaWidth = 12000.0f;

	/** @brief Uses each input point's yaw; otherwise every accepted area receives a deterministic random yaw. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Areas")
	bool bUseInputPointRotation = false;

	/** @brief Empty space reserved between the complete spline polygons of separate areas. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Areas", meta = (ClampMin = "0", Units = "cm"))
	float AreaClearance = 1000.0f;

	/** @brief Empty space added between every actor/decal footprint generated inside an area. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Areas", meta = (ClampMin = "0", Units = "cm"))
	float PlacementClearance = 50.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Areas", meta = (ClampMin = "1"))
	int32 PlacementAttemptsPerItem = 64;

	// --- Non-convex spline shape ---

	/** @brief Number of stations on each long edge; more stations allow more detailed concavity. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Non-Convex Shape", meta = (ClampMin = "4", ClampMax = "32"))
	int32 BoundaryStationsPerSide = 8;

	/** @brief Random fractional width variation applied before deliberate inward indentations. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Non-Convex Shape", meta = (ClampMin = "0", ClampMax = "0.45"))
	float BoundaryWidthVariation = 0.12f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Non-Convex Shape", meta = (ClampMin = "0"))
	int32 MinBoundaryIndentations = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Non-Convex Shape", meta = (ClampMin = "0"))
	int32 MaxBoundaryIndentations = 3;

	/** @brief Fraction of area half-width pushed inward at selected stations to create reflex corners. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Non-Convex Shape", meta = (ClampMin = "0", ClampMax = "0.7"))
	float BoundaryIndentDepth = 0.28f;

	/** @brief Spacing used while sampling the complete polygon against Excluded Bounds. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Non-Convex Shape", meta = (ClampMin = "25", Units = "cm"))
	float ExclusionSampleSpacing = 300.0f;

	// --- Slice proportions ---

	/** @brief Relative longitudinal size; all three slice values are normalized together. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Slice Proportions", meta = (ClampMin = "0.01"))
	float SovietSliceProportion = 0.35f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Slice Proportions", meta = (ClampMin = "0.01"))
	float NoMansLandSliceProportion = 0.30f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Slice Proportions", meta = (ClampMin = "0.01"))
	float GermanSliceProportion = 0.35f;

	/** @brief Keeps section-specific objects away from the boundary between adjacent slices. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Slice Proportions", meta = (ClampMin = "0", Units = "cm"))
	float SliceEdgePadding = 250.0f;

	// --- Soviet weaponry ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Soviet Weaponry")
	TArray<FBattlefieldActorEntry> SovietWeaponry;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Soviet Weaponry", meta = (ClampMin = "0"))
	int32 MinSovietWeaponryPerArea = 3;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Soviet Weaponry", meta = (ClampMin = "0"))
	int32 MaxSovietWeaponryPerArea = 8;

	/** @brief Fraction of the Soviet slice used around its center; 0 forms a tight group, 1 uses the full slice. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Soviet Weaponry", meta = (ClampMin = "0.05", ClampMax = "1"))
	float SovietWeaponrySpread = 0.85f;

	// --- German weaponry ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "German Weaponry")
	TArray<FBattlefieldActorEntry> GermanWeaponry;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "German Weaponry", meta = (ClampMin = "0"))
	int32 MinGermanWeaponryPerArea = 3;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "German Weaponry", meta = (ClampMin = "0"))
	int32 MaxGermanWeaponryPerArea = 8;

	/** @brief Fraction of the German slice used around its center; 0 forms a tight group, 1 uses the full slice. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "German Weaponry", meta = (ClampMin = "0.05", ClampMax = "1"))
	float GermanWeaponrySpread = 0.85f;

	// --- No man's land: barbed wire ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "No Man's Land|Barbed Wire")
	TArray<FBattlefieldLineActorEntry> BarbedWire;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "No Man's Land|Barbed Wire", meta = (ClampMin = "0"))
	int32 MinBarbedWireRunsPerArea = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "No Man's Land|Barbed Wire", meta = (ClampMin = "0"))
	int32 MaxBarbedWireRunsPerArea = 3;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "No Man's Land|Barbed Wire", meta = (ClampMin = "1"))
	int32 MinBarbedWirePiecesPerRun = 3;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "No Man's Land|Barbed Wire", meta = (ClampMin = "1"))
	int32 MaxBarbedWirePiecesPerRun = 8;

	/** @brief Requested end-to-end gap between measured barbed-wire pieces. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "No Man's Land|Barbed Wire",
		meta = (ClampMin = "0", Units = "cm"))
	float BarbedWirePieceGap = 10.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "No Man's Land|Barbed Wire",
		meta = (ClampMin = "0", ClampMax = "45", Units = "deg"))
	float BarbedWireMaxRunAngleDegrees = 15.0f;

	// --- No man's land: hedgehogs ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "No Man's Land|Hedgehogs")
	TArray<FBattlefieldActorEntry> Hedgehogs;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "No Man's Land|Hedgehogs", meta = (ClampMin = "0"))
	int32 MinHedgehogGroupsPerArea = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "No Man's Land|Hedgehogs", meta = (ClampMin = "0"))
	int32 MaxHedgehogGroupsPerArea = 3;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "No Man's Land|Hedgehogs", meta = (ClampMin = "1"))
	int32 MinHedgehogsPerGroup = 2;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "No Man's Land|Hedgehogs", meta = (ClampMin = "1"))
	int32 MaxHedgehogsPerGroup = 6;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "No Man's Land|Hedgehogs", meta = (ClampMin = "0", Units = "cm"))
	float MinHedgehogSpacing = 250.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "No Man's Land|Hedgehogs", meta = (ClampMin = "0", Units = "cm"))
	float MaxHedgehogSpacing = 700.0f;

	// --- Spoils of war ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Spoils Of War|Soviet")
	TArray<FBattlefieldActorEntry> SovietSpoilsOfWar;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Spoils Of War|Soviet", meta = (ClampMin = "0"))
	int32 MinSovietSpoilsPerArea = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Spoils Of War|Soviet", meta = (ClampMin = "0"))
	int32 MaxSovietSpoilsPerArea = 2;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Spoils Of War|German")
	TArray<FBattlefieldActorEntry> GermanSpoilsOfWar;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Spoils Of War|German", meta = (ClampMin = "0"))
	int32 MinGermanSpoilsPerArea = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Spoils Of War|German", meta = (ClampMin = "0"))
	int32 MaxGermanSpoilsPerArea = 2;

	// --- Global trees ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Global|Trees")
	TArray<FBattlefieldActorEntry> Trees;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Global|Trees", meta = (ClampMin = "0"))
	int32 MinTreesPerArea = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Global|Trees", meta = (ClampMin = "0"))
	int32 MaxTreesPerArea = 15;

	// --- Global decals ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Global|Decals")
	TArray<FBattlefieldDecalEntry> Decals;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Global|Decals", meta = (ClampMin = "0"))
	int32 MinDecalsPerArea = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Global|Decals", meta = (ClampMin = "0"))
	int32 MaxDecalsPerArea = 10;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Global|Decals", meta = (ClampMin = "0.01"))
	float GlobalDecalSizeMultiplier = 1.0f;

	// --- Terrain ---

	// Retained only so existing node assets deserialize cleanly after cached projection replaced traces.
	UPROPERTY(meta = (DeprecatedProperty,
		DeprecationMessage = "Landscape grounding now uses cached PCG projection instead of traces."))
	float GroundTraceUp = 15000.0f;

	UPROPERTY(meta = (DeprecatedProperty,
		DeprecationMessage = "Landscape grounding now uses cached PCG projection instead of traces."))
	float GroundTraceDown = 40000.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Terrain", meta = (ClampMin = "0", ClampMax = "89", Units = "deg"))
	float MaxGroundSlopeDegrees = 50.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Terrain")
	bool bAlignActorsToGround = true;
};

/**
 * @brief Main-thread execution element that measures configured Blueprints and registers generated
 * spline, actor, and decal-anchor resources before emitting area and placement point data.
 */
class FPCGCreateBattlefieldAreaElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
};
