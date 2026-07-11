// Copyright 2025 Yazan Hanna. All rights reserved.

#include "LandscapeSplineToPointsNode.h"
#include "PCGContext.h"
#include "PCGComponent.h"
#include "EngineUtils.h"
#include "metadata/PCGMetadata.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGHelpers.h"
#include "PCGGraph.h"

#include "Landscape.h"
#include "LandscapeProxy.h"
#include "LandscapeSplinesComponent.h"
#include "LandscapeSplineActor.h"
#include "LandscapeSplineControlPoint.h"
#include "LandscapeSplineSegment.h"

#define LOCTEXT_NAMESPACE "LandscapeSplineToPointsNode"

// ----- ULandscapeSplineToPointsNode -----

#if WITH_EDITOR
void ULandscapeSplineToPointsNode::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    const FName ChangedProp = PropertyChangedEvent.GetPropertyName();
    static const FName NAME_SampleDistance = GET_MEMBER_NAME_CHECKED(ULandscapeSplineToPointsNode, SampleDistance);
    static const FName NAME_ForceRefreshCounter = GET_MEMBER_NAME_CHECKED(ULandscapeSplineToPointsNode, ForceRefreshCounter);

    if (ChangedProp == NAME_SampleDistance || ChangedProp == NAME_None)
    {
    }

    if (ChangedProp == NAME_ForceRefreshCounter || ChangedProp == NAME_None)
    {
    }
}
#endif


FPCGElementPtr ULandscapeSplineToPointsNode::CreateElement() const
{
    return MakeShared<FLandscapeSplineToPointsElement>();
}



