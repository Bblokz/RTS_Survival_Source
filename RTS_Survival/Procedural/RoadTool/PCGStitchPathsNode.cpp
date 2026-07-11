// Copyright 2025 Yazan Hanna. All rights reserved.

#include "PCGStitchPathsNode.h"
#include "Helpers/PCGHelpers.h"
#include "PCGPoint.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAccessor.h"
#include "SpatialAlgo/PCGOctreeQueries.h"

FPCGElementPtr UPCGStitchPathsSettings::CreateElement() const
{
    return MakeShared<UPCGStitchPathsNode>();
}

bool UPCGStitchPathsNode::ExecuteInternal(FPCGContext* Context) const
{
    if (!Context)
    {
        UE_LOG(LogTemp, Warning, TEXT("PCGStitchPathsNode: Invalid context"));
        return false;
    }

    const UPCGStitchPathsSettings* Settings = Context->GetInputSettings<UPCGStitchPathsSettings>();
    if (!Settings)
    {
        UE_LOG(LogTemp, Warning, TEXT("PCGStitchPathsNode: Missing settings"));
        return false;
    }

    // Collect UPCGPointData inputs from tagged input collection
    TArray<UPCGPointData*> InputPaths;
    InputPaths.Reserve(Context->InputData.TaggedData.Num());

    for (const FPCGTaggedData& Tagged : Context->InputData.TaggedData)
    {
        if (const UPCGPointData* PDConst = Cast<UPCGPointData>(Tagged.Data))
        {
            // Cast away constness since InputPaths expects non-const pointers.
            UPCGPointData* PD = const_cast<UPCGPointData*>(PDConst);
            InputPaths.Add(PD);
        }
    }

    if (InputPaths.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("PCGStitchPathsNode: No UPCGPointData inputs found"));
        return false;
    }

    // 1) Build endpoint UPCGPointData (one start and one end per input path)
    UPCGPointData* EndpointData = nullptr;
    if (!BuildEndpointPointData(InputPaths, EndpointData, Context) || !EndpointData)
    {
        UE_LOG(LogTemp, Warning, TEXT("PCGStitchPathsNode: Failed to build endpoint data"));
        return false;
    }

    // 2) Nearest neighbor matching using octree helper
    TArray<int32> Matches;
    PerformClosestPointMatching(EndpointData, Settings, Matches);

    
    // 3) Fallback by SegmentIndex if requested
    if (Settings->bUseSegmentIndexFallback)
    {
        FallbackMatchBySegmentIndex(EndpointData, Settings, Matches);
    }

    // 4) Stitch original input paths using Matches -> produce a collection of tagged outputs
    TArray<FPCGTaggedData> CollectedTaggedData;
    if (!StitchPathsFromMatches(InputPaths, EndpointData, Matches, CollectedTaggedData, Context) || CollectedTaggedData.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("PCGStitchPathsNode: Stitching produced no output"));
        return false;
    }

    // Move collected results into the PCG context outputs
    for (FPCGTaggedData& Tagged : CollectedTaggedData)
    {
        FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
        Output = MoveTemp(Tagged);
    }


  
    return true;
}

