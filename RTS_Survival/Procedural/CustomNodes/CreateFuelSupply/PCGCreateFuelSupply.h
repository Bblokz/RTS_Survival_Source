// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "PCGCreateFuelSupply.generated.h"

class AActor;

UENUM(BlueprintType)
enum class EFuelCardinalDirection : uint8
{
	PositiveX UMETA(DisplayName = "+X"),
	NegativeX UMETA(DisplayName = "-X"),
	PositiveY UMETA(DisplayName = "+Y"),
	NegativeY UMETA(DisplayName = "-Y")
};

/**
 * @brief Defines one bounds-measured Blueprint that can fill a depot or tank role.
 * Weight permits art-directed variety without assuming that entries share a footprint.
 */
USTRUCT(BlueprintType)
struct FFuelSupplyActorEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset")
	TSoftClassPtr<AActor> ActorClass;

	/** @brief Local-X footprint size; 0 uses the pre-spawned Blueprint's measured bounds. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bounds",
		meta = (ClampMin = "0", Units = "cm", DisplayName = "Footprint Size X (0 = Measure)"))
	float FootprintOverrideX = 0.0f;

	/** @brief Local-Y footprint size; 0 uses the pre-spawned Blueprint's measured bounds. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bounds",
		meta = (ClampMin = "0", Units = "cm", DisplayName = "Footprint Size Y (0 = Measure)"))
	float FootprintOverrideY = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset", meta = (ClampMin = "0.01"))
	float Weight = 1.0f;

	/** @brief Additional footprint clearance used only for this Blueprint. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Placement", meta = (ClampMin = "0", Units = "cm"))
	float BoundsClearance = 100.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Placement", meta = (Units = "cm"))
	float ZOffset = 0.0f;

	/** @brief Added after the generated yaw to accommodate nonstandard Blueprint forward axes. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Placement", meta = (Units = "deg"))
	float YawOffsetDegrees = 0.0f;
};

/**
 * @brief Defines an arbitrary destructible pipe Blueprint through its footprint and connector sides.
 * Connector-aware rotation and reversible traversal let irregular authored pieces fill routed networks.
 */
USTRUCT(BlueprintType)
struct FFuelPipeActorEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset")
	TSoftClassPtr<AActor> ActorClass;

	/** @brief X size that preserves the measured pivot position; 0 uses the measured footprint. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bounds",
		meta = (ClampMin = "0", Units = "cm", DisplayName = "Footprint Size X (0 = Measure)"))
	float FootprintOverrideX = 0.0f;

	/** @brief Y size that preserves the measured pivot position; 0 uses the measured footprint. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bounds",
		meta = (ClampMin = "0", Units = "cm", DisplayName = "Footprint Size Y (0 = Measure)"))
	float FootprintOverrideY = 0.0f;

	/** @brief Side where traversal enters the authored piece. Default flow therefore points toward +X. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Connections")
	EFuelCardinalDirection StartConnectorDirection = EFuelCardinalDirection::NegativeX;

	/** @brief Authored exit/forward side. Defaults to +X; use +Y/-Y to describe elbows. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Connections")
	EFuelCardinalDirection ForwardDirection = EFuelCardinalDirection::PositiveX;

	/** @brief Allows the generator to exchange start/end connectors without mirroring the actor. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Connections")
	bool bCanReverse = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset", meta = (ClampMin = "0.01"))
	float Weight = 1.0f;

	/** @brief Amount each end may overlap its neighbor to hide seams. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Connections", meta = (ClampMin = "0", Units = "cm"))
	float EndOverlap = 5.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Connections", meta = (Units = "cm"))
	float ZOffset = 0.0f;
};

/**
 * @brief Defines one standalone Blueprint fence piece used to enclose generated fuel tanks.
 * Its authored forward direction and footprint allow differently oriented fence assets to mix.
 */
