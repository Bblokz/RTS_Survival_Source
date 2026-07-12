// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "PCGCreateFuelSupply.generated.h"

class AActor;

UENUM(BlueprintType)
enum class EFuelPipeBoundsAxis : uint8
{
	Automatic UMETA(DisplayName = "Automatic (Longest Horizontal Axis)"),
	LocalX UMETA(DisplayName = "Local X"),
	LocalY UMETA(DisplayName = "Local Y")
};

UENUM(BlueprintType)
enum class EFuelPipePieceType : uint8
{
	Straight,
	Corner
};

UENUM(BlueprintType)
enum class EFuelPipeTurnDirection : uint8
{
	Either,
	Left,
	Right
};

/**
 * @brief Defines one bounds-measured Blueprint that can fill a depot, tank, or dressing role.
 * Weight permits art-directed variety without assuming that entries share a footprint.
 */
USTRUCT(BlueprintType)
struct FFuelSupplyActorEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset")
	TSoftClassPtr<AActor> ActorClass;

	/** @brief Pivot-centered local-X footprint size; 0 uses the pre-spawned Blueprint's measured bounds. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bounds", meta = (ClampMin = "0", Units = "cm"))
	float FootprintOverrideX = 0.0f;

	/** @brief Pivot-centered local-Y footprint size; 0 uses the pre-spawned Blueprint's measured bounds. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bounds", meta = (ClampMin = "0", Units = "cm"))
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
 * @brief Defines one destructible pipe Blueprint and how its measured local bounds represent flow.
 * Straight pieces are tiled by their own measured length; corner pieces occupy routed bends.
 */
USTRUCT(BlueprintType)
struct FFuelPipeActorEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset")
	TSoftClassPtr<AActor> ActorClass;

	/** @brief Pivot-centered local-X footprint size; 0 uses the pre-spawned Blueprint's measured bounds. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bounds", meta = (ClampMin = "0", Units = "cm"))
	float FootprintOverrideX = 0.0f;

	/** @brief Pivot-centered local-Y footprint size; 0 uses the pre-spawned Blueprint's measured bounds. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bounds", meta = (ClampMin = "0", Units = "cm"))
	float FootprintOverrideY = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset")
	EFuelPipePieceType PieceType = EFuelPipePieceType::Straight;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset", meta = (ClampMin = "0.01"))
	float Weight = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bounds")
	EFuelPipeBoundsAxis LengthAxis = EFuelPipeBoundsAxis::Automatic;

	/** @brief Relevant to corners whose local X-to-Y flow only supports one turn direction. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bounds",
		meta = (EditCondition = "PieceType == EFuelPipePieceType::Corner", EditConditionHides))
	EFuelPipeTurnDirection TurnDirection = EFuelPipeTurnDirection::Either;

	/** @brief Added to the bounds-inferred rotation for Blueprints with unusual authoring axes. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bounds", meta = (Units = "deg"))
	float YawOffsetDegrees = 0.0f;

	/** @brief Amount each end may overlap its neighbor to hide seams. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Connections", meta = (ClampMin = "0", Units = "cm"))
	float EndOverlap = 5.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Connections", meta = (Units = "cm"))
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

	/** @brief Optional containers, fences, barrels, crates, barriers, pumps, or similar actors. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Assets|Industrial Dressing")
	TArray<FFuelSupplyActorEntry> IndustrialDressing;

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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Terrain")
	bool bProjectActorsToGround = true;

	/** @brief Tilts dressing actors to terrain; depots, tanks, and pipes remain upright. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Terrain")
	bool bAlignIndustrialDressingToGround = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Terrain", meta = (ClampMin = "0", ClampMax = "89", Units = "deg"))
	float MaxGroundSlopeDegrees = 25.0f;

	// --- Industrial dressing ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Industrial Dressing", meta = (ClampMin = "0"))
	int32 MinDressingActorsPerDepot = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Industrial Dressing", meta = (ClampMin = "0"))
	int32 MaxDressingActorsPerDepot = 3;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Industrial Dressing", meta = (ClampMin = "0", Units = "cm"))
	float DressingRadius = 3500.0f;
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