/*
 BuildEndpointPointData

 Creates a temporary UPCGPointData containing exactly start and end points for each input UPCGPointData.
 Adds metadata attributes to the endpoint UPCGPointData:
  - DataIndex (int): index into InputPaths array
  - IsStart (int): 1 for start point, 0 for end point
  - SegmentIndex (int): copied from source if present, otherwise INDEX_NONE
*/
bool UPCGStitchPathsNode::BuildEndpointPointData(const TArray<UPCGPointData*>& InputPaths, UPCGPointData*& OutEndpointData, FPCGContext* Context) const
{
    OutEndpointData = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);
    check(OutEndpointData);

    // Create attributes on endpoint metadata
    FPCGMetadataAttribute<int32>* DataIndexAttr = OutEndpointData->Metadata->CreateAttribute<int32>(TEXT("DataIndex"), -1, false, true);
    FPCGMetadataAttribute<int32>* IsStartAttr = OutEndpointData->Metadata->CreateAttribute<int32>(TEXT("IsStart"), 1, false, true);
    FPCGMetadataAttribute<int32>* SegmentAttr = OutEndpointData->Metadata->CreateAttribute<int32>(TEXT("SegmentIndex"), INDEX_NONE, false, true);

    TArray<FPCGPoint>& OutPoints = OutEndpointData->GetMutablePoints();
    OutPoints.Reset();

    // For each input path, copy first and last point (if distinct), and create metadata entries
    for (int32 PathIdx = 0; PathIdx < InputPaths.Num(); ++PathIdx)
    {
        const UPCGPointData* SrcData = InputPaths[PathIdx];
        if (!SrcData)
        {
            continue;
        }

        const TArray<FPCGPoint>& SrcPoints = SrcData->GetPoints();
        if (SrcPoints.Num() == 0)
        {
            continue;
        }

        auto AddEndpointFromSource = [&](const FPCGPoint& SrcPt, int32 bIsStart)
        {
            FPCGPoint NewPt = SrcPt; // copy transform, density, bounds, etc.
            NewPt.MetadataEntry = OutEndpointData->Metadata->AddEntry();
            DataIndexAttr->SetValue(NewPt.MetadataEntry, PathIdx);
            IsStartAttr->SetValue(NewPt.MetadataEntry, bIsStart);

            //const UPCGMetadata* Metadata = SrcData->Metadata();
            const FPCGMetadataAttribute<int32>* SegmentAttrIn = SrcData->Metadata->GetConstTypedAttribute<int32>(FName("SegmentIndex"));
            int32 SegmentIndex = SegmentAttrIn ? SegmentAttrIn->GetValueFromItemKey(SrcPt.MetadataEntry) : 0;
			SegmentAttr->SetValue(NewPt.MetadataEntry, SegmentIndex);



            OutPoints.Add(MoveTemp(NewPt));
        };

        // start
        AddEndpointFromSource(SrcPoints[0], 1);

        // end if distinct
        if (SrcPoints.Num() > 1)
        {
            AddEndpointFromSource(SrcPoints.Last(), 0);
        }
    }

    if (OutPoints.Num() == 0)
    {
        OutEndpointData = nullptr;
        return false;
    }

    // UPCGPointData builds its octree lazily; nothing else required here
    return true;
}

/*
 BuildEndpointOrder

 Deterministic ordering for iterating endpoints during matching.
 Primary sort by DataIndex ascending, then prefer IsStart if requested, then by point index.
*/
void UPCGStitchPathsNode::BuildEndpointOrder(const UPCGPointData* EndpointData, bool bPreferStartFirst, TArray<int32>& OutOrder) const
{
    OutOrder.Reset();
    const TArray<FPCGPoint>& Points = EndpointData->GetPoints();
    OutOrder.Reserve(Points.Num());

    for (int32 i = 0; i < Points.Num(); ++i)
    {
        OutOrder.Add(i);
    }

    // Get attribute pointers for faster access in comparator
    const FPCGMetadataAttribute<int32>* DataIndexAttr = EndpointData->Metadata->GetConstTypedAttribute<int32>(FName("DataIndex"));
    const FPCGMetadataAttribute<int32>* IsStartAttr = EndpointData->Metadata->GetConstTypedAttribute<int32>(FName("IsStart"));


    OutOrder.Sort([EndpointData, DataIndexAttr, IsStartAttr, bPreferStartFirst](int32 A, int32 B)
    {
        const FPCGPoint& PA = EndpointData->GetPoints()[A];
        const FPCGPoint& PB = EndpointData->GetPoints()[B];

        int32 AData = DataIndexAttr ? DataIndexAttr->GetValueFromItemKey(PA.MetadataEntry) : A;
        int32 BData = DataIndexAttr ? DataIndexAttr->GetValueFromItemKey(PB.MetadataEntry) : B;
        if (AData != BData)
        {
            return AData < BData;
        }

        int32 AIsStart = IsStartAttr ? IsStartAttr->GetValueFromItemKey(PA.MetadataEntry) : 1;
        int32 BIsStart = IsStartAttr ? IsStartAttr->GetValueFromItemKey(PB.MetadataEntry) : 1;
        if (AIsStart != BIsStart)
        {
            // prefer start (1) first if requested
            return bPreferStartFirst ? (AIsStart > BIsStart) : (AIsStart < BIsStart);
        }

        return A < B;
    });
}