bool FLandscapeSplineToPointsElement::ExecuteInternal(FPCGContext* Context) const
{
    const ULandscapeSplineToPointsNode* Settings = Cast<ULandscapeSplineToPointsNode>(Context->Node ? Context->Node->GetSettings() : nullptr);
    if (!Settings)
    {
        return true;
    }

    UPCGComponent* SourceComponent = Context->SourceComponent.Get();
    if (not IsValid(SourceComponent))
    {
        return true;
    }

    UWorld* World = SourceComponent->GetWorld();
    if (not IsValid(World))
    {
        return true;
    }

    const FName OutputPin = PCGPinConstants::DefaultOutputLabel;
    TArray<FPCGTaggedData> CollectedTaggedData;

#if WITH_EDITOR
    // Ensure this node has its delegate registered exactly once (per-node)
    ULandscapeSplineToPointsNode* MutableSettings = const_cast<ULandscapeSplineToPointsNode*>(Settings);
    if (!MutableSettings->bDelegatesInitialized)
    {
        MutableSettings->bDelegatesInitialized = true;
        MutableSettings->RegisterSplineDelegate();
    }
#endif

    // Sampling (must run on game thread)
    auto SamplingLambda = [Context, Settings, World, OutputPin, &CollectedTaggedData]()
        {
            for (TActorIterator<ALandscapeSplineActor> ActorIt(World); ActorIt; ++ActorIt)
            {
                ALandscapeSplineActor* SplineActor = *ActorIt;
                if (!SplineActor) continue;

                ULandscapeSplinesComponent* SplineComp = SplineActor->GetSplinesComponent();
                if (!SplineComp) continue;

                const TArray<ULandscapeSplineControlPoint*>& ControlPoints = SplineComp->GetControlPoints();
                const TArray<ULandscapeSplineSegment*>& Segments = SplineComp->GetSegments();

                // Use UPCGLandscapeSplineData (available in your project) to sample analytically
                UPCGLandscapeSplineData* SplineData = NewObject<UPCGLandscapeSplineData>(World);
                SplineData->Initialize(SplineComp);

                // Precompute control point world transforms if requested
                TMap<ULandscapeSplineControlPoint*, FTransform> ControlPointWorldTransform;
                if (Settings->bIncludeControlPoints)
                {
                    for (ULandscapeSplineControlPoint* CP : ControlPoints)
                    {
                        if (!CP) continue;
                        FTransform Local(CP->Rotation, CP->Location);
                        ControlPointWorldTransform.Add(CP, Local * SplineComp->GetComponentTransform());
                    }
                }

                // Quick guard
                if (Segments.Num() == 0)
                {
                    continue;
                }

                // Build adjacency: for every control point, list its connected segment indices.
                TMap<ULandscapeSplineControlPoint*, TArray<int32>> CPToSegments;
                CPToSegments.Reserve(ControlPoints.Num());

                for (int32 SegIdx = 0; SegIdx < Segments.Num(); ++SegIdx)
                {
                    ULandscapeSplineSegment* Segment = Segments[SegIdx];
                    if (!Segment) continue;

                    ULandscapeSplineControlPoint* CP0 = Segment->Connections[0].ControlPoint;
                    ULandscapeSplineControlPoint* CP1 = Segment->Connections[1].ControlPoint;
                    if (CP0) CPToSegments.FindOrAdd(CP0).Add(SegIdx);
                    if (CP1) CPToSegments.FindOrAdd(CP1).Add(SegIdx);
                }

                // Visited segments set to avoid duplication
                TSet<int32> VisitedSegments;
                VisitedSegments.Reserve(Segments.Num());

                // Helper lambda: produce ordered transforms for a single segment.
                auto SampleSegmentTransforms = [&](int32 SegmentIndex, bool bReverse) -> TArray<FTransform>
                    {
                        TArray<FTransform> Out;
                        ULandscapeSplineSegment* Segment = Segments.IsValidIndex(SegmentIndex) ? Segments[SegmentIndex] : nullptr;
                        if (!Segment) return Out;

                        const float Length = (float)SplineData->GetSegmentLength(SegmentIndex);
                        if (Length < Settings->MinSegmentLength) return Out;

                        const int32 EstimatedCount = FMath::Max(2, FMath::CeilToInt(Length / FMath::Max(0.0001f, Settings->SampleDistance))) + 2;
                        Out.Reserve(EstimatedCount);

                        // Determine raw transforms from start (distance 0) to end (distance Length)
                        FTransform StartTransform = SplineData->GetTransformAtDistance(SegmentIndex, 0.0f, true);
                        FTransform EndTransform = SplineData->GetTransformAtDistance(SegmentIndex, Length, true);

                        if (Settings->bIncludeControlPoints)
                        {
                            ULandscapeSplineControlPoint* StartCP = Segment->Connections[0].ControlPoint;
                            ULandscapeSplineControlPoint* EndCP = Segment->Connections[1].ControlPoint;
                            if (StartCP && ControlPointWorldTransform.Contains(StartCP))
                            {
                                StartTransform = ControlPointWorldTransform[StartCP];
                            }
                            if (EndCP && ControlPointWorldTransform.Contains(EndCP))
                            {
                                EndTransform = ControlPointWorldTransform[EndCP];
                            }
                        }

                        // Ascending order: start -> interior -> end
                        Out.Add(StartTransform);

                        if (Settings->bIncludeSampledPoints)
                        {
                            const float D = FMath::Max(0.0001f, Settings->SampleDistance);
                            for (float Dist = D; Dist < Length; Dist += D)
                            {
                                if (Dist >= Length) break;
                                Out.Add(SplineData->GetTransformAtDistance(SegmentIndex, Dist, true));
                            }
                        }

                        if (Out.Num() == 0 || !Out.Last().Equals(EndTransform))
                        {
                            Out.Add(EndTransform);
                        }
                        else
                        {
                            Out.Last() = EndTransform;
                        }

                        if (bReverse)
                        {
                            Algo::Reverse(Out);
                        }

                        return Out;
                    };

                // Helper to determine connection index for a given segment and incoming control point:
                // returns 0 if incoming CP is Connections[0], 1 if Connections[1], -1 if not found.
                auto FindConnectionIndex = [&](int32 SegmentIndex, ULandscapeSplineControlPoint* CP) -> int32
                    {
                        if (!Segments.IsValidIndex(SegmentIndex) || !CP) return -1;
                        ULandscapeSplineSegment* Segment = Segments[SegmentIndex];
                        if (Segment->Connections[0].ControlPoint == CP) return 0;
                        if (Segment->Connections[1].ControlPoint == CP) return 1;
                        return -1;
                    };

                // Start walking paths from control points that are endpoints (degree == 1) or branch points (degree >= 3)
                for (ULandscapeSplineControlPoint* CPStart : ControlPoints)
                {
                    if (!CPStart) continue;

                    const TArray<int32>* ConnectedSegsPtr = CPToSegments.Find(CPStart);
                    int32 Degree = ConnectedSegsPtr ? ConnectedSegsPtr->Num() : 0;
                    if (Degree == 0) continue;

                    // Only start from degree == 1 (ends) or degree >= 3 (branches)
                    if (!(Degree == 1 || Degree >= 3))
                    {
                        continue;
                    }

                    // For each connected segment, if not visited, start a path
                    for (int32 OutSegIdx : *ConnectedSegsPtr)
                    {
                        if (VisitedSegments.Contains(OutSegIdx))
                        {
                            continue;
                        }

                        // Build path transforms
                        TArray<FTransform> PathTransforms;
                        PathTransforms.Reserve(128);

                        // We'll perform: enter segment from CPStart. If CPStart is Connections[1] then we must sample reversed.
                        int32 CurrSegIdx = OutSegIdx;
                        ULandscapeSplineControlPoint* FromCP = CPStart;

                        while (true)
                        {
                            if (VisitedSegments.Contains(CurrSegIdx))
                            {
                                break;
                            }

                            ULandscapeSplineSegment* CurrSeg = Segments.IsValidIndex(CurrSegIdx) ? Segments[CurrSegIdx] : nullptr;
                            if (!CurrSeg) break;

                            // Find which side we're entering from
                            int32 ConnIndex = FindConnectionIndex(CurrSegIdx, FromCP);
                            // If we enter from connection index 0, we traverse from start->end (no reverse)
                            // If we enter from connection index 1, we traverse from end->start (reverse)
                            bool bReverseSample = (ConnIndex == 1);

                            // Sample this segment with correct orientation
                            TArray<FTransform> SegmentSamples = SampleSegmentTransforms(CurrSegIdx, bReverseSample);
                            if (SegmentSamples.Num() == 0)
                            {
                                VisitedSegments.Add(CurrSegIdx);
                                break;
                            }

                            // Append segment samples to PathTransforms avoiding duplicate of connecting point
                            if (PathTransforms.Num() == 0)
                            {
                                // first segment: take all
                                PathTransforms.Append(SegmentSamples);
                            }
                            else
                            {
                                // subsequent segments: avoid repeating first sample if equal to last of path
                                const FTransform& FirstOfSeg = SegmentSamples[0];
                                const FTransform& LastOfPath = PathTransforms.Last();
                                if (FirstOfSeg.Equals(LastOfPath))
                                {
                                    // append starting from index 1
                                    for (int32 i = 1; i < SegmentSamples.Num(); ++i)
                                    {
                                        PathTransforms.Add(SegmentSamples[i]);
                                    }
                                }
                                else
                                {
                                    // no duplicate, append all
                                    PathTransforms.Append(SegmentSamples);
                                }
                            }

                            // Mark visited
                            VisitedSegments.Add(CurrSegIdx);

                            // Determine next control point at the far end of this segment (opposite of FromCP)
                            ULandscapeSplineControlPoint* CP0 = CurrSeg->Connections[0].ControlPoint;
                            ULandscapeSplineControlPoint* CP1 = CurrSeg->Connections[1].ControlPoint;
                            ULandscapeSplineControlPoint* NextCP = (FromCP == CP0) ? CP1 : CP0;
                            if (!NextCP)
                            {
                                break;
                            }

                            // Determine degree of NextCP
                            const TArray<int32>* NextCPConnectedSegs = CPToSegments.Find(NextCP);
                            int32 NextDegree = NextCPConnectedSegs ? NextCPConnectedSegs->Num() : 0;

                            // If NextCP is an end or branch (degree == 1 or >= 3), we stop after adding this segment
                            if (NextDegree == 1 || NextDegree >= 3)
                            {
                                break;
                            }

                            // Otherwise NextCP degree == 2: find the other segment to continue to
                            check(NextDegree == 2);
                            int32 NextSegCandidate = INDEX_NONE;
                            for (int32 CandidateSegIdx : *NextCPConnectedSegs)
                            {
                                if (CandidateSegIdx != CurrSegIdx)
                                {
                                    NextSegCandidate = CandidateSegIdx;
                                    break;
                                }
                            }

                            if (NextSegCandidate == INDEX_NONE)
                            {
                                break;
                            }

                            // Advance: enter next segment from NextCP
                            FromCP = NextCP;
                            CurrSegIdx = NextSegCandidate;

                            // loop continues
                        } // while walking along degree-2 chain

                        // Emit point data for this path if it has samples
                        if (PathTransforms.Num() > 0)
                        {
                            UPCGPointData* PathPointData = NewObject<UPCGPointData>(World);

                            FPCGMetadataAttribute<int32>* SegmentIndexAttr = PathPointData->Metadata->CreateAttribute<int32>(TEXT("SegmentIndex"), -1, false, true);

                            for (const FTransform& T : PathTransforms)
                            {
                                FPCGPoint Point;
                                Point.Transform = T;
                                Point.Density = 1.0f;
                                Point.SetLocalBounds(FBox(FVector(-1.0f), FVector(1.0f)));
                                Point.MetadataEntry = PathPointData->Metadata->AddEntry();
                                // store -1 to SegmentIndex to indicate it's a multi-segment path; single-segment paths still get -1
                                SegmentIndexAttr->SetValue(Point.MetadataEntry, OutSegIdx);
                                PathPointData->GetMutablePoints().Add(Point);
                            }

                            FPCGTaggedData Tagged;
                            Tagged.Data = PathPointData;
                            Tagged.Pin = OutputPin;
                            CollectedTaggedData.Add(MoveTemp(Tagged));
                        }
                    } // for each connected segment of starting CP
                } // for each CPStart

                // Finally, there might remain cycles consisting entirely of degree-2 control points (closed loops).
                // Find unvisited segments and traverse them as closed paths.
                for (int32 SegIdx = 0; SegIdx < Segments.Num(); ++SegIdx)
                {
                    if (VisitedSegments.Contains(SegIdx)) continue;

                    // Start a loop path from this segment
                    TArray<FTransform> LoopTransforms;
                    LoopTransforms.Reserve(128);

                    int32 CurrSegIdx = SegIdx;
                    ULandscapeSplineControlPoint* EnterFromCP = nullptr;

                    // Decide arbitrary entering CP: use Connections[0] as start, then sample forward (no reverse)
                    {
                        ULandscapeSplineSegment* StartSeg = Segments[CurrSegIdx];
                        if (!StartSeg) continue;
                        EnterFromCP = StartSeg->Connections[0].ControlPoint;
                    }

                    // Walk until we return to visited segments or repeat segment index
                    while (true)
                    {
                        if (VisitedSegments.Contains(CurrSegIdx))
                        {
                            break;
                        }

                        ULandscapeSplineSegment* CurrSeg = Segments.IsValidIndex(CurrSegIdx) ? Segments[CurrSegIdx] : nullptr;
                        if (!CurrSeg) break;

                        int32 ConnIndex = FindConnectionIndex(CurrSegIdx, EnterFromCP);
                        bool bReverseSample = (ConnIndex == 1);

                        TArray<FTransform> SegmentSamples = SampleSegmentTransforms(CurrSegIdx, bReverseSample);
                        if (SegmentSamples.Num() == 0)
                        {
                            VisitedSegments.Add(CurrSegIdx);
                            break;
                        }

                        if (LoopTransforms.Num() == 0)
                        {
                            LoopTransforms.Append(SegmentSamples);
                        }
                        else
                        {
                            const FTransform& FirstOfSeg = SegmentSamples[0];
                            const FTransform& LastOfPath = LoopTransforms.Last();
                            if (FirstOfSeg.Equals(LastOfPath))
                            {
                                for (int32 i = 1; i < SegmentSamples.Num(); ++i)
                                {
                                    LoopTransforms.Add(SegmentSamples[i]);
                                }
                            }
                            else
                            {
                                LoopTransforms.Append(SegmentSamples);
                            }
                        }

                        VisitedSegments.Add(CurrSegIdx);

                        // Advance
                        ULandscapeSplineControlPoint* CP0 = CurrSeg->Connections[0].ControlPoint;
                        ULandscapeSplineControlPoint* CP1 = CurrSeg->Connections[1].ControlPoint;
                        ULandscapeSplineControlPoint* NextCP = (EnterFromCP == CP0) ? CP1 : CP0;
                        if (!NextCP) break;

                        const TArray<int32>* NextCPConnectedSegs = CPToSegments.Find(NextCP);
                        int32 NextDegree = NextCPConnectedSegs ? NextCPConnectedSegs->Num() : 0;

                        // If degree == 2 continue along the other segment
                        if (NextDegree == 2)
                        {
                            int32 NextSegCandidate = INDEX_NONE;
                            for (int32 CandidateSegIdx : *NextCPConnectedSegs)
                            {
                                if (CandidateSegIdx != CurrSegIdx)
                                {
                                    NextSegCandidate = CandidateSegIdx;
                                    break;
                                }
                            }

                            if (NextSegCandidate == INDEX_NONE)
                            {
                                break;
                            }

                            EnterFromCP = NextCP;
                            CurrSegIdx = NextSegCandidate;
                            continue;
                        }
                        else
                        {
                            // We've hit a non-degree-2 control point (shouldn't normally happen in pure loop), stop
                            break;
                        }
                    } // loop for building closed path

                    if (LoopTransforms.Num() > 0)
                    {
                        UPCGPointData* PathPointData = NewObject<UPCGPointData>(World);

                        FPCGMetadataAttribute<int32>* SegmentIndexAttr = PathPointData->Metadata->CreateAttribute<int32>(TEXT("SegmentIndex"), -1, false, true);

                        for (const FTransform& T : LoopTransforms)
                        {
                            FPCGPoint Point;
                            Point.Transform = T;
                            Point.Density = 1.0f;
                            Point.SetLocalBounds(FBox(FVector(-1.0f), FVector(1.0f)));
                            Point.MetadataEntry = PathPointData->Metadata->AddEntry();
                            SegmentIndexAttr->SetValue(Point.MetadataEntry, -1);
                            PathPointData->GetMutablePoints().Add(Point);
                        }

                        FPCGTaggedData Tagged;
                        Tagged.Data = PathPointData;
                        Tagged.Pin = OutputPin;
                        CollectedTaggedData.Add(MoveTemp(Tagged));
                    }
                } // end loops for leftover segments
            }
        };

    // Execute on game thread
    if (IsInGameThread())
    {
        SamplingLambda();
    }
    else
    {
        FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool(true);
        check(DoneEvent);

        AsyncTask(ENamedThreads::GameThread, [SamplingLambda, DoneEvent]()
            {
                SamplingLambda();
                DoneEvent->Trigger();
            });

        DoneEvent->Wait();
        FPlatformProcess::ReturnSynchEventToPool(DoneEvent);
    }

    // Move collected results into the PCG context outputs
    for (FPCGTaggedData& Tagged : CollectedTaggedData)
    {
        FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
        Output = MoveTemp(Tagged);
    }

    return true;
}


