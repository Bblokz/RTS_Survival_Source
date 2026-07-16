// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SampleSplineWithBorderOffset.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"
#include "Data/PCGPolyLineData.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGHelpers.h"
#include "Utils/PCGLogErrors.h"

#define LOCTEXT_NAMESPACE "PCGSampleSplineWithBorderOffset"

namespace SampleSplineWithBorderOffset
{
	const FName SplinePinLabel = TEXT("Spline");
	const FName BoundingShapePinLabel = TEXT("Bounding Shape");

	struct FBorderData
	{
		TArray<FVector> Points;
		TArray<FVector2D> Points2D;
		FBox2D Bounds = FBox2D(EForceInit::ForceInit);
		double MinZ = 0.0;
		double MaximumSampleGap = 0.0;
	};

	struct FSamplingGrid
	{
		double MinX = 0.0;
		double MaxX = 0.0;
		double MinY = 0.0;
		double MaxY = 0.0;
	};

	struct FCandidateSamplingContext
	{
		const FBorderData& BorderData;
		const UPCGSpatialData* BoundingShape = nullptr;
		const UPCGSampleSplineWithBorderOffsetSettings& Settings;
		FTransform SplineTransform;
		FBox PointBounds;
		double RequiredBorderDistanceSquared = 0.0;
		int32 ContextSeed = 0;
	};

	void AddBorderPoint(FBorderData& BorderData, const FVector& Point)
	{
		BorderData.Points.Add(Point);
		BorderData.Points2D.Emplace(Point.X, Point.Y);
		BorderData.Bounds += FVector2D(Point.X, Point.Y);
		BorderData.MinZ = BorderData.Points.Num() == 1 ? Point.Z : FMath::Min(BorderData.MinZ, Point.Z);
	}

	FBorderData BuildBorderData(
		const UPCGPolyLineData& Spline,
		const UPCGSampleSplineWithBorderOffsetSettings& Settings)
	{
		FBorderData BorderData;
		const int32 NumSegments = Spline.GetNumSegments();

		for (int32 SegmentIndex = 0; SegmentIndex < NumSegments; ++SegmentIndex)
		{
			const double SegmentLength = Spline.GetSegmentLength(SegmentIndex);
			const int32 NumIntervals = Settings.bTreatSplineAsPolyline
				? 1
				: FMath::Max(1, FMath::CeilToInt32(SegmentLength / Settings.InteriorBorderSampleSpacing));
			const double SampleGap = NumIntervals > 0 ? SegmentLength / NumIntervals : 0.0;
			BorderData.MaximumSampleGap = FMath::Max(BorderData.MaximumSampleGap, SampleGap);

			for (int32 IntervalIndex = 0; IntervalIndex < NumIntervals; ++IntervalIndex)
			{
				AddBorderPoint(
					BorderData,
					Spline.GetLocationAtDistance(SegmentIndex, IntervalIndex * SampleGap, false));
			}
		}

		return BorderData;
	}

	bool IsInsideBorder(const TArray<FVector2D>& BorderPoints, const FVector2D& Position)
	{
		bool bIsInside = false;
		for (int32 CurrentIndex = 0, PreviousIndex = BorderPoints.Num() - 1;
			CurrentIndex < BorderPoints.Num(); PreviousIndex = CurrentIndex++)
		{
			const FVector2D& CurrentPoint = BorderPoints[CurrentIndex];
			const FVector2D& PreviousPoint = BorderPoints[PreviousIndex];
			const bool bCrossesRay = (CurrentPoint.Y > Position.Y) != (PreviousPoint.Y > Position.Y);
			if (bCrossesRay && Position.X < (PreviousPoint.X - CurrentPoint.X)
				* (Position.Y - CurrentPoint.Y) / (PreviousPoint.Y - CurrentPoint.Y) + CurrentPoint.X)
			{
				bIsInside = not bIsInside;
			}
		}

		return bIsInside;
	}

