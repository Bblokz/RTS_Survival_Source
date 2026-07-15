// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#pragma once

#include "PCGSettings.h"
#include "PCGContext.h"
#include "PCGEnemyBaseSetupTypes.h"
#include "PCGFortifiedBuilding.generated.h"

/**
 * @brief Settings for the Fortified Building node. Places one military building (a bunker, a
 * command post...) at each input point, wraps it in a protective sandbag wall (flanks, front or
 * full ring) and optionally lays an anti-tank obstacle belt and barbed wire in front. Ammo / fuel
 * decorators, foliage and ground decals can be scattered around each building. The facing (which
 * way the building and its front cover point) comes from the facing mode: the point's own
 * rotation, a fixed yaw, or toward / away from the AimTarget input.
 * @note Buildings, Sandbags, Obstacles, BarbedWire and Decorators output points-with-bounds per
 * category; TotalBounds outputs one enclosing box per building cluster; Foliage outputs points
 * with a "Mesh" attribute for a Static Mesh Spawner.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UPCGFortifiedBuildingSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	// ~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spawner; }
#endif

	/** @brief Deterministic from RandomSeed, not the PCG component seed. */
	virtual bool UseSeed() const override { return false; }

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	// ~End UPCGSettings interface

public:
	// --- General ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General")
	int32 RandomSeed = 42;

	/** @brief How each building decides the direction it faces (and thus where its front cover goes). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General")
	EEnemyFacingMode FacingMode = EEnemyFacingMode::AimTowardsTarget;

	/** @brief Fixed facing yaw (degrees) used when FacingMode is FixedYaw. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General",
		meta = (EditCondition = "FacingMode == EEnemyFacingMode::FixedYaw"))
	float FixedYawDegrees = 0.0f;

	/** @brief Extra clearance (cm) reserved around every building before overlap tests. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General", meta = (ClampMin = "0"))
	float BuildingSpacingExtra = 100.0f;

	// --- Buildings ---

	/** @brief Building Blueprints; one is weighted-picked per input point. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Buildings")
	TArray<FEnemyBlueprintEntry> Buildings;

	// --- Defensive works ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Sandbags")
	FEnemySandbagSettings Sandbags;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Obstacle Belt")
	FEnemyObstacleBeltSettings ObstacleBelt;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Barbed Wire")
	FEnemyBarbedWireSettings BarbedWire;

	// --- Decoration ---

	/** @brief Decorator profiles (ammo crates, fuel tanks...) scattered around each building. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Decorators")
	TArray<FEnemyDecoratorProfile> DecoratorProfiles;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Foliage")
	FEnemyFoliageSettings Foliage;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Decals")
	FEnemyDecalSettings Decals;
};

/**
 * @brief Execution element for Fortified Building. Resolves assets and real bounds, drives the
 * shared placement builder once per input point, spawns the managed actors and emits the
 * categorized point-with-bounds outputs. Runs on the game thread; never cached (spawns actors).
 */
class FPCGFortifiedBuildingElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
};
