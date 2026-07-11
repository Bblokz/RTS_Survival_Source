// Copyright 2025 Yazan Hanna. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGNode.h"
#include "Data/PCGPointData.h"
#include "PCGSettings.h"
#include "PCGElement.h"
#include "LandscapeSplineActor.h"
#include "LandscapeSplinesComponent.h"
#include "LandscapeSplineControlPoint.h"
#include "LandscapeSplineSegment.h"
#include "Data/PCGLandscapeSplineData.h"
#include "Delegates/IDelegateInstance.h"
#include "PCGGraph.h"

#include "LandscapeSplineToPointsNode.generated.h"

UCLASS(BlueprintType, ClassGroup = (Procedural), meta = (DisplayName = "Landscape Splines → Points"))
class RTS_SURVIVAL_API ULandscapeSplineToPointsNode : public UPCGSettings
{
    GENERATED_BODY()

public:

    // Distance between sampled interior points along each segment
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta = (ClampMin = "0.01", PCG_Overridable))
    float SampleDistance = 100.0f;

    // Minimum segment length to consider
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta = (ClampMin = "0.0", PCG_Overridable))
    float MinSegmentLength = 1.0f;

    // Use the control point transforms as exact start/end points
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta = (PCG_Overridable))
    bool bIncludeControlPoints = true;

    // Include interior sampled points between start and end
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta = (PCG_Overridable))
    bool bIncludeSampledPoints = true;

    // Force refresh counter for editor-triggered refresh
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Debug", meta = (PCG_Overridable))
    int32 ForceRefreshCounter = 0;


#if WITH_EDITORONLY_DATA
    FDelegateHandle OnObjectModifiedHandle;

    TSharedPtr<FTimerHandle> RefreshDebounceTimer;
    TWeakObjectPtr<UWorld> TimerWorld;  

    UPROPERTY(Transient)
    bool bDelegatesInitialized;


#endif



#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
    virtual FName GetDefaultNodeName() const override { return TEXT("Landscape Splines To Points"); }
    virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Sampler; }


#endif

    // No input pins
    virtual TArray<FPCGPinProperties> InputPinProperties() const override { return {}; }

    // Single point output pin
    virtual TArray<FPCGPinProperties> OutputPinProperties() const override
    {
        return {
            FPCGPinProperties(TEXT("Points"), EPCGDataType::Point)
        };
    }

    virtual FPCGElementPtr CreateElement() const override;




#if WITH_EDITOR
    // Registers the OnObjectModified delegate for this node
    void RegisterSplineDelegate();

    // Unregisters the delegate when the node is destroyed
    void UnregisterSplineDelegate();

    void OnEditorObjectModified(UObject* Obj);


#endif
};


// Execution element — follows the IPCGElement pattern used by RoadPCGNode
class FLandscapeSplineToPointsElement : public IPCGElement
{
public:
    virtual bool ExecuteInternal(FPCGContext* Context) const override;
    virtual bool IsPassthrough(const UPCGSettings* InSettings) const override { return false; }
};