	double GetDistanceSquaredToBorder(const TArray<FVector2D>& BorderPoints, const FVector2D& Position)
	{
		double MinimumDistanceSquared = TNumericLimits<double>::Max();
		for (int32 PointIndex = 0; PointIndex < BorderPoints.Num(); ++PointIndex)
		{
			const FVector2D& SegmentStart = BorderPoints[PointIndex];
			const FVector2D& SegmentEnd = BorderPoints[(PointIndex + 1) % BorderPoints.Num()];
			const double DistanceSquared = FMath::PointDistToSegmentSquared(
				FVector(Position, 0.0), FVector(SegmentStart, 0.0), FVector(SegmentEnd, 0.0));
			MinimumDistanceSquared = FMath::Min(MinimumDistanceSquared, DistanceSquared);
		}

		return MinimumDistanceSquared;
	}

	double ProjectHeightOntoBorder(const TArray<FVector>& BorderPoints, const FVector2D& Position)
	{
		double WeightedHeight = 0.0;
		double TotalWeight = 0.0;
		for (const FVector& BorderPoint : BorderPoints)
		{
			const double DistanceSquared = FVector2D::DistSquared(Position, FVector2D(BorderPoint));
			if (FMath::IsNearlyZero(DistanceSquared))
			{
				return BorderPoint.Z;
			}

			const double Weight = 1.0 / DistanceSquared;
			WeightedHeight += BorderPoint.Z * Weight;
			TotalWeight += Weight;
		}

		return FMath::IsNearlyZero(TotalWeight) ? 0.0 : WeightedHeight / TotalWeight;
	}

	const UPCGSpatialData* ResolveBoundingShape(
		FPCGContext* Context,
		const UPCGSampleSplineWithBorderOffsetSettings& Settings)
	{
		if (Settings.bUnbounded)
		{
			return nullptr;
		}

		bool bUnionCreated = false;
		const UPCGSpatialData* BoundingShape = Context->InputData.GetSpatialUnionOfInputsByPin(
			Context, BoundingShapePinLabel, bUnionCreated);
		if (BoundingShape != nullptr)
		{
			return BoundingShape;
		}

		const UPCGComponent* SourceComponent = Context->SourceComponent.Get();
		return SourceComponent != nullptr ? Cast<UPCGSpatialData>(SourceComponent->GetActorPCGData()) : nullptr;
	}

	bool IsWithinBoundingShape(
		const UPCGSpatialData* BoundingShape,
		const FTransform& PointTransform,
		const FBox& PointBounds)
	{
		if (BoundingShape == nullptr)
		{
			return true;
		}

		FPCGPoint SampledPoint;
		return BoundingShape->SamplePoint(PointTransform, PointBounds, SampledPoint, nullptr);
	}

	void SetPointSeed(
		FPCGPoint& Point,
		const FVector& LocalPosition,
		const UPCGSampleSplineWithBorderOffsetSettings& Settings,
		const int32 ContextSeed,
		const int32 SampleIndex)
	{
		if (Settings.SeedingMode == EPCGSplineSamplingSeedingMode::SeedFromIndex)
		{
			Point.Seed = PCGHelpers::ComputeSeed(ContextSeed, SampleIndex);
			return;
		}

		const FVector SeedPosition = Settings.bSeedFromLocalPosition
			? LocalPosition
			: Point.Transform.GetLocation();
		Point.Seed = Settings.bSeedFrom2DPosition
			? PCGHelpers::ComputeSeed(static_cast<int32>(SeedPosition.X), static_cast<int32>(SeedPosition.Y))
			: PCGHelpers::ComputeSeedFromPosition(SeedPosition);
	}

	bool IsCandidateValid(
		const FBorderData& BorderData,
		const FVector2D& Position,
		const double RequiredBorderDistanceSquared)
	{
		return IsInsideBorder(BorderData.Points2D, Position)
			&& GetDistanceSquaredToBorder(BorderData.Points2D, Position) >= RequiredBorderDistanceSquared;
	}

