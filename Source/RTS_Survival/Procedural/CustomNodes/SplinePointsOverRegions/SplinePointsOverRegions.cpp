// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SplinePointsOverRegions.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Algo/Sort.h"
#include "Math/UnrealMathUtility.h"
#include "RTS_Survival/DeveloperSettings.h"

#define LOCTEXT_NAMESPACE "PCGGenerateSplinePointsOverRegions"

// ============================================================================
// UPCGGenerateSplinePointsOverRegionsSettings implementation

#if WITH_EDITOR
FName UPCGGenerateSplinePointsOverRegionsSettings::GetDefaultNodeName() const
{
	return FName(TEXT("GenerateSplinePointsOverRegions"));
}

FText UPCGGenerateSplinePointsOverRegionsSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Generate Spline Points Over Regions");
}

FText UPCGGenerateSplinePointsOverRegionsSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Generates spline points over a set of adjacent spatial regions. "
	               "It first selects the largest connected group of regions, then assigns each region a number of points (proportional to its volume between MinPoints and MaxPoints). "
	               "For each region the first point is chosen so that it is at least the PreferredMinDistance away from the previous region’s last point. "
	               "Intermediate points are generated so that consecutive segments satisfy the angle constraints, and the final point is sampled on the border (within a border region) with a neighboring region (or the farthest border if none).");
}
#endif

TArray<FPCGPinProperties> UPCGGenerateSplinePointsOverRegionsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	// Expect spatial data on the default input.
	Pins.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Spatial).SetRequiredPin();
	return Pins;
}

TArray<FPCGPinProperties> UPCGGenerateSplinePointsOverRegionsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	// Output is point data.
	Pins.Emplace_GetRef(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);
	return Pins;
}

FPCGElementPtr UPCGGenerateSplinePointsOverRegionsSettings::CreateElement() const
{
	return MakeShared<FPCGGenerateSplinePointsOverRegionsElement>();
}

// ============================================================================
// Helper Functions

static bool AreAdjacent(const UPCGSpatialData* A, const UPCGSpatialData* B, const float Tolerance = 1e-3f)
{
    if (!A || !B)
        return false;
    
    // Expand the boxes slightly.
    FBox BoxA = A->GetBounds().ExpandBy(Tolerance);
    FBox BoxB = B->GetBounds().ExpandBy(Tolerance);
    
    // Calculate the gaps along the X and Y axes.
    float gapX = FMath::Max(0.f, FMath::Max(BoxB.Min.X - BoxA.Max.X, BoxA.Min.X - BoxB.Max.X));
    float gapY = FMath::Max(0.f, FMath::Max(BoxB.Min.Y - BoxA.Max.Y, BoxA.Min.Y - BoxB.Max.Y));
    
    // Calculate the overlap lengths along each axis.
    float overlapX = FMath::Max(0.f, FMath::Min(BoxA.Max.X, BoxB.Max.X) - FMath::Max(BoxA.Min.X, BoxB.Min.X));
    float overlapY = FMath::Max(0.f, FMath::Min(BoxA.Max.Y, BoxB.Max.Y) - FMath::Max(BoxA.Min.Y, BoxB.Min.Y));
    
    // For proper edge contact, the overlap along the contacting edge should be significant.
    // For horizontal contact (touching vertical edges), use the Y-axis overlap.
    float requiredOverlapY = FMath::Min(BoxA.GetSize().Y, BoxB.GetSize().Y) * 0.25f; // e.g. 25% of the smaller height.
    // For vertical contact (touching horizontal edges), use the X-axis overlap.
    float requiredOverlapX = FMath::Min(BoxA.GetSize().X, BoxB.GetSize().X) * 0.25f; // e.g. 25% of the smaller width.
    
    // Check for horizontal adjacency (vertical edges touching).
    bool horizontalAdjacent = (gapX < Tolerance) && (overlapY >= requiredOverlapY);
    // Check for vertical adjacency (horizontal edges touching).
    bool verticalAdjacent = (gapY < Tolerance) && (overlapX >= requiredOverlapX);
    
    return horizontalAdjacent || verticalAdjacent;
}

