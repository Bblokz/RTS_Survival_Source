// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#pragma once

#include "PCGSettings.h"
#include "PCGContext.h"
#include "PCGEnemyBaseSetupTypes.h"
#include "PCGBunkerLine.generated.h"

/**
 * @brief Settings for the Bunker Line node. At each input point it lays a row of bunkers facing a
 * chosen direction, then an anti-tank obstacle belt (straight, arc or staggered double row) and
 * barbed wire in front, plus an optional sandbag parapet wrapping the row. Ammo / fuel decorators,
 * foliage and decals can be scattered around the line. The direction the line faces comes from the
 * facing mode (point rotation, fixed yaw, or toward / away from the AimTarget input).
 * @note Bunkers, Sandbags, Obstacles, BarbedWire and Decorators output points-with-bounds per
 * category; TotalBounds outputs one enclosing box per line; Foliage outputs points for a Static
 * Mesh Spawner.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UPCGBunkerLineSettings : public UPCGSettings
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

	/** @brief How the whole line decides the direction it faces (and where the belt / wire go). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General")
	EEnemyFacingMode FacingMode = EEnemyFacingMode::AimTowardsTarget;

	/** @brief Fixed facing yaw (degrees) used when FacingMode is FixedYaw. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General",
		meta = (EditCondition = "FacingMode == EEnemyFacingMode::FixedYaw"))
	float FixedYawDegrees = 0.0f;

	// --- Bunkers ---

	/** @brief Bunker Blueprints; one is weighted-picked per position along the row. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bunkers")
	TArray<FEnemyBlueprintEntry> Bunkers;

	/** @brief Total length (cm) of the bunker row along its right axis. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bunkers", meta = (ClampMin = "0"))
	float LineLength = 3000.0f;

	/** @brief Distance (cm) between consecutive bunker centers along the row. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bunkers", meta = (ClampMin = "100"))
	float BunkerSpacing = 900.0f;

	/** @brief Extra clearance (cm) reserved around every bunker before overlap tests. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bunkers", meta = (ClampMin = "0"))
	float BunkerSpacingExtra = 50.0f;

	// --- Defensive works ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Obstacle Belt")
	FEnemyObstacleBeltSettings ObstacleBelt;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Barbed Wire")
	FEnemyBarbedWireSettings BarbedWire;

	/** @brief Optional sandbag parapet wrapping the whole row (use Front & Flanks for a classic parapet). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Sandbags")
	FEnemySandbagSettings Sandbags;

	// --- Decoration ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Decorators")
	TArray<FEnemyDecoratorProfile> DecoratorProfiles;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Foliage")
	FEnemyFoliageSettings Foliage;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Decals")
	FEnemyDecalSettings Decals;
};

/**
 * @brief Execution element for Bunker Line. Resolves assets, drives the shared placement builder
 * once per input point (one bunker row + belt + wire + parapet), spawns managed actors and emits
 * the categorized outputs. Runs on the game thread; never cached.
 */
class FPCGBunkerLineElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
};