	FSamplingGrid BuildSamplingGrid(const FBorderData& BorderData, const double SampleSpacing)
	{
		FSamplingGrid Grid;
		Grid.MinX = FMath::CeilToDouble(BorderData.Bounds.Min.X / SampleSpacing) * SampleSpacing;
		Grid.MaxX = FMath::FloorToDouble(BorderData.Bounds.Max.X / SampleSpacing) * SampleSpacing;
		Grid.MinY = FMath::CeilToDouble(BorderData.Bounds.Min.Y / SampleSpacing) * SampleSpacing;
		Grid.MaxY = FMath::FloorToDouble(BorderData.Bounds.Max.Y / SampleSpacing) * SampleSpacing;
		return Grid;
	}

	double GetRequiredLocalBorderDistance(
		const FBorderData& BorderData,
		const FTransform& SplineTransform,
		const UPCGSampleSplineWithBorderOffsetSettings& Settings)
	{
		if (FMath::IsNearlyZero(Settings.MinDistanceToSplineBorder))
		{
			return 0.0;
		}

		const FVector AbsoluteScale = SplineTransform.GetScale3D().GetAbs();
		const double MinimumPlanarScale = FMath::Min(AbsoluteScale.X, AbsoluteScale.Y);
		const double WorldDistanceInLocalSpace = Settings.MinDistanceToSplineBorder / MinimumPlanarScale;
		const double CurveSafetyMargin = Settings.bTreatSplineAsPolyline
			? 0.0
			: BorderData.MaximumSampleGap * 0.5;
		return WorldDistanceInLocalSpace + CurveSafetyMargin;
	}

	void AddCandidatePoint(
		FPCGPoint& Point,
		const FVector& LocalPosition,
		const FTransform& PointTransform,
		const FBox& PointBounds,
		const UPCGSampleSplineWithBorderOffsetSettings& Settings,
		const int32 ContextSeed,
		const int32 SampleIndex)
	{
		Point.Transform = PointTransform;
		Point.BoundsMin = PointBounds.Min;
		Point.BoundsMax = PointBounds.Max;
		Point.Density = 1.0f;
		Point.Steepness = Settings.PointSteepness;
		SetPointSeed(Point, LocalPosition, Settings, ContextSeed, SampleIndex);
	}

	bool TryAddCandidatePoint(
		const FVector2D& Position,
		const FCandidateSamplingContext& SamplingContext,
		TArray<FPCGPoint>& OutputPoints,
		int32& InOutSampleIndex)
	{
		if (not IsCandidateValid(
			SamplingContext.BorderData,
			Position,
			SamplingContext.RequiredBorderDistanceSquared))
		{
			return false;
		}

		const double LocalZ = SamplingContext.Settings.bProjectOntoSurface
			? ProjectHeightOntoBorder(SamplingContext.BorderData.Points, Position)
			: SamplingContext.BorderData.MinZ;
		const FVector LocalPosition(Position, LocalZ);
		const FTransform PointTransform = FTransform(LocalPosition) * SamplingContext.SplineTransform;
		if (not IsWithinBoundingShape(
			SamplingContext.BoundingShape, PointTransform, SamplingContext.PointBounds))
		{
			return false;
		}

		FPCGPoint& Point = OutputPoints.Emplace_GetRef();
		AddCandidatePoint(
			Point,
			LocalPosition,
			PointTransform,
			SamplingContext.PointBounds,
			SamplingContext.Settings,
			SamplingContext.ContextSeed,
			++InOutSampleIndex);
		return true;
	}

