// Copyright 2025 Yazan Hanna. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGNode.h"
#include "PCGSettings.h"
#include "Data/PCGPointData.h"
#include "PCGPoint.h"
#include "PCGContext.h"
#include "PCGElement.h"

#include "PCGStitchPathsNode.generated.h"

UCLASS(BlueprintType, ClassGroup = (Procedural))
class UPCGStitchPathsSettings : public UPCGSettings
{
    GENERATED_BODY()

public:
    UPCGStitchPathsSettings()
    {
        Tolerance = 100.0;
        bUseSegmentIndexFallback = true;
        bPreferStartFirst = true;
    }

    // maximum distance to consider two endpoints connectable (world units)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Stitch", meta = (PCG_Overridable))
    double Tolerance;

    // If true, attempt fallback matching using SegmentIndex metadata
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Stitch", meta = (PCG_Overridable))
    bool bUseSegmentIndexFallback;

    // Deterministic ordering preference for iteration (start endpoints first)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings|Stitch", meta = (PCG_Overridable))
    bool bPreferStartFirst;

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
    virtual FPCGElementPtr CreateElement() const override;

#if WITH_EDITOR
    virtual FName GetDefaultNodeName() const override { return TEXT("Stitch Paths"); }
    virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }


#endif
};


class UPCGStitchPathsNode : public IPCGElement
{
public:
    // IPCGElement entrypoint - use this instead of ExecuteNode
    virtual bool ExecuteInternal(FPCGContext* Context) const override;

protected:
    // Build a temporary point data containing exactly the first and last points of each input path
    // Adds metadata keys: "DataIndex" (int), "IsStart" (int: 1=start, 0=end)
    bool BuildEndpointPointData(const TArray<UPCGPointData*>& InputPaths, UPCGPointData*& OutEndpointData, FPCGContext* Context) const;

    // Primary nearest-neighbor matching using PCG octree GetClosestPointIndex helper
    // Produces Matches array sized N where Matches[i] == matched index or INDEX_NONE
    void PerformClosestPointMatching(UPCGPointData* EndpointData, const UPCGStitchPathsSettings* Settings, TArray<int32>& OutMatches) const;

    // Fallback matching by SegmentIndex metadata for unmatched endpoints
    void FallbackMatchBySegmentIndex(UPCGPointData* EndpointData, const UPCGStitchPathsSettings* Settings, TArray<int32>& Matches) const;

    // Using the Matches array stitch original input path arrays into continuous paths
    // InputPaths index matches DataIndex metadata on EndpointData
    bool StitchPathsFromMatches(const TArray<UPCGPointData*>& InputPaths, UPCGPointData* EndpointData, const TArray<int32>& Matches, TArray<FPCGTaggedData>& OutCollectedTaggedData, FPCGContext* Context) const;

    // Utility: deterministic endpoint order
    void BuildEndpointOrder(const UPCGPointData* EndpointData, bool bPreferStartFirst, TArray<int32>& OutOrder) const;
};