static float ComputeVolume(const FBox& Box)
{
	const FVector Extent = Box.Max - Box.Min;
	return Extent.X * Extent.Y * Extent.Z;
}

static void BuildConnectedComponents(const TArray<const UPCGSpatialData*>& Regions,
                                     TArray<TArray<int32>>& OutComponents)
{
	const int32 Num = Regions.Num();
	TArray<bool> Visited;
	Visited.Init(false, Num);

	for (int32 i = 0; i < Num; ++i)
	{
		if (Visited[i])
			continue;
		TArray<int32> SubsetOfConnectedRegions;
		TArray<int32> Stack;
		Stack.Add(i);
		while (Stack.Num() > 0)
		{
			int32 Cur = Stack.Pop();
			if (Visited[Cur])
				continue;
			Visited[Cur] = true;
			SubsetOfConnectedRegions.Add(Cur);
			for (int32 j = 0; j < Num; ++j)
			{
				if (!Visited[j] && AreAdjacent(Regions[Cur], Regions[j]))
				{
					Stack.Add(j);
				}
			}
		}
		OutComponents.Add(SubsetOfConnectedRegions);
	}
}

static int32 LerpInt(const int32 MinValue, const int32 MaxValue, const float Ratio)
{
	return FMath::RoundToInt(FMath::Lerp(static_cast<float>(MinValue), static_cast<float>(MaxValue), Ratio));
}




