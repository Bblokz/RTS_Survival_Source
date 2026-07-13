// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "PCGCreateMetalField.generated.h"

class AActor;

/**
 * @brief Cardinal axis describing how a Blueprint fence piece is authored.
 * The pivot sits in the middle of the piece; +X means the unrotated fence extends along its local X,
 * so its pivot is centered on the X span. Non-standard assets can declare a different forward axis.
 */
UENUM(BlueprintType)
enum class EMetalFenceDirection : uint8
{
	PositiveX UMETA(DisplayName = "+X"),
	NegativeX UMETA(DisplayName = "-X"),
	PositiveY UMETA(DisplayName = "+Y"),
	NegativeY UMETA(DisplayName = "-Y")
};

/**
 * @brief One bounds-measured Blueprint used as a tower, auxiliary, building, or scattered metal object.
 * Weight biases weighted selection; the footprint overrides feed spacing math without rescaling the actor.
 */
USTRUCT(BlueprintType)
struct FMetalFieldActorEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset")
	TSoftClassPtr<AActor> ActorClass;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset", meta = (ClampMin = "0.01"))
	float Weight = 1.0f;

	/** @brief Local-X footprint size used for spacing; 0 measures the spawned Blueprint bounds. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bounds",
		meta = (ClampMin = "0", Units = "cm", DisplayName = "Footprint Size X (0 = Measure)"))
	float FootprintOverrideX = 0.0f;

	/** @brief Local-Y footprint size used for spacing; 0 measures the spawned Blueprint bounds. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bounds",
		meta = (ClampMin = "0", Units = "cm", DisplayName = "Footprint Size Y (0 = Measure)"))
	float FootprintOverrideY = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Placement", meta = (ClampMin = "0.01"))
	float MinUniformScale = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Placement", meta = (ClampMin = "0.01"))
	float MaxUniformScale = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Placement", meta = (Units = "cm"))
	float ZOffset = 0.0f;

	/** @brief Added to the generated yaw for assets authored with a nonstandard forward axis. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Placement", meta = (Units = "deg"))
	float YawOffsetDegrees = 0.0f;
};

/**
 * @brief One standalone Blueprint fence piece used to enclose a metal building.
 * The pivot is expected in the middle. X/Y bounds default to the measured Blueprint bounds when 0, and
 * the authored forward direction keeps the pivot centered on whichever axis the piece extends along.
 */
