// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "PCGCreateHedgehogCorners.generated.h"

class AActor;

/**
 * @brief One bounds-measured Blueprint the corner pattern can place (a Czech hedgehog / anti-tank
 * obstacle). Weight biases the weighted pick; the footprint override feeds the containment margin
 * without rescaling the actor, and an optional uniform scale range is applied per placement.
 */
USTRUCT(BlueprintType)
struct FHedgehogCornersEntry
{
	GENERATED_BODY()

	/** @brief The hedgehog Blueprint to spawn. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset")
	TSoftClassPtr<AActor> HedgehogClass;

	/** @brief Relative pick chance among all entries in the list. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Asset", meta = (ClampMin = "0.01"))
	float Weight = 1.0f;

	/** @brief Local-X footprint size used for the in-volume margin; 0 measures the spawned Blueprint bounds. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bounds",
		meta = (ClampMin = "0", Units = "cm", DisplayName = "Footprint Size X (0 = Measure)"))
	float FootprintOverrideX = 0.0f;

	/** @brief Local-Y footprint size used for the in-volume margin; 0 measures the spawned Blueprint bounds. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bounds",
		meta = (ClampMin = "0", Units = "cm", DisplayName = "Footprint Size Y (0 = Measure)"))
	float FootprintOverrideY = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Placement", meta = (ClampMin = "0.01"))
	float MinUniformScale = 1.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Placement", meta = (ClampMin = "0.01"))
	float MaxUniformScale = 1.0f;

	/** @brief Raises (or lowers) the placed hedgehog off the volume floor. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Placement", meta = (Units = "cm"))
	float ZOffset = 0.0f;

	/** @brief Added to the random yaw for assets authored with a nonstandard forward axis. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Placement", meta = (Units = "deg"))
	float YawOffsetDegrees = 0.0f;
};

/**
 * @brief Places anti-tank hedgehogs in a matching L-shaped pattern at each of the four corners of an
 * input rectangular volume. From each corner the pattern is inset inward by InwardsOffset, then two
 * arms run inward along the two edges meeting at that corner; each arm is TotalLongitudeSize long with
 * one hedgehog every HedgeOffset. Every hedgehog is weighted-picked from the Blueprint list and given a
 * random yaw. Placements are clamped so each hedgehog's footprint always stays inside the volume.
 * @note Connect the rectangular volume (or any spatial data) to In. Placements outputs one point per
 * spawned hedgehog; CollectiveVolume outputs one point-with-bounds per corner enclosing that corner's
 * hedgehogs. Every hedgehog Blueprint is spawned once up front to measure its bounds. Deterministic
 * from RandomSeed.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UPCGCreateHedgehogCornersSettings : public UPCGSettings
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
	// --- General ---

	/** @brief Seeds the random yaws, scales and weighted picks so a graph regenerates identically. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "General")
	int32 RandomSeed = 1337;

	/** @brief Hedgehog Blueprints; one is weighted-picked for every placed position. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Hedgehogs")
	TArray<FHedgehogCornersEntry> Hedgehogs;

	// --- Corner Pattern ---

	/** @brief How far inward from each corner (along both edges) the pattern starts. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Corner Pattern", meta = (ClampMin = "0", Units = "cm"))
	float InwardsOffset = 300.0f;

	/** @brief Length of each of the two arms of a corner pattern, clamped to stay inside the volume. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Corner Pattern", meta = (ClampMin = "0", Units = "cm"))
	float TotalLongitudeSize = 2000.0f;

	/** @brief Spacing between two sequential hedgehogs along an arm. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Corner Pattern", meta = (ClampMin = "1", Units = "cm"))
	float HedgeOffset = 350.0f;
};

/**
 * @brief Main-thread execution element for Create Hedgehog Corners. Measures the configured Blueprints,
 * builds the four corner patterns, spawns every hedgehog as a PCG-managed actor, and emits the per-actor
 * placement points and the per-corner collective-volume bounds. Never cached (it spawns actors).
 */
class FPCGCreateHedgehogCornersElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
};
