// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#pragma once

#include "PCGSettings.h"
#include "PCGContext.h"
#include "PCGForestBiomeTypes.h"
#include "PCGCreateForestBiome.generated.h"

/**
 * @brief Settings for the CreateForestBiome node. Scatters a forest inside the input spawn volume:
 * large trees, regular trees, bushes, foliage and auxiliary props (rocks etc.), each with its own
 * areal density. Trees spawn as their own Blueprint actors; bushes and instanced auxiliaries are
 * batched into Hierarchical Instanced Static Mesh components; foliage is emitted on the Foliage
 * output pin for a GPU Static Mesh Spawner. Categories are placed largest-first into a shared
 * occupancy grid so nothing overlaps, and nothing is ever placed inside the Exclusion input.
 * @note Wire the Foliage output pin into a Static Mesh Spawner with mesh selection "By Attribute"
 * (attribute "Mesh").
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UPCGCreateForestBiomeSettings : public UPCGSettings
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

	/** @brief The biome is deterministic from the RandomSeed property, not the PCG seed. */
	virtual bool UseSeed() const override { return false; }

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	// ~End UPCGSettings interface

public:
	// --- Main ---
	// Per-category amounts and global tuning. All densities are counts per 1000 x 1000 unit
	// (10 m x 10 m) tile of the input area.

	/** @brief All placement, scaling and selection derives deterministically from this seed. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Main")
	int32 RandomSeed = 42;

	/** @brief Average large trees per 1000 x 1000 unit tile of the biome. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Main", meta = (ClampMin = "0"))
	float LargeTreesPer1000Units = 0.5f;

	/** @brief Average regular trees per 1000 x 1000 unit tile of the biome. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Main", meta = (ClampMin = "0"))
	float RegularTreesPer1000Units = 2.0f;

	/** @brief Average bushes per 1000 x 1000 unit tile of the biome. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Main", meta = (ClampMin = "0"))
	float BushesPer1000Units = 3.0f;

	/** @brief Average foliage meshes per 1000 x 1000 unit tile of the biome. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Main", meta = (ClampMin = "0"))
	float FoliagePer1000Units = 8.0f;

	/** @brief Average auxiliary props per 1000 x 1000 unit tile of the biome. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Main", meta = (ClampMin = "0"))
	float AuxiliariesPer1000Units = 1.0f;

	/** @brief Scales every category's density at once; 1 = use the per-category values as-is. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Main", meta = (ClampMin = "0"))
	float GlobalDensityMultiplier = 1.0f;

	/** @brief Extra spacing (cm) kept between neighbouring trees and bushes on top of their radii. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Main", meta = (ClampMin = "0"))
	float PropSpacing = 100.0f;

	// --- Large trees ---

	/** @brief Large tree Blueprints; spawned as standalone actors (never instanced). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Large Trees")
	TArray<FForestBlueprintEntry> LargeTrees;

	// --- Regular trees ---

	/** @brief Regular tree Blueprints; spawned as standalone actors (never instanced). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Regular Trees")
	TArray<FForestBlueprintEntry> RegularTrees;

	// --- Bushes ---

	/** @brief Bush Blueprints; their static meshes are extracted and instanced (HISM). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bushes")
	TArray<FForestBlueprintEntry> Bushes;

	// --- Foliage ---

	/** @brief Foliage meshes emitted on the Foliage output pin for a GPU Static Mesh Spawner. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Foliage")
	TArray<FForestFoliageEntry> Foliage;

	// --- Auxiliaries ---

	/** @brief Auxiliary props (rocks, debris...) dispersed into the space trees and bushes leave free. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Auxiliaries")
	TArray<FForestAuxiliaryEntry> Auxiliaries;

	/** @brief Isolation multiplier applied to Small auxiliaries' preferred distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Auxiliaries", meta = (ClampMin = "0"))
	float SmallAuxiliaryIsolationMultiplier = 1.0f;

	/** @brief Isolation multiplier applied to Medium auxiliaries' preferred distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Auxiliaries", meta = (ClampMin = "0"))
	float MediumAuxiliaryIsolationMultiplier = 2.0f;

	/** @brief Isolation multiplier applied to Large auxiliaries' preferred distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Auxiliaries", meta = (ClampMin = "0"))
	float LargeAuxiliaryIsolationMultiplier = 4.0f;
};

/**
 * @brief Execution element for CreateForestBiome. Resolves assets and their measured clearance
 * radii, runs the deterministic scatter generator, spawns trees as actors, bushes and instanced
 * auxiliaries as batched HISM actors, Blueprint auxiliaries as actors, and emits foliage points.
 */
class FPCGCreateForestBiomeElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

	/** @brief Spawns actors and measures Blueprint bounds; must run on the game thread. */
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }

	/** @brief Spawning actors is a side effect; results must never come from the cache. */
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
};