static bool GenerateLastRegionPoint(const FBox& RegionBox, const FVector& ReferencePoint, const float BorderRadius,
                                    const FRandomStream& RandStream, FVector& OutPoint, const UPCGSpatialData* NextNeighbor)
{
	constexpr float Tolerance = 1e-3f;

	if (NextNeighbor != nullptr)
	{
		// Retrieve the neighbor's bounds.
		const FBox NeighborBox = NextNeighbor->GetBounds();

		// Check for shared border along the X axis.
		const bool bSharedXMin = FMath::Abs(RegionBox.Min.X - NeighborBox.Max.X) < Tolerance;
		const bool bSharedXMax = FMath::Abs(RegionBox.Max.X - NeighborBox.Min.X) < Tolerance;
		// Check for shared border along the Y axis.
		const bool bSharedYMin = FMath::Abs(RegionBox.Min.Y - NeighborBox.Max.Y) < Tolerance;
		const bool bSharedYMax = FMath::Abs(RegionBox.Max.Y - NeighborBox.Min.Y) < Tolerance;

		if (bSharedXMin || bSharedXMax)
		{
			// Determine overlapping Y-range between the two boxes.
			const float OverlapYMin = FMath::Max(RegionBox.Min.Y, NeighborBox.Min.Y);
			const float OverlapYMax = FMath::Min(RegionBox.Max.Y, NeighborBox.Max.Y);
			// Apply the BorderRadius margin.
			float YLow = OverlapYMin + BorderRadius;
			float YHigh = OverlapYMax - BorderRadius;
			if (YLow > YHigh)
			{
				YLow = YHigh = (OverlapYMin + OverlapYMax) * 0.5f;
			}
			const float YSample = RandStream.FRandRange(YLow, YHigh);
			// Z is sampled uniformly over the region's Z range.
			const float ZSample = RandStream.FRandRange(RegionBox.Min.Z, RegionBox.Max.Z);
			FVector Candidate;
			Candidate.Y = YSample;
			Candidate.Z = ZSample;
			Candidate.X = bSharedXMin ? RegionBox.Min.X : RegionBox.Max.X;
			OutPoint = Candidate;
			return true;
		}
		else if (bSharedYMin || bSharedYMax)
		{
			// Determine overlapping X-range.
			const float OverlapXMin = FMath::Max(RegionBox.Min.X, NeighborBox.Min.X);
			const float OverlapXMax = FMath::Min(RegionBox.Max.X, NeighborBox.Max.X);
			float XLow = OverlapXMin + BorderRadius;
			float XHigh = OverlapXMax - BorderRadius;
			if (XLow > XHigh)
			{
				XLow = XHigh = (OverlapXMin + OverlapXMax) * 0.5f;
			}
			const float XSample = RandStream.FRandRange(XLow, XHigh);
			const float ZSample = RandStream.FRandRange(RegionBox.Min.Z, RegionBox.Max.Z);
			FVector Candidate;
			Candidate.X = XSample;
			Candidate.Z = ZSample;
			Candidate.Y = bSharedYMin ? RegionBox.Min.Y : RegionBox.Max.Y;
			OutPoint = Candidate;
			return true;
		}
		// If no clear shared border is found, fall through to a random face approach.
	}

	// If NextNeighbor is null (or no shared border found), then this is the last region.
	// Choose the candidate on the region's border (one of the corners) that is farthest from the ReferencePoint.
	// For an axis-aligned box, the farthest point is one of its corners.
	FVector Corners[4];
	// We choose the four corners in the X-Y plane, sampling Z randomly.
	Corners[0] = FVector(RegionBox.Min.X, RegionBox.Min.Y, RandStream.FRandRange(RegionBox.Min.Z, RegionBox.Max.Z));
	Corners[1] = FVector(RegionBox.Min.X, RegionBox.Max.Y, RandStream.FRandRange(RegionBox.Min.Z, RegionBox.Max.Z));
	Corners[2] = FVector(RegionBox.Max.X, RegionBox.Min.Y, RandStream.FRandRange(RegionBox.Min.Z, RegionBox.Max.Z));
	Corners[3] = FVector(RegionBox.Max.X, RegionBox.Max.Y, RandStream.FRandRange(RegionBox.Min.Z, RegionBox.Max.Z));
	float MaxDistance = -1.0f;
	FVector BestCorner = Corners[0];
	for (int i = 0; i < 4; ++i)
	{
		const float Dist = FVector::Dist(Corners[i], ReferencePoint);
		if (Dist > MaxDistance)
		{
			MaxDistance = Dist;
			BestCorner = Corners[i];
		}
	}
	// To avoid the candidate being exactly at the corner, add a random offset within ±BorderRadius.
	FVector Offset;
	Offset.X = RandStream.FRandRange(-BorderRadius, BorderRadius);
	Offset.Y = RandStream.FRandRange(-BorderRadius, BorderRadius);
	Offset.Z = RandStream.FRandRange(-BorderRadius, BorderRadius);
	OutPoint = BestCorner + Offset;
	return true;
}


static bool GenerateBezierCurvePoints(const FBox& RegionBox,
                                      int32 NumPoints,
                                      const FVector& FirstPoint,
                                      const FVector& LastPoint,
                                      TArray<FVector>& OutPoints)
{
	OutPoints.Reset();

	// Get the center of the region.
	const FVector BoxCenter = RegionBox.GetCenter();

	// Choose control points by blending with the box center.
	// This guarantees the control points remain inside the RegionBox.
	const FVector ControlPoint1 = FMath::Lerp(FirstPoint, BoxCenter, 0.5f);
	const FVector ControlPoint2 = FMath::Lerp(LastPoint, BoxCenter, 0.5f);

	// Generate points along the cubic Bézier curve, skipping the first point (t=0)
	// so that FirstPoint is not added twice.
	NumPoints++;
	for (int32 i = 1; i < NumPoints; ++i)
	{
		const float t = static_cast<float>(i) / (NumPoints - 1);
		const float OneMinusT = 1.f - t;

		// Cubic Bézier curve formula:
		// B(t) = (1-t)^3 * FirstPoint + 3*(1-t)^2*t * ControlPoint1 + 3*(1-t)*t^2 * ControlPoint2 + t^3 * LastPoint
		const FVector Point =
			OneMinusT * OneMinusT * OneMinusT * FirstPoint +
			3.f * OneMinusT * OneMinusT * t * ControlPoint1 +
			3.f * OneMinusT * t * t * ControlPoint2 +
			t * t * t * LastPoint;

		OutPoints.Add(Point);
	}

	return true;
}


