// Copyright 2025 Yazan Hanna. All rights reserved.

#include "PCGMovingAverageNode.h"
#include "PCGGraph.h"
#include "PCGComponent.h"
#include "PCGPoint.h"
#include "PCGContext.h"
#include "Helpers/PCGHelpers.h"

FPCGElementPtr UPCGMovingAverageSettings::CreateElement() const
{
    return MakeShared<UPCGMovingAverageNode>();
}

UPCGMovingAverageSettings::UPCGMovingAverageSettings()
{
    Radius = 1;
    Influence = 1.0;
    bClosedLoop = true;
}

bool UPCGMovingAverageNode::ExecuteInternal(FPCGContext* Context) const
{
    if (!Context)
    {
        UE_LOG(LogTemp, Warning, TEXT("PCGMovingAverageNode: Invalid context"));
        return false;
    }

    const UPCGMovingAverageSettings* Settings = Context->GetInputSettings<UPCGMovingAverageSettings>();
    if (!Settings)
    {
        UE_LOG(LogTemp, Warning, TEXT("PCGMovingAverageNode: Missing settings"));
        return false;
    }

    // Collect UPCGPointData inputs from tagged input collection
    TArray<UPCGPointData*> InputPaths;
    InputPaths.Reserve(Context->InputData.TaggedData.Num());

    for (const FPCGTaggedData& Tagged : Context->InputData.TaggedData)
    {
        if (const UPCGPointData* PDConst = Cast<UPCGPointData>(Tagged.Data))
        {
            UPCGPointData* PD = const_cast<UPCGPointData*>(PDConst);
            InputPaths.Add(PD);
        }
    }

    if (InputPaths.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("PCGMovingAverageNode: No UPCGPointData inputs found"));
        return false;
    }

    TArray<FPCGTaggedData> CollectedTaggedData;
    if (!SmoothPaths(InputPaths, Settings, CollectedTaggedData, Context) || CollectedTaggedData.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("PCGMovingAverageNode: No smoothed output produced"));
        return false;
    }

    // Move results into the context outputs
    for (FPCGTaggedData& Tagged : CollectedTaggedData)
    {
        FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
        Output = MoveTemp(Tagged);
    }

    return true;
}
bool UPCGMovingAverageNode::SmoothPaths(const TArray<UPCGPointData*>& InputPaths, const UPCGMovingAverageSettings* Settings, TArray<FPCGTaggedData>& OutTagged, FPCGContext* Context) const
{
    if (!Settings)
    {
        return false;
    }

    const int32 Radius = FMath::Max(1, Settings->Radius);
    const int32 Window = 2 * Radius + 1;
    const double Influence = FMath::Max(0.0, Settings->Influence);
    const bool bClosedLoop = Settings->bClosedLoop;

    OutTagged.Reset();

    for (UPCGPointData* Src : InputPaths)
    {
        if (!Src)
        {
            continue;
        }

        const TArray<FPCGPoint>& SourcePoints = Src->GetPoints();
        const int32 NumPoints = SourcePoints.Num();
        if (NumPoints == 0)
        {
            continue;
        }

        // Create output container and initialize from source  
        UPCGPointData* Smoothed = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);

        Smoothed->InitializeFromData(Src);
        TArray<FPCGPoint>& SmoothedPoints = Smoothed->GetMutablePoints();
        SmoothedPoints = SourcePoints;

        // Prepare precomputed linear falloff weights  
        TArray<double> Weights;
        Weights.SetNum(Window);
        for (int32 offset = -Radius; offset <= Radius; ++offset)
        {
            const double falloff = 1.0 - (double)FMath::Abs(offset) / (double)Radius;
            Weights[offset + Radius] = falloff;  
        }

        // Compute weighted average positions  
        for (int32 PointIndex = 0; PointIndex < NumPoints; ++PointIndex)
        {
            FVector AccumPos = FVector::ZeroVector;
            double TotalWeight = 0.0;

            for (int32 offset = -Radius; offset <= Radius; ++offset)
            {
                int32 SampleIndex = PointIndex + offset;

                if (bClosedLoop)
                {
                    // Wrap indices around for closed loop  
                    SampleIndex = SampleIndex % NumPoints;
                    if (SampleIndex < 0) { SampleIndex += NumPoints; }
                }
                else
                {
                    // Clamp to valid range for open path  
                    SampleIndex = FMath::Clamp(SampleIndex, 0, NumPoints - 1);
                }

                const double w = Weights[offset + Radius];
                AccumPos += SourcePoints[SampleIndex].Transform.GetLocation() * (float)w;
                TotalWeight += w;
            }

            // Apply Influence as a lerp between original and smoothed  
            FVector SmoothedPos = AccumPos / (float)TotalWeight;
            FVector OriginalPos = SourcePoints[PointIndex].Transform.GetLocation();
            FVector FinalPos = FMath::Lerp(OriginalPos, SmoothedPos, Influence);

            SmoothedPoints[PointIndex].Transform.SetLocation(FinalPos);
        }

        // Add tagged result  
        if (Smoothed->GetNumPoints() > 0)
        {
            FPCGTaggedData Tagged;
            Tagged.Data = Smoothed;
            Tagged.Pin = TEXT("Points");
            OutTagged.Add(MoveTemp(Tagged));
        }
    }

    return OutTagged.Num() > 0;
}