USTRUCT(BlueprintType)
struct FFuelFenceActorEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset")
	TSoftClassPtr<AActor> ActorClass;

	/** @brief X size that preserves the measured pivot position; 0 uses the measured footprint. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bounds",
		meta = (ClampMin = "0", Units = "cm", DisplayName = "Footprint Size X (0 = Measure)"))
	float FootprintOverrideX = 0.0f;

	/** @brief Y size that preserves the measured pivot position; 0 uses the measured footprint. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bounds",
		meta = (ClampMin = "0", Units = "cm", DisplayName = "Footprint Size Y (0 = Measure)"))
	float FootprintOverrideY = 0.0f;

	/** @brief Direction in which the unrotated fence piece extends; defaults to local +X. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bounds")
	EFuelCardinalDirection AuthoredForwardDirection = EFuelCardinalDirection::PositiveX;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset", meta = (ClampMin = "0.01"))
	float Weight = 1.0f;

	/** @brief Permitted longitudinal overlap with adjacent fence pieces. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Placement", meta = (ClampMin = "0", Units = "cm"))
	float EndOverlap = 5.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Placement", meta = (Units = "cm"))
	float ZOffset = 0.0f;
};

/**
 * @brief Generates complete, PCG-managed fuel facilities within spatial input while respecting
 * exclusion data. Every Blueprint is independently spawned for measurement and final placement.
 * @note Connect the desired area to In and optional blocking volumes to Exclusion.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UPCGCreateFuelSupplySettings : public UPCGSettings
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
	// --- Assets ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Assets|Supply Depots")
	TArray<FFuelSupplyActorEntry> SupplyDepots;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Assets|Fuel Tanks")
	TArray<FFuelSupplyActorEntry> FuelTanks;

	/** @brief Destructible pipe Blueprints. They always spawn as individual actors, never instances. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Assets|Fuel Pipes")
	TArray<FFuelPipeActorEntry> FuelPipes;

	/** @brief Standalone Blueprint fences placed around tanks after pipe routes are known. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Assets|Fuel Tank Fences")
	TArray<FFuelFenceActorEntry> FuelTankFences;

	// --- Facility layout ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Facility Layout")
	int32 RandomSeed = 7319;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Facility Layout", meta = (ClampMin = "1"))
	int32 MinSupplyDepots = 2;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Facility Layout", meta = (ClampMin = "1"))
	int32 MaxSupplyDepots = 4;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Facility Layout", meta = (ClampMin = "0"))
	int32 MinFuelTanks = 2;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Facility Layout", meta = (ClampMin = "0"))
	int32 MaxFuelTanks = 7;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Facility Layout", meta = (ClampMin = "0", Units = "cm"))
	float GlobalActorClearance = 250.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Facility Layout", meta = (ClampMin = "1"))
	int32 PlacementAttemptsPerActor = 80;

	/** @brief Tanks prefer this radius around a depot but still obey all measured footprints. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Facility Layout", meta = (ClampMin = "0", Units = "cm"))
	float FuelTankClusterRadius = 4500.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Facility Layout")
	bool bUseRightAngleActorRotations = true;

	// --- Pipe network ---

	/** @brief Chance to add each useful depot-to-depot loop after the guaranteed spanning network. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pipe Network", meta = (ClampMin = "0", ClampMax = "1"))
	float DepotLoopChance = 0.25f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pipe Network", meta = (ClampMin = "100", Units = "cm"))
	float RouteGridSize = 300.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pipe Network", meta = (ClampMin = "100"))
	int32 MaxRouteSearchCells = 24000;

	/** @brief Clearance between routed pipes and unrelated generated actors or exclusions. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pipe Network", meta = (ClampMin = "0", Units = "cm"))
	float PipeObstacleClearance = 125.0f;

	/** @brief Gap outside each depot/tank footprint before its first pipe piece begins. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pipe Network", meta = (ClampMin = "0", Units = "cm"))
	float ConnectionClearance = 25.0f;

	/** @brief Permits small longitudinal scaling so routes close cleanly without partial pieces. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pipe Network")
	bool bAllowPipeLengthScaling = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pipe Network",
		meta = (ClampMin = "0.1", ClampMax = "1.0", EditCondition = "bAllowPipeLengthScaling"))
	float MinPipeLengthScale = 0.8f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pipe Network",
		meta = (ClampMin = "1.0", ClampMax = "2.0", EditCondition = "bAllowPipeLengthScaling"))
	float MaxPipeLengthScale = 1.2f;

	// --- Terrain ---

	/** @brief Moves every generated actor vertically after ground projection; negative values sink actors into the landscape. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Terrain", meta = (Units = "cm"))
	float GlobalActorZOffset = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Terrain")
	bool bProjectActorsToGround = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Terrain", meta = (ClampMin = "0", ClampMax = "89", Units = "deg"))
	float MaxGroundSlopeDegrees = 25.0f;

	// --- Fuel tank fences ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Fuel Tank Fences")
	bool bGenerateFuelTankFences = true;

	/** @brief Distance from each tank footprint to the surrounding fence rectangle. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Fuel Tank Fences", meta = (ClampMin = "0", Units = "cm"))
	float FenceDistanceFromTank = 600.0f;

	/** @brief Additional opening on both sides of every pipe crossing through a fence. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Fuel Tank Fences", meta = (ClampMin = "0", Units = "cm"))
	float FencePipeGap = 250.0f;

	/** @brief Keeps fence pieces away from unrelated depots, tanks, and generated actors. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Fuel Tank Fences", meta = (ClampMin = "0", Units = "cm"))
	float FenceObstacleClearance = 50.0f;

	/** @brief Optional corner opening; 0 lets perpendicular sides overlap for a closed enclosure. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Fuel Tank Fences", meta = (ClampMin = "0", Units = "cm"))
	float FenceCornerClearance = 0.0f;
};

/**
 * @brief Main-thread PCG element that measures Blueprint construction-script bounds, generates a
 * deterministic obstacle-aware network, and registers every standalone actor as a managed resource.
 */
class FPCGCreateFuelSupplyElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
};
