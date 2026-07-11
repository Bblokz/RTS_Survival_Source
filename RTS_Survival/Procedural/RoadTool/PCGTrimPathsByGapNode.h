// Copyright 2025 Yazan Hanna. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGNode.h"
#include "PCGSettings.h"
#include "Data/PCGPointData.h"
#include "PCGPoint.h"
#include "PCGContext.h"
#include "PCGElement.h"

#include "PCGTrimPathsByGapNode.generated.h"

UCLASS(BlueprintType, ClassGroup = (Procedural))
class UPCGTrimPathsByGapSettings : public UPCGSettings
{
    GENERATED_BODY()

public:
    UPCGTrimPathsByGapSettings();

    // Name of the per-point attribute that contains the distance to the previous point.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Trim", meta = (PCG_Overridable))
    FName DistanceAttributeName;

    // Fraction of the path length (by point count) from each end to consider for trimming.
    // Range limited to [0.0, 0.5]
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Trim", meta = (ClampMin = "0.0", ClampMax = "0.5", UIMin = "0.0", UIMax = "0.5", PCG_Overridable))
    float EndPercentage;

    // Gap tolerance value: any distance > Tolerance is considered a gap.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Trim", meta = (ClampMin = "0.0", PCG_Overridable))
    float Tolerance;

        virtual TArray<FPCGPinProperties> InputPinProperties() const override
    {
        return {
            FPCGPinProperties(TEXT("In"), EPCGDataType::Point)
        };
    }


    virtual TArray<FPCGPinProperties> OutputPinProperties() const override
    {
        return {
            FPCGPinProperties(TEXT("Out"), EPCGDataType::Point)
        };
    }

    // Create element
    virtual FPCGElementPtr CreateElement() const override;

#if WITH_EDITOR
    virtual FName GetDefaultNodeName() const override { return FName(TEXT("Trim Paths By Gap")); }
    virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Filter; }


#endif
};


class UPCGTrimPathsByGapNode : public IPCGElement
{
public:
    UPCGTrimPathsByGapNode() = default;

    virtual bool ExecuteInternal(FPCGContext* Context) const override;

private:
    // Process the input UPCGPointData paths and produce one tagged UPCGPointData per (possibly trimmed) path.
    // Returns true if at least one output was produced.
    bool TrimPathsByGap(const TArray<UPCGPointData*>& InputPaths, const UPCGTrimPathsByGapSettings* Settings, TArray<FPCGTaggedData>& OutTagged, FPCGContext* Context) const;

    // Helper to read a numeric distance value from metadata; supports float and int typed attributes.
    bool ReadDistanceValue(const UPCGPointData* PointData, int32 PointIndex, const FName& AttrName, double& OutValue) const;
};