/** Generates all spline points for a region (first, intermediate, and last).
 *  The LasSplinePoint is on the border with this region or the center of this region if this is the first region in sequence.
 *  For the last point, it samples on the border with the neighbor using BorderRadius,
 *  if there is no NExtNeighbor, it samples on the farthest border from the LastSplinePoint.
 *  Itermediary points on the bezier curve between the lastsplinepoint and the lastpoint.
 *  
 */
static bool GenerateRegionSplinePoints(const FBox& RegionBox, const int32 NumPoints, const FVector& LastSplinePoint,
                                       float PreferredMinDistance,
                                       const float BorderRadius, const FRandomStream& RandStream, const
                                       UPCGSpatialData* NextNeighbor, TArray<FVector>& OutPoints)
{
	OutPoints.Reset();
	FVector LastPoint;
	// Generate the last point on the border with the neighbor if no neighbor this is the last region and the last poitn will be sampled around the
	// furthest corner away from the LastSplinePoint.
	if (!GenerateLastRegionPoint(RegionBox, LastSplinePoint, BorderRadius, RandStream, LastPoint, NextNeighbor))
	{
		// set last point to center of box
		LastPoint = RegionBox.GetCenter();
	}
	// If we only have 2 points or the distance between the first and last point is too small, we can't generate intermediate points.
	if (NumPoints == 2 || FVector::Distance(LastSplinePoint, LastPoint) <=
		DeveloperSettings::Procedural::RTSSplineSystems::MinDistanceFirstLastPoint)
	{
		OutPoints.Add(LastPoint);
		return true;
	}
	GenerateBezierCurvePoints(RegionBox, NumPoints, LastSplinePoint, LastPoint, OutPoints);
	return true;
}


bool FPCGGenerateSplinePointsOverRegionsElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);

	// Declare a random stream.
	const FRandomStream RandStream(Context->GetSeed());

	// Retrieve input spatial data.
	const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	if (Inputs.Num() == 0)
	{
		return false;
	}

	// Extract spatial regions and select the largest connected component.
	const TArray<const UPCGSpatialData*> SelectedRegions =
		SplinePointsHelpers::ExtractLargestConnectedComponentOfRegions(Inputs);
	if (SelectedRegions.Num() == 0)
	{
		return false;
	}

	const UPCGGenerateSplinePointsOverRegionsSettings* Settings = Context->GetInputSettings<
		UPCGGenerateSplinePointsOverRegionsSettings>();

	// Uses the user provided min and max points in proportion to the volume of the region to determine the amount of points.
	TArray<int32> RegionPointCounts = SplinePointsHelpers::ComputeRegionPointCounts(SelectedRegions, Settings);

	TArray<const UPCGSpatialData*> OrderedRegions =
		SplinePointsHelpers::FindLongestAdjacentRegionChain(SelectedRegions);

	// Global container for spline point positions.
	TArray<FVector> GlobalSplinePoints;
	// For the first region, use its center as the starting reference.
	FVector LastSplinePoint = OrderedRegions[0]->GetBounds().GetCenter();
	constexpr float BorderRadius = DeveloperSettings::Procedural::RTSSplineSystems::RegionBorderRadius;

	TArray<int32> InbetweenRegionsPointIndices;

	for (int32 Idx = 0; Idx < OrderedRegions.Num(); ++Idx)
	{
		const UPCGSpatialData* CurrentRegion = OrderedRegions[Idx];
		// Determine next neighbor if available.
		const UPCGSpatialData* NextNeighbor = (Idx < OrderedRegions.Num() - 1) ? OrderedRegions[Idx + 1] : nullptr;
		FBox RegionBox = CurrentRegion->GetBounds();
		const int32 RegionIndex = SelectedRegions.IndexOfByKey(CurrentRegion);
		const int32 PointsForRegion = (RegionIndex != INDEX_NONE) ? RegionPointCounts[RegionIndex] : Settings->MaxPoints;

		TArray<FVector> RegionSplinePoints;
		// Generate spline points for this region.
		if (!GenerateRegionSplinePoints(RegionBox, PointsForRegion, LastSplinePoint,
		                                Settings->PreferredMinDistance, BorderRadius, RandStream,
		                                NextNeighbor, RegionSplinePoints))
		{
			continue;
		}
		if (RegionSplinePoints.Num() > 0)
		{
			LastSplinePoint = RegionSplinePoints.Last(); // Update reference.
		}
		GlobalSplinePoints.Append(RegionSplinePoints);
		// If this is not the last region, then the last point added to this region is a point inbetween regions.
		if (Idx < OrderedRegions.Num() - 1 && RegionSplinePoints.Num() > 0)
		{
			InbetweenRegionsPointIndices.Add(GlobalSplinePoints.Num() - 1);
		}
	}


	// Smooth the in-between points to reduce sharp angles.
	SplinePointsHelpers::LaplacianSmoothing(GlobalSplinePoints, InbetweenRegionsPointIndices);

	for(auto& EachPoint : GlobalSplinePoints)
	{
		EachPoint.Z = Settings->ZValue;
	}

	SplinePointsHelpers::EnsurePairsSatisfyDistanceConstraint(Settings, GlobalSplinePoints);
	
	SplinePointsHelpers::CreateOutputData(Context, GlobalSplinePoints);

	return true;
}


