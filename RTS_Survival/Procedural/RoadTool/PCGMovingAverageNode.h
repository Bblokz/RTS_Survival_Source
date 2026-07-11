// Copyright 2025 Yazan Hanna. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGNode.h"
#include "PCGSettings.h"
#include "Data/PCGPointData.h"
#include "PCGPoint.h"
#include "PCGContext.h"
#include "PCGElement.h"

#include "PCGMovingAverageNode.generated.h"

UCLASS(BlueprintType, ClassGroup = (Procedural))
class UPCGMovingAverageSettings : public UPCGSettings
{
    GENERATED_BODY()

public:
    UPCGMovingAverageSettings();

    // Radius in points for the smoothing kernel. Effective window = 2 * Radius + 1
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Smoothing", meta = (ClampMin = "1", PCG_Overridable))
    int32 Radius = 5;

    // Global influence applied to the weighting kernel (1.0 = neutral)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Smoothing", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0", PCG_Overridable))
    double Influence = 1.0;

    // If true use mirrored edge sampling; otherwise clamp indices at edges
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Smoothing", meta = (PCG_Overridable))
    bool bClosedLoop = false;

    virtual TArray<FPCGPinProperties> InputPinProperties() const override
    {
        return { FPCGPinProperties(TEXT("In"), EPCGDataType::Point) };
    }

    virtual TArray<FPCGPinProperties> OutputPinProperties() const override
    {
        return { FPCGPinProperties(TEXT("Out"), EPCGDataType::Point) };
    }

    // Create element
    virtual FPCGElementPtr CreateElement() const override;

#if WITH_EDITOR
    virtual FName GetDefaultNodeName() const override { return FName(TEXT("Moving Average Smooth")); }
    virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif
};

class UPCGMovingAverageNode : public IPCGElement
{
public:
    UPCGMovingAverageNode() = default;

    // IPCGElement override
    virtual bool ExecuteInternal(FPCGContext* Context) const override;

private:
    // Core smoothing implementation that operates on a list of UPCGPointData inputs.
    bool SmoothPaths(const TArray<UPCGPointData*>& InputPaths, const UPCGMovingAverageSettings* Settings, TArray<FPCGTaggedData>& OutTagged, FPCGContext* Context) const;

    // Helper to reflect index (mirror) around 0..MaxIndex
    static FORCEINLINE int32 MirrorIndex(int32 Index, int32 MaxIndex)
    {
        if (MaxIndex < 0) return 0;
        if (Index < 0)
        {
            const int32 mirrored = -Index - 1;
            return FMath::Clamp(mirrored, 0, MaxIndex);
        }
        if (Index > MaxIndex)
        {
            const int32 mirrored = 2 * MaxIndex - Index + 1;
            return FMath::Clamp(mirrored, 0, MaxIndex);
        }
        return Index;
    }
};
