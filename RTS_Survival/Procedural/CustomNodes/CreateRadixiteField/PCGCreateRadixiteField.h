// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "PCGCreateRadixiteField.generated.h"

class AActor;
class UMaterialInterface;

/**
 * @brief Configures an actor that can represent a growth center or a regular radixite crystal.
 * Measured Blueprint bounds keep differently sized entries from overlapping.
 */
USTRUCT(BlueprintType)
struct FRadixiteFieldActorEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset")
	TSoftClassPtr<AActor> ActorClass;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset", meta = (ClampMin = "0.01"))
	float Weight = 1.0f;

	/** @brief Horizontal radius used for placement; 0 measures the spawned Blueprint bounds. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Placement", meta = (ClampMin = "0", Units = "cm"))
	float FootprintRadiusOverride = 0.0f;

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
 * @brief Configures a field decal with independently randomized dimensions.
 * The decal is projected along the local terrain normal so slopes remain covered correctly.
 */
USTRUCT(BlueprintType)
struct FRadixiteFieldDecalEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset")
	TSoftObjectPtr<UMaterialInterface> Material;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset", meta = (ClampMin = "0.01"))
	float Weight = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Size", meta = (ClampMin = "1", Units = "cm"))
	FVector2D MinSize = FVector2D(300.0, 300.0);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Size", meta = (ClampMin = "1", Units = "cm"))
	FVector2D MaxSize = FVector2D(800.0, 800.0);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Size", meta = (ClampMin = "1", Units = "cm"))
	float ProjectionDepth = 500.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Placement", meta = (Units = "cm"))
	float SurfaceOffset = 4.0f;
};

/**
 * @brief Creates separate spline-carved radixite fields from candidate points, rejects fields that
 * overlap exclusions or one another, then places terrain-aligned growth nodes, crystals, and decals.
 * @note Connect candidate point data to In and optional blocking spatial data to Excluded Bounds.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UPCGCreateRadixiteFieldSettings : public UPCGSettings
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
	int32 RandomSeed = 5813;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Fields", meta = (ClampMin = "0"))
	int32 MinFieldsToCreate = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Fields", meta = (ClampMin = "0"))
	int32 MaxFieldsToCreate = 3;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Fields", meta = (ClampMin = "100", Units = "cm"))
	float MinFieldRadius = 3000.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Fields", meta = (ClampMin = "100", Units = "cm"))
	float MaxFieldRadius = 5500.0f;

	/** @brief Random radial variation gives each temporary spline a natural, non-circular edge. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Fields", meta = (ClampMin = "0", ClampMax = "0.8"))
	float FieldBoundaryVariation = 0.2f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Fields", meta = (ClampMin = "6", ClampMax = "64"))
	int32 SplinePointCount = 16;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Fields", meta = (ClampMin = "0", Units = "cm"))
	float FieldClearance = 250.0f;

	/** @brief Spacing used while checking the complete carved area against exclusion data. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Fields", meta = (ClampMin = "25", Units = "cm"))
	float ExclusionSampleSpacing = 250.0f;

	// --- Growth centers ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Growth Centers", meta = (ClampMin = "1"))
	int32 GrowthCentersPerField = 3;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Growth Centers")
	TArray<FRadixiteFieldActorEntry> GrowthNodeActors;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Growth Centers", meta = (ClampMin = "0", Units = "cm"))
	float MinGrowthCenterSpacing = 800.0f;

	// --- Regular crystals ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Regular Crystals")
	TArray<FRadixiteFieldActorEntry> RadixiteCrystalActors;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Regular Crystals", meta = (ClampMin = "0"))
	int32 MinCrystalsPerGrowthCenter = 4;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Regular Crystals", meta = (ClampMin = "0"))
	int32 MaxCrystalsPerGrowthCenter = 9;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Regular Crystals", meta = (ClampMin = "0", Units = "cm"))
	float MinGrowthNodeToCrystalDistance = 500.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Regular Crystals", meta = (ClampMin = "0", Units = "cm"))
	float MaxGrowthNodeToCrystalDistance = 2200.0f;

	/** @brief Hard minimum separation between the measured footprints of regular crystals. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Regular Crystals", meta = (ClampMin = "0", Units = "cm"))
	float MinIndividualCrystalDistance = 250.0f;

	/** @brief After the first crystal at a center, candidates must remain connected within this distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Regular Crystals", meta = (ClampMin = "0", Units = "cm"))
	float MaxIndividualCrystalDistance = 1200.0f;

	// --- Decals ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Decals")
	TArray<FRadixiteFieldDecalEntry> Decals;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Decals", meta = (ClampMin = "0"))
	int32 MinDecalsPerField = 3;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Decals", meta = (ClampMin = "0"))
	int32 MaxDecalsPerField = 8;

	// --- Terrain and attempts ---

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Terrain", meta = (ClampMin = "0", Units = "cm"))
	float GroundTraceUp = 15000.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Terrain", meta = (ClampMin = "0", Units = "cm"))
	float GroundTraceDown = 40000.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Terrain", meta = (ClampMin = "0", ClampMax = "89", Units = "deg"))
	float MaxGroundSlopeDegrees = 55.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Terrain")
	bool bAlignActorsToGround = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Placement", meta = (ClampMin = "1"))
	int32 PlacementAttemptsPerItem = 64;
};

/**
 * @brief Main-thread execution element that measures configured actors and registers all generated
 * splines, actors, and decal anchors as PCG-managed resources.
 */
class FPCGCreateRadixiteFieldElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
};