/*
 PerformClosestPointMatching

 For each endpoint (deterministic order) query octree for closest point within Settings->Tolerance.
 Writes symmetric matches into OutMatches (size = N endpoints). Uses bInDiscardCenter = true
 so the query won't return the same point.
*/
void UPCGStitchPathsNode::PerformClosestPointMatching(UPCGPointData* EndpointData, const UPCGStitchPathsSettings* Settings, TArray<int32>& OutMatches) const
{
    const TArray<FPCGPoint>& EndPts = EndpointData->GetPoints();
    const int32 N = EndPts.Num();
    OutMatches.Init(INDEX_NONE, N);
    TArray<char> bMatched; bMatched.Init(0, N);

    // Deterministic order
    TArray<int32> Order;
    BuildEndpointOrder(EndpointData, Settings->bPreferStartFirst, Order);

    const double SearchDist = Settings->Tolerance;
    const bool bDiscardCenter = true;

    // Attribute pointers for reading DataIndex and IsStart
    const FPCGMetadataAttribute<int32>* DataIndexAttr = EndpointData->Metadata->GetConstTypedAttribute<int32>(FName("DataIndex"));
    const FPCGMetadataAttribute<int32>* IsStartAttr = EndpointData->Metadata->GetConstTypedAttribute<int32>(FName("IsStart"));
    for (int32 Idx : Order)
    {
        if (!EndPts.IsValidIndex(Idx))
        {
            continue;
        }
        if (bMatched[Idx])
        {
            continue;
        }

        const FVector QueryPos = EndPts[Idx].Transform.GetLocation();

        // Ask octree for closest point.
        const FPCGPoint* ClosestPoint = UPCGOctreeQueries::GetClosestPoint(EndpointData, QueryPos, bDiscardCenter, SearchDist);
        const int32 ClosestIdx = ClosestPoint ? static_cast<int32>(ClosestPoint - EndPts.GetData()) : INDEX_NONE;
        if (ClosestIdx == INDEX_NONE || !EndPts.IsValidIndex(ClosestIdx))
        {
            continue;
        }

        // candidate already matched -> skip (keeps deterministic)
        if (bMatched[ClosestIdx])
        {
            continue;
        }

        int32 QueryData = DataIndexAttr ? DataIndexAttr->GetValueFromItemKey(EndPts[Idx].MetadataEntry) : -1;
        int32 CandData = DataIndexAttr ? DataIndexAttr->GetValueFromItemKey(EndPts[ClosestIdx].MetadataEntry) : -2;
        int32 QueryIsStart = IsStartAttr ? IsStartAttr->GetValueFromItemKey(EndPts[Idx].MetadataEntry) : 1;
        int32 CandIsStart = IsStartAttr ? IsStartAttr->GetValueFromItemKey(EndPts[ClosestIdx].MetadataEntry) : 1;

        // don't match same data same-end type
        if (QueryData == CandData && QueryIsStart == CandIsStart)
        {
            continue;
        }

        // same data opposite ends -> mark loop (matched to itself)
        if (QueryData == CandData && QueryIsStart != CandIsStart)
        {
            OutMatches[Idx] = ClosestIdx;
            OutMatches[ClosestIdx] = Idx;
            bMatched[Idx] = 1;
            bMatched[ClosestIdx] = 1;
            continue;
        }

        // normal cross-path match
        OutMatches[Idx] = ClosestIdx;
        OutMatches[ClosestIdx] = Idx;
        bMatched[Idx] = 1;
        bMatched[ClosestIdx] = 1;
    }
}

