// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#pragma once

#include "PCGSettings.h"
#include "PCGContext.h"
#include "PCGEnemyBaseSetupTypes.h"
#include "PCGEnemyBaseSetup.generated.h"

/**
 * @brief Settings for the Enemy Base Setup macro node. Fortifies a whole base area at once: it
 * wraps the area in bunker lines (full perimeter, a single front line facing the AimTarget, or
 * corner strongpoints), each with an obstacle belt, barbed wire and a sandbag parapet in front,
 * and drops fortified interior buildings (at the optional InteriorPoints, or scattered) that face
 * outward. Ammo / fuel decorators, foliage and decals dress every cluster. Combines the smaller
 * setup nodes into one; all placements share one occupancy grid so nothing overlaps.
 * @note Bunkers, Sandbags, Obstacles, BarbedWire and Decorators output points-with-bounds per
 * category; TotalBounds outputs one enclosing box per line / building cluster.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UPCGEnemyBaseSetupSettings : public UPCGSettings
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

	/** @brief How bunker lines wrap the base area. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Perimeter")
	EEnemyPerimeterLayout PerimeterLayout = EEnemyPerimeterLayout::FullPerimeter;

	/** @brief Distance (cm) the bunker lines sit inside the area edge. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Perimeter", meta = (ClampMin = "0"))
	float PerimeterInset = 300.0f;

	/** @brief Amount (cm) each full-side line is shortened from the side length (leaves corner gaps). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Perimeter", meta = (ClampMin = "0",
		EditCondition = "PerimeterLayout != EEnemyPerimeterLayout::Corners"))
	float LineMargin = 600.0f;

	/** @brief Corners only: length (cm) of each corner strongpoint line. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Perimeter", meta = (ClampMin = "0",
		EditCondition = "PerimeterLayout == EEnemyPerimeterLayout::Corners"))
	float CornerLineLength = 2000.0f;

	// --- Perimeter bunkers & works ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Perimeter Bunkers")
	TArray<FEnemyBlueprintEntry> Bunkers;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Perimeter Bunkers", meta = (ClampMin = "100"))
	float BunkerSpacing = 900.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Perimeter Bunkers", meta = (ClampMin = "0"))
	float BunkerSpacingExtra = 50.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Obstacle Belt")
	FEnemyObstacleBeltSettings ObstacleBelt;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Barbed Wire")
	FEnemyBarbedWireSettings BarbedWire;

	/** @brief Sandbag wall used for both the perimeter parapet and the interior buildings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Sandbags")
	FEnemySandbagSettings Sandbags;

	// --- Interior buildings ---

	/** @brief Interior building Blueprints placed at InteriorPoints, or scattered when none are wired. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Interior")
	TArray<FEnemyBlueprintEntry> InteriorBuildings;

	/** @brief How many interior buildings to scatter when the InteriorPoints pin is empty. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Interior", meta = (ClampMin = "0"))
	int32 InteriorBuildingCount = 4;

	/** @brief Extra clearance (cm) reserved around every interior building before overlap tests. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Interior", meta = (ClampMin = "0"))
	float InteriorSpacingExtra = 150.0f;

	/** @brief How interior buildings face; defaults to facing away from the area center (outward). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Interior")
	EEnemyFacingMode InteriorFacingMode = EEnemyFacingMode::AwayFromTarget;

	// --- Decoration (applied to every cluster) ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Decorators")
	TArray<FEnemyDecoratorProfile> DecoratorProfiles;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Foliage")
	FEnemyFoliageSettings Foliage;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Decals")
	FEnemyDecalSettings Decals;
};

/**
 * @brief Execution element for the Enemy Base Setup macro. Derives the perimeter from the Area
 * bounds, builds the bunker lines and interior buildings through the shared placement builder,
 * spawns managed actors and emits the categorized outputs. Runs on the game thread; never cached.
 */
class FPCGEnemyBaseSetupElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
};
