// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "PCGSettings.h"
#include "PCGData.h"
#include "PCGSampleNeighborhoods.generated.h"

/**
 * Settings for the PCGSampleNeighborhoods node.
 *
 * Parameters:
 * - Radius: The radius (in XY) beyond the input point’s bounds from which to sample new points (default 200, min 1).
 * - MinPoints / MaxPoints: The minimum and maximum number of points to sample around each input point (default 1 and 2, min 1).
 * - XYBounds: The half-extent for the bounds on the generated points (generated points get bounds of [-XYBounds, XYBounds] in X and Y, and ±1 in Z).
 * - Spread: Controls how evenly distributed the sample angles are. A value of 1 produces perfectly evenly spaced samples (i.e. less randomness),
 *   while 0 yields fully random angles. (Default is 0.5)
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UPCGSampleNeighborhoodsSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	// ~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("SampleNeighborhoods")); }
	virtual FText GetDefaultNodeTitle() const override { return FText::FromString("Sample Neighborhoods"); }
	virtual FText GetNodeTooltipText() const override { return FText::FromString("Samples a set of points around each input point. "
		"For each input point, a random number of samples (between MinPoints and MaxPoints) is generated. "
		"Each sample is placed outside the original point's XY bounds (plus its future bounds) but within a virtual annulus defined by Radius. "
		"A Spread parameter controls how evenly distributed the sample angles are."); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Sampler; }
#endif
	virtual bool UseSeed() const override { return true; }
	virtual bool HasDynamicPins() const override { return false; }
	
protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	// ~End UPCGSettings interface

public:
	/** Radius used to define the virtual annulus around the input point’s XY bounds (plus the future bounds) from which to sample new points (min 1). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ClampMin = "1"))
	float Radius = 200.f;

	/** Minimum number of points to sample around each input point (min 1). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ClampMin = "1"))
	int32 MinPoints = 1;

	/** Maximum number of points to sample around each input point (min 1). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ClampMin = "1"))
	int32 MaxPoints = 2;

	/** Half‑extent for the bounds to assign to each generated point.
	    The generated point’s Bounds will be set to [-XYBounds, -XYBounds, -1] and [XYBounds, XYBounds, 1]. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	float XYBounds = 100.f;

	/** Controls the angular spread of the generated sample points.
	    A value of 1 produces perfectly evenly spaced angles (i.e. less randomness),
	    whereas 0 produces fully random angles. (Clamped between 0 and 1) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ClampMin = "0", ClampMax = "1"))
	float Spread = 0.5f;
};

/**
 * Execution element for PCGSampleNeighborhoods.
 */
class FPCGSampleNeighborhoodsElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