TArray<const UPCGSpatialData*> SplinePointsHelpers::ExtractLargestConnectedComponentOfRegions(
	const TArray<FPCGTaggedData>& Inputs)
{
	TArray<const UPCGSpatialData*> SpatialRegions;
	for (const FPCGTaggedData& Tagged : Inputs)
	{
		const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Tagged.Data.Get());
		if (SpatialData)
		{
			SpatialRegions.Add(SpatialData);
		}
	}
	if (SpatialRegions.Num() == 0)
	{
		return SpatialRegions;
	}

	// Group spatial regions into connected components.
	TArray<TArray<int32>> Components;
	BuildConnectedComponents(SpatialRegions, Components);

	// Choose the largest connected component.
	TArray<int32>* LargestComponent = nullptr;
	int32 LargestSize = 0;
	for (TArray<int32>& Comp : Components)
	{
		if (Comp.Num() > LargestSize)
		{
			LargestSize = Comp.Num();
			LargestComponent = &Comp;
		}
	}
	TArray<const UPCGSpatialData*> SelectedRegions;
	if (LargestComponent && LargestComponent->Num() > 0)
	{
		for (const int32 Idx : *LargestComponent)
		{
			SelectedRegions.Add(SpatialRegions[Idx]);
		}
	}
	else
	{
		SelectedRegions.Add(SpatialRegions[0]);
	}
	return SelectedRegions;
}


TArray<int32> SplinePointsHelpers::ComputeRegionPointCounts(const TArray<const UPCGSpatialData*>& SelectedRegions,
                                                            const UPCGGenerateSplinePointsOverRegionsSettings* Settings)
{
	TArray<float> Volumes;
	Volumes.SetNum(SelectedRegions.Num());
	float MinVolume = FLT_MAX;
	float MaxVolume = 0.f;
	for (int32 i = 0; i < SelectedRegions.Num(); ++i)
	{
		FBox Box = SelectedRegions[i]->GetBounds();
		const float Vol = ComputeVolume(Box);
		Volumes[i] = Vol;
		MinVolume = FMath::Min(MinVolume, Vol);
		MaxVolume = FMath::Max(MaxVolume, Vol);
	}

	TArray<int32> RegionPointCounts;
	RegionPointCounts.SetNum(SelectedRegions.Num());
	const int32 GlobalMinPoints = Settings->MinPoints;
	const int32 GlobalMaxPoints = Settings->MaxPoints;
	for (int32 i = 0; i < SelectedRegions.Num(); ++i)
	{
		if (MaxVolume > MinVolume)
		{
			const float Ratio = (Volumes[i] - MinVolume) / (MaxVolume - MinVolume);
			RegionPointCounts[i] = LerpInt(GlobalMinPoints, GlobalMaxPoints, Ratio);
		}
		else
		{
			RegionPointCounts[i] = GlobalMaxPoints;
		}
	}
	return RegionPointCounts;
}