	void SampleSpline(
		FPCGContext* Context,
		const UPCGPolyLineData& Spline,
		const UPCGSpatialData* BoundingShape,
		const UPCGSampleSplineWithBorderOffsetSettings& Settings,
		UPCGPointData& OutputData)
	{
		const FBorderData BorderData = BuildBorderData(Spline, Settings);
		if (BorderData.Points2D.Num() < 3 || not BorderData.Bounds.bIsValid)
		{
			return;
		}

		const FTransform SplineTransform = Spline.GetTransform();
		const FVector AbsoluteScale = SplineTransform.GetScale3D().GetAbs();
		if (FMath::Min(AbsoluteScale.X, AbsoluteScale.Y) <= UE_DOUBLE_SMALL_NUMBER)
		{
			PCGLog::LogErrorOnGraph(LOCTEXT(
				"InvalidSplineScale",
				"Sample Spline With Border Offset cannot sample a spline with zero planar scale."), Context);
			return;
		}

		const FSamplingGrid Grid = BuildSamplingGrid(BorderData, Settings.InteriorSampleSpacing);
		const double HalfSpacing = Settings.InteriorSampleSpacing * 0.5;
		const double RequiredDistance = GetRequiredLocalBorderDistance(BorderData, SplineTransform, Settings);
		const FCandidateSamplingContext SamplingContext{
			BorderData,
			BoundingShape,
			Settings,
			SplineTransform,
			FBox(FVector(-HalfSpacing), FVector(HalfSpacing)),
			FMath::Square(RequiredDistance),
			Context->GetSeed()};
		TArray<FPCGPoint>& OutputPoints = OutputData.GetMutablePoints();
		int32 SampleIndex = 0;

		for (double Y = Grid.MinY; Y <= Grid.MaxY + UE_KINDA_SMALL_NUMBER; Y += Settings.InteriorSampleSpacing)
		{
			for (double X = Grid.MinX; X <= Grid.MaxX + UE_KINDA_SMALL_NUMBER; X += Settings.InteriorSampleSpacing)
			{
				TryAddCandidatePoint(FVector2D(X, Y), SamplingContext, OutputPoints, SampleIndex);
			}
		}
	}
}

#if WITH_EDITOR
FName UPCGSampleSplineWithBorderOffsetSettings::GetDefaultNodeName() const
{
	return FName(TEXT("SampleSplineWithBorderOffset"));
}

FText UPCGSampleSplineWithBorderOffsetSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Sample Spline With Border Offset");
}

FText UPCGSampleSplineWithBorderOffsetSettings::GetNodeTooltipText() const
{
	return LOCTEXT(
		"NodeTooltip",
		"Samples the interior of each closed spline while excluding every point whose planar distance "
		"to the spline border is less than Min Distance To Spline Border.");
}
#endif

TArray<FPCGPinProperties> UPCGSampleSplineWithBorderOffsetSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	Pins.Emplace_GetRef(
		SampleSplineWithBorderOffset::SplinePinLabel,
		EPCGDataType::PolyLine,
		true,
		true).SetRequiredPin();
	Pins.Emplace(
		SampleSplineWithBorderOffset::BoundingShapePinLabel,
		EPCGDataType::Spatial,
		false,
		false);
	return Pins;
}

TArray<FPCGPinProperties> UPCGSampleSplineWithBorderOffsetSettings::OutputPinProperties() const
{
	return DefaultPointOutputPinProperties();
}

FPCGElementPtr UPCGSampleSplineWithBorderOffsetSettings::CreateElement() const
{
	return MakeShared<FPCGSampleSplineWithBorderOffsetElement>();
}

bool FPCGSampleSplineWithBorderOffsetElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);
	const UPCGSampleSplineWithBorderOffsetSettings* Settings =
		Context->GetInputSettings<UPCGSampleSplineWithBorderOffsetSettings>();
	check(Settings);

	const UPCGSpatialData* BoundingShape = SampleSplineWithBorderOffset::ResolveBoundingShape(Context, *Settings);
	const TArray<FPCGTaggedData> SplineInputs = Context->InputData.GetInputsByPin(
		SampleSplineWithBorderOffset::SplinePinLabel);

	for (const FPCGTaggedData& Input : SplineInputs)
	{
		const UPCGPolyLineData* Spline = Cast<UPCGPolyLineData>(Input.Data);
		if (Spline == nullptr || not Spline->IsClosed())
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT(
				"InvalidSpline",
				"Sample Spline With Border Offset requires closed PolyLine data on its Spline input."));
			continue;
		}

		UPCGPointData* OutputData = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);
		OutputData->InitializeFromData(Spline);
		SampleSplineWithBorderOffset::SampleSpline(Context, *Spline, BoundingShape, *Settings, *OutputData);

		FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef(Input);
		Output.Data = OutputData;
		Output.Pin = PCGPinConstants::DefaultOutputLabel;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
