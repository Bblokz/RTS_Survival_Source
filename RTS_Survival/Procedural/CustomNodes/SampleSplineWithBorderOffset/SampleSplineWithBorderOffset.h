// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "Elements/PCGSplineSampler.h"
#include "PCGSettings.h"

#include "SampleSplineWithBorderOffset.generated.h"

/**
 * @brief Configures interior spline sampling when generated content must retain a guaranteed
 * clearance from the spline boundary. Use it in place of the regular Spline Sampler's Interior mode.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UPCGSampleSplineWithBorderOffsetSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	// ~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Sampler; }
#endif
	virtual bool UseSeed() const override { return true; }

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	// ~End UPCGSettings interface

public:
	/** Space between candidate points in the spline's local XY plane. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings,
		meta = (ClampMin = "0.1", Units = "cm", PCG_Overridable))
	float InteriorSampleSpacing = 100.0f;

	/**
	 * Space between samples used to approximate curved borders. Smaller values improve accuracy;
	 * curved splines also apply a conservative half-step clearance so the requested minimum is upheld.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings,
		meta = (ClampMin = "0.1", Units = "cm", EditCondition = "!bTreatSplineAsPolyline",
			EditConditionHides, PCG_Overridable))
	float InteriorBorderSampleSpacing = 100.0f;

	/** Minimum planar clearance every output point must retain from the spline border. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings,
		meta = (ClampMin = "0", Units = "cm", PCG_Overridable))
	float MinDistanceToSplineBorder = 0.0f;

	/** Use spline control points as a polygon; intended for splines whose interpolation is linear. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bTreatSplineAsPolyline = false;

	/** Interpolate the output Z from the sampled spline border instead of using its lowest Z. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bProjectOntoSurface = false;

	/** Ignore both the Bounding Shape input and the source actor bounds. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bUnbounded = false;

	/** Controls the softness of the volume represented by each generated point. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Points",
		meta = (ClampMin = "0", ClampMax = "1", PCG_Overridable))
	float PointSteepness = 0.5f;

	/** Controls whether point seeds are derived from positions or stable sample indices. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Seeding")
	EPCGSplineSamplingSeedingMode SeedingMode = EPCGSplineSamplingSeedingMode::SeedFromPosition;

	/** Use the spline-local position when deriving position-based seeds. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Seeding",
		meta = (EditCondition = "SeedingMode==EPCGSplineSamplingSeedingMode::SeedFromPosition",
			EditConditionHides))
	bool bSeedFromLocalPosition = false;

	/** Ignore Z when deriving a seed from a position. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Seeding",
		meta = (EditCondition = "SeedingMode==EPCGSplineSamplingSeedingMode::SeedFromPosition",
			EditConditionHides))
	bool bSeedFrom2DPosition = false;
};

/**
 * @brief Samples the eroded interior of each closed spline and emits points that satisfy the
 * configured border clearance and optional bounding shape.
 */
class FPCGSampleSplineWithBorderOffsetElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool ShouldComputeFullOutputDataCrc(FPCGContext* Context) const override { return true; }
	virtual EPCGElementExecutionLoopMode ExecutionLoopMode(const UPCGSettings* Settings) const override
	{
		return EPCGElementExecutionLoopMode::SinglePrimaryPin;
	}
};