#if WITH_EDITOR

void ULandscapeSplineToPointsNode::RegisterSplineDelegate()
{
    if (OnObjectModifiedHandle.IsValid())
    {
        return;
    }

    TWeakObjectPtr<ULandscapeSplineToPointsNode> WeakThis(this);

    OnObjectModifiedHandle = FCoreUObjectDelegates::OnObjectModified.AddLambda(
        [WeakThis, this](UObject* Obj)
        {
            ULandscapeSplineToPointsNode* Node = WeakThis.Get();
            if (!Node || !IsValid(Node))
            {
                return;
            }

            // Node is still alive - proceed with your logic  
            Node->OnEditorObjectModified(Obj);
        }
    );
}

void ULandscapeSplineToPointsNode::UnregisterSplineDelegate()
{
    if (OnObjectModifiedHandle.IsValid())
    {
        FCoreUObjectDelegates::OnObjectModified.Remove(OnObjectModifiedHandle);
        OnObjectModifiedHandle.Reset();
    }

    if (RefreshDebounceTimer.IsValid() && TimerWorld.IsValid())
    {
        if (UWorld* World = TimerWorld.Get())
        {
            World->GetTimerManager().ClearTimer(*RefreshDebounceTimer);
        }
        RefreshDebounceTimer.Reset();
        TimerWorld.Reset();
    }

    bDelegatesInitialized = false;
}