USTRUCT(BlueprintType)
struct FMetalFieldFenceEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset")
	TSoftClassPtr<AActor> ActorClass;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset", meta = (ClampMin = "0.01"))
	float Weight = 1.0f;

	/** @brief X bound of the fence piece; 0 measures the spawned Blueprint bounds. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bounds",
		meta = (ClampMin = "0", Units = "cm", DisplayName = "Fence Bound X (0 = Measure)"))
	float FootprintOverrideX = 0.0f;

	/** @brief Y bound of the fence piece; 0 measures the spawned Blueprint bounds. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bounds",
		meta = (ClampMin = "0", Units = "cm", DisplayName = "Fence Bound Y (0 = Measure)"))
	float FootprintOverrideY = 0.0f;

	/** @brief Axis the unrotated fence extends along; +X keeps the pivot centered on the local X span. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bounds")
	EMetalFenceDirection AuthoredForwardDirection = EMetalFenceDirection::PositiveX;

	/** @brief Permitted longitudinal overlap between adjacent fence pieces, used to hide seams. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Placement", meta = (ClampMin = "0", Units = "cm"))
	float EndOverlap = 5.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Placement", meta = (Units = "cm"))
	float ZOffset = 0.0f;
};

/**
 * @brief Creates separate, non-overlapping metal resource fields from candidate points. Each accepted
 * field deterministically becomes one of three kinds: a cluster of metal towers with scattered
 * auxiliaries, a single metal building enclosed by a fence, or a loose scatter of metal objects with
 * auxiliaries. Every Blueprint is independently spawned once to measure its bounds before placement.
 * @note Connect candidate point data to In and optional blocking spatial data to Excluded Bounds.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UPCGCreateMetalFieldSettings : public UPCGSettings
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
	// --- Fields ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Fields")
	int32 RandomSeed = 9173;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Fields", meta = (ClampMin = "0"))
	int32 MinFieldsToCreate = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Fields", meta = (ClampMin = "0"))
	int32 MaxFieldsToCreate = 3;

	/** @brief Scatter radius for tower and regular-object fields; also their reserved footprint. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Fields", meta = (ClampMin = "100", Units = "cm"))
	float MinFieldRadius = 2500.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Fields", meta = (ClampMin = "100", Units = "cm"))
	float MaxFieldRadius = 5000.0f;

	/** @brief Minimum empty spacing kept between the reserved footprints of two separate fields. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Fields", meta = (ClampMin = "0", Units = "cm"))
	float FieldClearance = 400.0f;

	/** @brief Minimum empty spacing kept between the measured footprints of any two placed actors. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Fields", meta = (ClampMin = "0", Units = "cm"))
	float MinActorClearance = 50.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Fields", meta = (ClampMin = "1"))
	int32 PlacementAttemptsPerItem = 64;

	// --- Field type selection ---

	/** @brief Relative chance of a field becoming a metal-tower cluster (only if tower actors exist). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Field Type Selection", meta = (ClampMin = "0"))
	float TowerFieldWeight = 1.0f;

	/** @brief Relative chance of a field becoming a fenced metal building (only if building actors exist). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Field Type Selection", meta = (ClampMin = "0"))
	float BuildingFieldWeight = 1.0f;

	/** @brief Relative chance of a field becoming a regular metal scatter (only if regular actors exist). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Field Type Selection", meta = (ClampMin = "0"))
	float RegularMetalFieldWeight = 1.0f;

	// --- Terrain ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Terrain", meta = (ClampMin = "0", Units = "cm"))
	float GroundTraceUp = 15000.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Terrain", meta = (ClampMin = "0", Units = "cm"))
	float GroundTraceDown = 40000.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Terrain", meta = (ClampMin = "0", ClampMax = "89", Units = "deg"))
	float MaxGroundSlopeDegrees = 45.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Terrain")
	bool bAlignActorsToGround = true;

	// --- Metal Tower Field ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Metal Tower Field")
	TArray<FMetalFieldActorEntry> MetalTowerActors;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Metal Tower Field")
	TArray<FMetalFieldActorEntry> MetalTowerAuxiliaryActors;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Metal Tower Field", meta = (ClampMin = "1"))
	int32 MinTowersPerField = 2;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Metal Tower Field", meta = (ClampMin = "1"))
	int32 MaxTowersPerField = 4;

	/** @brief Hard minimum separation between two towers' centers. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Metal Tower Field", meta = (ClampMin = "0", Units = "cm"))
	float MinTowerToTowerDistance = 900.0f;

	/** @brief Each tower after the first must sit within this distance of an existing tower (keeps the cluster connected). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Metal Tower Field", meta = (ClampMin = "0", Units = "cm"))
	float MaxTowerToTowerDistance = 2500.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Metal Tower Field", meta = (ClampMin = "0"))
	int32 MinTowerAuxiliariesPerField = 3;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Metal Tower Field", meta = (ClampMin = "0"))
	int32 MaxTowerAuxiliariesPerField = 7;

	/** @brief Inner radius of the ring around an owning tower where an auxiliary may spawn. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Metal Tower Field", meta = (ClampMin = "0", Units = "cm"))
	float MinTowerAuxiliaryToTowerDistance = 300.0f;

	/** @brief Outer radius of the ring around an owning tower where an auxiliary may spawn. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Metal Tower Field", meta = (ClampMin = "0", Units = "cm"))
	float MaxTowerAuxiliaryToTowerDistance = 900.0f;

	/** @brief Hard minimum separation between two auxiliaries' centers. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Metal Tower Field", meta = (ClampMin = "0", Units = "cm"))
	float MinTowerAuxiliarySpacing = 200.0f;

	/** @brief Each auxiliary after an owning tower's first must sit within this distance of a sibling auxiliary. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Metal Tower Field", meta = (ClampMin = "0", Units = "cm"))
	float MaxTowerAuxiliarySpacing = 800.0f;

	// --- Metal Building Field ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Metal Building Field")
	TArray<FMetalFieldActorEntry> MetalBuildingActors;

	/** @brief A single fence type is chosen per building and tiled around it. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Metal Building Field")
	TArray<FMetalFieldFenceEntry> MetalBuildingFences;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Metal Building Field")
	bool bEncloseBuildingWithFence = true;

	/** @brief Distance from the building footprint to the surrounding fence rectangle. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Metal Building Field", meta = (ClampMin = "0", Units = "cm"))
	float FenceDistanceFromBuilding = 600.0f;

	/** @brief Optional corner opening; 0 lets perpendicular sides meet for a closed enclosure. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Metal Building Field", meta = (ClampMin = "0", Units = "cm"))
	float FenceCornerClearance = 0.0f;

	// --- Regular Metal Field ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Regular Metal Field")
	TArray<FMetalFieldActorEntry> RegularMetalActors;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Regular Metal Field")
	TArray<FMetalFieldActorEntry> RegularMetalAuxiliaryActors;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Regular Metal Field", meta = (ClampMin = "1"))
	int32 MinRegularMetalObjects = 3;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Regular Metal Field", meta = (ClampMin = "1"))
	int32 MaxRegularMetalObjects = 8;

	/** @brief Hard minimum separation between two metal objects' centers. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Regular Metal Field", meta = (ClampMin = "0", Units = "cm"))
	float MinRegularMetalSpacing = 400.0f;

	/** @brief Each object after the first must sit within this distance of an existing object (keeps the scatter cohesive). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Regular Metal Field", meta = (ClampMin = "0", Units = "cm"))
	float MaxRegularMetalSpacing = 1600.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Regular Metal Field", meta = (ClampMin = "0"))
	int32 MinRegularAuxiliariesPerField = 2;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Regular Metal Field", meta = (ClampMin = "0"))
	int32 MaxRegularAuxiliariesPerField = 6;

	/** @brief Inner radius of the ring around an owning object where an auxiliary may spawn. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Regular Metal Field", meta = (ClampMin = "0", Units = "cm"))
	float MinRegularAuxiliaryToObjectDistance = 250.0f;

	/** @brief Outer radius of the ring around an owning object where an auxiliary may spawn. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Regular Metal Field", meta = (ClampMin = "0", Units = "cm"))
	float MaxRegularAuxiliaryToObjectDistance = 800.0f;

	/** @brief Hard minimum separation between two auxiliaries' centers. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Regular Metal Field", meta = (ClampMin = "0", Units = "cm"))
	float MinRegularAuxiliarySpacing = 200.0f;

	/** @brief Each auxiliary after an owning object's first must sit within this distance of a sibling auxiliary. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Regular Metal Field", meta = (ClampMin = "0", Units = "cm"))
	float MaxRegularAuxiliarySpacing = 700.0f;
};

/**
 * @brief Main-thread execution element that measures configured Blueprints and registers every generated
 * actor as a PCG-managed resource, then emits per-field bounds and per-actor placement points.
 */
class FPCGCreateMetalFieldElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
};