/*
 FallbackMatchBySegmentIndex

 For unmatched endpoints, group by SegmentIndex attribute (if available on endpoint data) and pair start->end
 deterministically within each segment group.
*/
void UPCGStitchPathsNode::FallbackMatchBySegmentIndex(UPCGPointData* EndpointData, const UPCGStitchPathsSettings* /*Settings*/, TArray<int32>& Matches) const
{
    const TArray<FPCGPoint>& Points = EndpointData->GetPoints();
    const int32 N = Points.Num();
    if (N == 0)
    {
        return;
    }

    const FPCGMetadataAttribute<int32>* SegAttr = EndpointData->Metadata->GetConstTypedAttribute<int32>(FName("SegmentIndex"));
    const FPCGMetadataAttribute<int32>* IsStartAttr = EndpointData->Metadata->GetConstTypedAttribute<int32>(FName("IsStart"));

    // Map: SegmentIndex -> Map(IsStart -> Array of endpoint indices)
    TMap<int32, TMap<int32, TArray<int32>>> SegBuckets;

    for (int32 i = 0; i < N; ++i)
    {
        if (Matches.IsValidIndex(i) && Matches[i] != INDEX_NONE)
        {
            continue;
        }

        int32 Seg = INDEX_NONE;
        if (SegAttr)
        {
            Seg = SegAttr->GetValue(Points[i].MetadataEntry);
        }
        if (Seg == INDEX_NONE)
        {
            continue;
        }

        int32 IsStart = IsStartAttr ? IsStartAttr->GetValue(Points[i].MetadataEntry) : 1;

        // Add to bucket for this segment and IsStart
        TMap<int32, TArray<int32>>& Inner = SegBuckets.FindOrAdd(Seg);
        Inner.FindOrAdd(IsStart).Add(i);
    }

    // For each segment and each IsStart value, pair elements deterministically by index order
    for (const auto& SegKV : SegBuckets)
    {
        const int32 Seg = SegKV.Key;
        const TMap<int32, TArray<int32>>& InnerMap = SegKV.Value;

        // two possible IsStart keys: 1 (start) and 0 (end). Iterate both deterministically.
        const int32 IsStartKeys[2] = { 1, 0 };
        for (int kKey = 0; kKey < 2; ++kKey)
        {
            int32 Key = IsStartKeys[kKey];
            const TArray<int32>* Bucket = InnerMap.Find(Key);
            if (!Bucket || Bucket->Num() < 2)
            {
                continue;
            }

            // Pair first with second, third with fourth, etc.
            int32 PairCount = (*Bucket).Num() / 2;
            for (int32 p = 0; p < PairCount; ++p)
            {
                int32 A = (*Bucket)[2 * p];
                int32 B = (*Bucket)[2 * p + 1];

                if (Matches.IsValidIndex(A) && Matches[A] == INDEX_NONE && Matches.IsValidIndex(B) && Matches[B] == INDEX_NONE)
                {
                    Matches[A] = B;
                    Matches[B] = A;
                }
            }
        }
    }
}


