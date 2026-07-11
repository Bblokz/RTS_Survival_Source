// Copyright 2025 Yazan Hanna. All rights reserved.

#include "PCGTrimPathsByGapNode.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAccessor.h"
#include "PCGPoint.h"
#include "Helpers/PCGHelpers.h"

FPCGElementPtr UPCGTrimPathsByGapSettings::CreateElement() const
{
    return MakeShared<UPCGTrimPathsByGapNode>();
}

UPCGTrimPathsByGapSettings::UPCGTrimPathsByGapSettings()
{
    DistanceAttributeName = FName(TEXT("DistanceToPrevious"));
    EndPercentage = 0.1f;
    Tolerance = 500.0f;
}

bool UPCGTrimPathsByGapNode::ExecuteInternal(FPCGContext* Context) const
{
    if (!Context)
    {
        UE_LOG(LogTemp, Warning, TEXT("PCGTrimPathsByGapNode: Invalid context"));
        return false;
    }

    const UPCGTrimPathsByGapSettings* Settings = Context->GetInputSettings<UPCGTrimPathsByGapSettings>();
    if (!Settings)
    {
        UE_LOG(LogTemp, Warning, TEXT("PCGTrimPathsByGapNode: Missing settings"));
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
        UE_LOG(LogTemp, Warning, TEXT("PCGTrimPathsByGapNode: No UPCGPointData inputs found"));
        return false;
    }

    TArray<FPCGTaggedData> CollectedTaggedData;
    if (!TrimPathsByGap(InputPaths, Settings, CollectedTaggedData, Context) || CollectedTaggedData.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("PCGTrimPathsByGapNode: No trimmed output produced"));
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

bool UPCGTrimPathsByGapNode::ReadDistanceValue(const UPCGPointData* PointData, int32 PointIndex, const FName& AttrName, double& OutValue) const
{
    if (!PointData || !PointData->GetPoints().IsValidIndex(PointIndex))
    {
        return false;
    }

    const FPCGPoint& Pt = PointData->GetPoints()[PointIndex];

    // Try float attribute
    if (const FPCGMetadataAttribute<float>* FloatAttr = PointData->Metadata->GetConstTypedAttribute<float>(AttrName))
    {
        OutValue = FloatAttr->GetValueFromItemKey(Pt.MetadataEntry);
        return true;
    }

    // Try double attribute (if present in your metadata implementation)
    if (const FPCGMetadataAttribute<double>* DoubleAttr = PointData->Metadata->GetConstTypedAttribute<double>(AttrName))
    {
        OutValue = DoubleAttr->GetValueFromItemKey(Pt.MetadataEntry);
        return true;
    }

    // Try int attribute (maybe distances were stored as ints)
    if (const FPCGMetadataAttribute<int32>* IntAttr = PointData->Metadata->GetConstTypedAttribute<int32>(AttrName))
    {
        OutValue = static_cast<double>(IntAttr->GetValueFromItemKey(Pt.MetadataEntry));
        return true;
    }

    return false;
}

bool UPCGTrimPathsByGapNode::TrimPathsByGap(const TArray<UPCGPointData*>& InputPaths, const UPCGTrimPathsByGapSettings* Settings, TArray<FPCGTaggedData>& OutTagged, FPCGContext* Context) const
{
    if (!Settings)
    {
        return false;
    }

    const FName AttrName = Settings->DistanceAttributeName;
    const float Tolerance = Settings->Tolerance;
    const float EndPercent = FMath::Clamp(Settings->EndPercentage, 0.0f, 0.5f);

    OutTagged.Reset();

    for (int32 PathIdx = 0; PathIdx < InputPaths.Num(); ++PathIdx)
    {
        UPCGPointData* Src = InputPaths[PathIdx];
        if (!Src)
        {
            continue;
        }

        const TArray<FPCGPoint>& SrcPts = Src->GetPoints();
        const int32 N = SrcPts.Num();
        if (N == 0)
        {
            continue;
        }

        auto CreateOutputPointDataFromRange = [Context](const UPCGPointData* SourceData, int32 StartIndex, int32 NumOutputPoints) -> UPCGPointData*
        {
            UPCGPointData* OutputPointData = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);
            OutputPointData->InitializeFromData(SourceData);

            const TArray<FPCGPoint>& SourcePoints = SourceData->GetPoints();
            TArray<FPCGPoint>& OutputPoints = OutputPointData->GetMutablePoints();
            OutputPoints.Reserve(NumOutputPoints);

            const int32 EndIndexExclusive = StartIndex + NumOutputPoints;
            for (int32 PointIndex = StartIndex; PointIndex < EndIndexExclusive; ++PointIndex)
            {
                if (SourcePoints.IsValidIndex(PointIndex))
                {
                    OutputPoints.Add(SourcePoints[PointIndex]);
                }
            }

            return OutputPointData;
        };

        if (N == 1)
        {
            UPCGPointData* SingleOut = CreateOutputPointDataFromRange(Src, 0, 1);

            FPCGTaggedData Tagged;
            Tagged.Data = SingleOut;
            Tagged.Pin = TEXT("Points");
            OutTagged.Add(MoveTemp(Tagged));
            continue;
        }

        int32 EndWindow = FMath::FloorToInt(N * EndPercent);
        EndWindow = FMath::Max(EndWindow, 0);

        int32 StartTrimIndex = 0;
        bool bStartTrimmed = false;

        int32 StartScanLimit = FMath::Min(EndWindow, N - 1);
        for (int32 i = 1; i <= StartScanLimit; ++i)
        {
            double Dist = 0.0;
            if (!ReadDistanceValue(Src, i, AttrName, Dist))
            {
                StartScanLimit = -1;
                break;
            }

            if (Dist > Tolerance)
            {
                StartTrimIndex = i;
                bStartTrimmed = true;
                break;
            }
        }

        int32 EndTrimIndex = N - 1;
        bool bEndTrimmed = false;

        int32 EndScanLimit = FMath::Max(N - 1 - EndWindow, 1);
        for (int32 i = N - 1; i >= EndScanLimit; --i)
        {
            double Dist = 0.0;
            if (!ReadDistanceValue(Src, i, AttrName, Dist))
            {
                EndScanLimit = -1;
                break;
            }

            if (Dist > Tolerance)
            {
                EndTrimIndex = i - 1;
                bEndTrimmed = true;
                break;
            }
        }

        // PATH 1: Attribute was missing - emit original unchanged  
        if (StartScanLimit == -1 || EndScanLimit == -1)
        {
            UPCGPointData* OutUnchanged = CreateOutputPointDataFromRange(Src, 0, N);

            FPCGTaggedData Tagged;
            Tagged.Data = OutUnchanged;
            Tagged.Pin = TEXT("Points");
            OutTagged.Add(MoveTemp(Tagged));
            continue;
        }

        int32 FinalStart = StartTrimIndex;
        int32 FinalEnd = EndTrimIndex;
        if (FinalStart > FinalEnd)
        {
            continue;
        }

        // PATH 2: Nothing trimmed - emit original unchanged  
        if (!bStartTrimmed && !bEndTrimmed)
        {
            UPCGPointData* OutUnchanged = CreateOutputPointDataFromRange(Src, 0, N);

            FPCGTaggedData Tagged;
            Tagged.Data = OutUnchanged;
            Tagged.Pin = TEXT("Points");
            OutTagged.Add(MoveTemp(Tagged));
            continue;
        }

        // PATH 3: Create trimmed output  
        const int32 NumOutputPoints = FinalEnd - FinalStart + 1;
        UPCGPointData* Trimmed = CreateOutputPointDataFromRange(Src, FinalStart, NumOutputPoints);

        if (Trimmed->GetNumPoints() == 0)
        {
            continue;
        }

        FPCGTaggedData Tagged;
        Tagged.Data = Trimmed;
        Tagged.Pin = TEXT("Points");
        OutTagged.Add(MoveTemp(Tagged));
    }

    return OutTagged.Num() > 0;
}