void ULandscapeSplineToPointsNode::OnEditorObjectModified(UObject* Obj)
{
    // Basic guard
    if (!Obj)
    {
        return;
    }


    // NEW: Find the owning graph by traversing outers  
    UPCGGraph* OwningGraph = nullptr;
    UObject* CurrentOuter = GetOuter();
    while (CurrentOuter)
    {
        if (UPCGGraph* Graph = Cast<UPCGGraph>(CurrentOuter))
        {
            OwningGraph = Graph;
            break;
        }
        CurrentOuter = CurrentOuter->GetOuter();
    }

    // Check if we found a graph and if this settings object is still in it  
    if (!OwningGraph || !OwningGraph->FindNodeWithSettings(this))
    {
        UnregisterSplineDelegate();
        return;
    }

    // Quick filter for relevant types
    const bool bIsRelevant =
        Obj->IsA(ALandscapeSplineActor::StaticClass()) ||
        Obj->IsA(ULandscapeSplinesComponent::StaticClass()) ||
        Obj->IsA(ULandscapeSplineControlPoint::StaticClass()) ||
        Obj->IsA(ULandscapeSplineSegment::StaticClass());

    if (!bIsRelevant)
    {
        return;
    }

    // Get world from Obj
    UWorld* ObjWorld = Obj->GetWorld();
    if (!ObjWorld)
    {
        return;
    }

    // Ensure debounce timer exists
    if (!RefreshDebounceTimer.IsValid())
    {
        RefreshDebounceTimer = MakeShared<FTimerHandle>();
    }

    FTimerManager& TM = ObjWorld->GetTimerManager();



    // Clear existing debounce timer
    if (RefreshDebounceTimer->IsValid())
    {
        TM.ClearTimer(*RefreshDebounceTimer);
    }

    // Capture a weak ptr to this node for the callback
    TWeakObjectPtr<ULandscapeSplineToPointsNode> NodeWeak(this);
    FString ObjName = GetNameSafe(Obj);
    const float DebounceDelay = 0.2f;


    TimerWorld = ObjWorld; // Store weak pointer to world  

    TM.SetTimer(*RefreshDebounceTimer, [NodeWeak, ObjName]()
        {
            if (ULandscapeSplineToPointsNode* NodeInner = NodeWeak.Get())
            {
                NodeInner->ForceRefreshCounter++;

                FProperty* Prop = ULandscapeSplineToPointsNode::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(ULandscapeSplineToPointsNode, ForceRefreshCounter));
                FPropertyChangedEvent PropChangedEvent(Prop ? Prop : nullptr);
                NodeInner->PostEditChangeProperty(PropChangedEvent);
                NodeInner->MarkPackageDirty();

                // Refresh PCG components in same world
                int32 RefreshedCount = 0;
                for (TObjectIterator<UPCGComponent> CompIt; CompIt; ++CompIt)
                {
                    UPCGComponent* Comp = *CompIt;
                    if (!Comp) continue;
                    if (Comp->GetWorld() != NodeInner->GetWorld()) continue;
                    Comp->Generate(true);
                    Comp->DirtyGenerated();
                    RefreshedCount++;
                }

            }
            else
            {
            }
        }, DebounceDelay, false);
}

#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