TArray<const UPCGSpatialData*> SplinePointsHelpers::FindLongestAdjacentRegionChain(
	const TArray<const UPCGSpatialData*>& SelectedRegions)
{
	// Build an adjacency list based on the borders.
	TArray<TArray<int32>> Adjacency;
	Adjacency.SetNum(SelectedRegions.Num());
	for (int32 i = 0; i < SelectedRegions.Num(); ++i)
	{
		for (int32 j = 0; j < SelectedRegions.Num(); ++j)
		{
			if (i == j)
				continue;
			if (AreAdjacent(SelectedRegions[i], SelectedRegions[j]))
			{
				Adjacency[i].Add(j);
			}
		}
		// sort the neighbor indices to fix the traversal order as we cannot assume that unreal engine provides the nodes in the same order.
		// This is important for deterministic results.
		Adjacency[i].Sort();
	}

	// Use DFS to find the longest chain.
	TArray<int32> BestPathIndices;
	TFunction<void(int32, TArray<int32>&, TArray<bool>&)> Dfs;
	Dfs = [&Dfs, &Adjacency, &BestPathIndices](const int32 CurrentIndex, TArray<int32>& CurrentPath, TArray<bool>& Visited)
	{
		bool bExtended = false;
		for (int32 Neighbor : Adjacency[CurrentIndex])
		{
			if (!Visited[Neighbor])
			{
				Visited[Neighbor] = true;
				CurrentPath.Add(Neighbor);
				Dfs(Neighbor, CurrentPath, Visited);
				CurrentPath.Pop();
				Visited[Neighbor] = false;
				bExtended = true;
			}
		}
		// Cache the longest path found
		if (!bExtended && CurrentPath.Num() > BestPathIndices.Num())
		{
			BestPathIndices = CurrentPath;
		}
	};

	// Try each region as a starting point.
	for (int32 i = 0; i < SelectedRegions.Num(); ++i)
	{
		TArray<bool> Visited;
		Visited.Init(false, SelectedRegions.Num());
		TArray<int32> CurrentPath;
		Visited[i] = true;
		CurrentPath.Add(i);
		Dfs(i, CurrentPath, Visited);
	}

	// Build the ordered list of regions from the best chain.
	TArray<const UPCGSpatialData*> OrderedRegions;
	for (const int32 Index : BestPathIndices)
	{
		OrderedRegions.Add(SelectedRegions[Index]);
	}
	// Fallback in case the search yields an empty chain.
	if (OrderedRegions.Num() == 0 && SelectedRegions.Num() > 0)
	{
		OrderedRegions.Add(SelectedRegions[0]);
	}
	return OrderedRegions;
}