/*
 StitchPathsFromMatches

 Walks data chains based on Matches array and concatenates full point sequences from InputPaths.
 When entering a path via start -> append forward; when entering via end -> append reversed.
 Writes resulting points into a new UPCGPointData and returns it via OutStitchedData.
*/
bool UPCGStitchPathsNode::StitchPathsFromMatches(const TArray<UPCGPointData*>& InputPaths, UPCGPointData* EndpointData, const TArray<int32>& Matches, TArray<FPCGTaggedData>& OutCollectedTaggedData, FPCGContext* Context) const
{

    const FName OutputPin = PCGPinConstants::DefaultOutputLabel;

    // consumed flags per input path
    TArray<char> bConsumed;
    bConsumed.Init(0, InputPaths.Num());

    const TArray<FPCGPoint>& EPs = EndpointData->GetPoints();
    const int32 EPCount = EPs.Num();

    // Build quick map: DataIndex -> endpoints (startIdx, endIdx)
    struct FDataEndpoints { int32 StartIdx = INDEX_NONE; int32 EndIdx = INDEX_NONE; };
    TArray<FDataEndpoints> DataEndpoints;
    DataEndpoints.SetNumZeroed(InputPaths.Num());

    const FPCGMetadataAttribute<int32>* DataIndexAttr = EndpointData->Metadata->GetConstTypedAttribute<int32>(FName("DataIndex"));
    const FPCGMetadataAttribute<int32>* IsStartAttr = EndpointData->Metadata->GetConstTypedAttribute<int32>(FName("IsStart"));

    for (int32 ep = 0; ep < EPCount; ++ep)
    {
        if (!EPs.IsValidIndex(ep))
        {
            continue;
        }
        int32 DataIndex = DataIndexAttr ? DataIndexAttr->GetValueFromItemKey(EPs[ep].MetadataEntry) : -1;
        int32 IsStart = IsStartAttr ? IsStartAttr->GetValueFromItemKey(EPs[ep].MetadataEntry) : 1;
        if (DataIndex < 0 || DataIndex >= DataEndpoints.Num())
        {
            continue;
        }
        if (IsStart)
        {
            DataEndpoints[DataIndex].StartIdx = ep;
        }
        else
        {
            DataEndpoints[DataIndex].EndIdx = ep;
        }
    }

    // Traverse each input path in deterministic order and build chains
    for (int32 StartData = 0; StartData < InputPaths.Num(); ++StartData)
    {
        if (bConsumed[StartData])
        {
            continue;
        }

        int32 CurrentData = StartData;
        bool EnteredViaStart = true;
        if (DataEndpoints.IsValidIndex(CurrentData) && DataEndpoints[CurrentData].StartIdx == INDEX_NONE)
        {
            EnteredViaStart = false;
        }

        TArray<int32> ChainDataIndices;
        TArray<bool> ChainEnterViaStart;

        // Walk chain
        while (true)
        {
            if (!bConsumed.IsValidIndex(CurrentData))
            {
                break;
            }
            if (bConsumed[CurrentData])
            {
                break;
            }

            ChainDataIndices.Add(CurrentData);
            ChainEnterViaStart.Add(EnteredViaStart);
            bConsumed[CurrentData] = 1;

            int32 EpIdxUsed = INDEX_NONE;
            if (EnteredViaStart)
            {
                EpIdxUsed = DataEndpoints[CurrentData].EndIdx;
            }
            else
            {
                EpIdxUsed = DataEndpoints[CurrentData].StartIdx;
            }

            if (EpIdxUsed == INDEX_NONE)
            {
                break;
            }
            if (!Matches.IsValidIndex(EpIdxUsed))
            {
                break;
            }

            int32 MatchedEp = Matches[EpIdxUsed];
            if (MatchedEp == INDEX_NONE)
            {
                break;
            }

            int32 NextData = DataIndexAttr ? DataIndexAttr->GetValueFromItemKey(EPs[MatchedEp].MetadataEntry) : -1;
            if (NextData < 0 || NextData >= InputPaths.Num())
            {
                break;
            }

            // detect loop within this chain
            bool bAlreadyInChain = false;
            for (int32 Existing : ChainDataIndices)
            {
                if (Existing == NextData)
                {
                    bAlreadyInChain = true;
                    break;
                }
            }
            if (bAlreadyInChain)
            {
                break;
            }

            int32 NextIsStart = IsStartAttr ? IsStartAttr->GetValueFromItemKey(EPs[MatchedEp].MetadataEntry) : 1;
            bool NextEnteredViaStart = (NextIsStart == 1);

            // When we match to NextIsStart, we will enter next data via the opposite end
			EnteredViaStart = NextEnteredViaStart; // changes from !NextEnteredViaStart;
            CurrentData = NextData;
        }
        UPCGPointData* OutPointsData = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);


        // Append chain points to OutPoints
        for (int32 idx = 0; idx < ChainDataIndices.Num(); ++idx)
        {
            int32 DataIdx = ChainDataIndices[idx];
            bool EnterViaStart = ChainEnterViaStart[idx];

            const UPCGPointData* Src = InputPaths[DataIdx];
            if (!Src)
            {
                continue;
            }

            const TArray<FPCGPoint>& SrcPts = Src->GetPoints();
            if (SrcPts.Num() == 0)
            {
                continue;
            }

            if (EnterViaStart)
            {
                for (const FPCGPoint& P : SrcPts)
                {
                    FPCGPoint NewP = P;
                    NewP.MetadataEntry = OutPointsData->Metadata->AddEntry();

                    OutPointsData->GetMutablePoints().Add(MoveTemp(NewP));
                }
            }
            else
            {
                for (int32 r = SrcPts.Num() - 1; r >= 0; --r)
                {
                    FPCGPoint NewP = SrcPts[r];
                    NewP.MetadataEntry = OutPointsData->Metadata->AddEntry();

                    OutPointsData->GetMutablePoints().Add(MoveTemp(NewP));
                }
            }

        }

        FPCGTaggedData Tagged;
        Tagged.Data = OutPointsData;
        Tagged.Pin = OutputPin;
        OutCollectedTaggedData.Add(MoveTemp(Tagged));

    }



    if (OutCollectedTaggedData.Num() == 0)
    {
        return false;
    }

    return true;
}