void SplinePointsHelpers::LaplacianSmoothing(TArray<FVector>& GlobalSplinePoints, const TArray<int32>& InbetweenIndices,
                                             const float ThresholdAngleDegrees)
{
	for (const int32 EachInbetweenIndex : InbetweenIndices)
	{
		// Ensure that both the previous and next points exist.
		if (!GlobalSplinePoints.IsValidIndex(EachInbetweenIndex - 1) ||
			!GlobalSplinePoints.IsValidIndex(EachInbetweenIndex + 1))
		{
			continue;
		}

		const FVector& Prev = GlobalSplinePoints[EachInbetweenIndex - 1];
		const FVector& Curr = GlobalSplinePoints[EachInbetweenIndex];
		const FVector& Next = GlobalSplinePoints[EachInbetweenIndex + 1];

		// Compute the normalized direction vectors from the current point to its neighbors.
		FVector Dir1 = (Curr - Prev).GetSafeNormal();
		FVector Dir2 = (Next - Curr).GetSafeNormal();

		// Compute the angle (in degrees) between Dir1 and Dir2.
		const float Dot = FMath::Clamp(FVector::DotProduct(Dir1, Dir2), -1.f, 1.f);
		float AngleRadians = FMath::Acos(Dot);
		const float AngleDegrees = FMath::RadiansToDegrees(AngleRadians);

		// If the angle is sharper than the threshold, apply smoothing.
		if (AngleDegrees < ThresholdAngleDegrees)
		{
			// The smoothing factor increases as the angle becomes sharper.
			float SmoothingFactor = FMath::Clamp((ThresholdAngleDegrees - AngleDegrees) / ThresholdAngleDegrees, 0.f,
			                                     1.f);

			// Compute the midpoint between the previous and next points.
			FVector MidPoint = (Prev + Next) * 0.5f;

			// Blend the current point toward the midpoint.
			GlobalSplinePoints[EachInbetweenIndex] = FMath::Lerp(Curr, MidPoint, SmoothingFactor);
		}
	}
}


bool SplinePointsHelpers::CreateOutputData(FPCGContext* Context, const TArray<FVector>& GlobalSplinePoints)
{
	// Convert GlobalSplinePoints into FPCGPoints.
	TArray<FPCGPoint> OutPoints;
	for (const FVector& Pos : GlobalSplinePoints)
	{
		FPCGPoint NewPoint;
		NewPoint.Transform.SetLocation(Pos);
		NewPoint.BoundsMin = FVector(-50.f, -50.f, -50.f);
		NewPoint.BoundsMax = FVector(50.f, 50.f, 50.f);
		NewPoint.Density = 1.0f;
		OutPoints.Add(NewPoint);
	}

	// Create output point data.
	UPCGPointData* OutPointData = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);
	OutPointData->InitializeFromData(nullptr);
	OutPointData->SetPoints(OutPoints);

	// Populate output collection.
	Context->OutputData.TaggedData.Reset();
	FPCGTaggedData OutTagged;
	OutTagged.Data = OutPointData;
	OutTagged.Pin = PCGPinConstants::DefaultOutputLabel;
	Context->OutputData.TaggedData.Add(OutTagged);

	Context->OutputData.DataCrcs.Reset();
	Context->OutputData.DataCrcs.Add(OutPointData->GetOrComputeCrc(true));

	return true;
}

void SplinePointsHelpers::EnsurePairsSatisfyDistanceConstraint(
	const UPCGGenerateSplinePointsOverRegionsSettings* Settings,
	TArray<FVector>& GlobalSplinePoints)
{
    const float DesiredMinimalDistance = Settings->PreferredMinDistance;

    // Iterate while there is a valid pair to check.
    int32 i = 0;
    while (i < GlobalSplinePoints.Num() - 1)
    {
        // Calculate the distance between the current point and the next point.
        float CurrentDistance = FVector::Dist(GlobalSplinePoints[i], GlobalSplinePoints[i + 1]);

        if (CurrentDistance < DesiredMinimalDistance)
        {
            // If there is a point after the next point, remove the next point.
            if (i + 2 < GlobalSplinePoints.Num())
            {
                GlobalSplinePoints.RemoveAt(i + 1);
                // Do not increment i so that the new neighbor of GlobalSplinePoints[i] is checked.
            }
            else
            {
                // If no point exists after the next point, remove the current point.
                GlobalSplinePoints.RemoveAt(i);
                // Stay at the same index; note that the previous point (if any) now has a new neighbor.
                // Also, if i was 0, we continue checking from the beginning.
                if (i > 0)
                {
                    --i; // Optionally, step back to re-check the pair with the previous point.
                }
            }
        }
        else
        {
            // The pair satisfies the distance constraint; move to the next pair.
            ++i;
        }
    }
}



#undef LOCTEXT_NAMESPACE
