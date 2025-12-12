// Copyright (C) Bas Blokzijl - All rights reserved.

#include "PCGSampleNeighborhoods.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"
#include "Math/UnrealMathUtility.h"

#define LOCTEXT_NAMESPACE "PCGSampleNeighborhoods"

// ~Begin UPCGSampleNeighborhoodsSettings implementation

TArray<FPCGPinProperties> UPCGSampleNeighborhoodsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	// Expect point data on the default input.
	Pins.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point).SetRequiredPin();
	return Pins;
}

TArray<FPCGPinProperties> UPCGSampleNeighborhoodsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	// Output is point data.
	Pins.Emplace_GetRef(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);
	return Pins;
}

FPCGElementPtr UPCGSampleNeighborhoodsSettings::CreateElement() const
{
	return MakeShared<FPCGSampleNeighborhoodsElement>();
}

// ~End UPCGSampleNeighborhoodsSettings implementation

// ============================================================================
// Helper function: Given an input point, compute an inner radius based on its XY bounds.
// We assume the point's BoundsMin and BoundsMax represent the extent around its center.
// Now we add the future bounds (XYBounds) so that generated points do not overlap.
static float ComputeInnerRadius(const FPCGPoint& InPoint, float FutureBound)
{
	// Compute half-size in X and Y.
	float HalfX = 0.5f * FMath::Abs(InPoint.BoundsMax.X - InPoint.BoundsMin.X);
	float HalfY = 0.5f * FMath::Abs(InPoint.BoundsMax.Y - InPoint.BoundsMin.Y);
	// Use the larger half-extent and add the future bounds.
	return FMath::Max(HalfX, HalfY) + FutureBound;
}

// ============================================================================
// Execution element implementation

bool FPCGSampleNeighborhoodsElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);

	// Create a random stream using the context's seed.
	FRandomStream RandStream(Context->GetSeed());

	// Retrieve input point data.
	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	if (Inputs.Num() == 0)
	{
		return false;
	}
	TArray<FPCGPoint> OutPoints;

	// We expect point data.
	for (const FPCGTaggedData& Tagged : Inputs)
	{
		const auto InputPointData = Cast<UPCGPointData>(Tagged.Data.Get());
		if (not InputPointData)
		{
			continue;
		}

		const UPCGSampleNeighborhoodsSettings* Settings = Context->GetInputSettings<UPCGSampleNeighborhoodsSettings>();
		const TArray<FPCGPoint>& InPoints = InputPointData->GetPoints();

		for (const FPCGPoint& InPoint : InPoints)
		{
			// Use the input point's transform location as the center.
			FVector Center = InPoint.Transform.GetLocation();

			// Compute the inner radius based on the input point's bounds and add the future point bounds (XYBounds)
			float InnerRadius = ComputeInnerRadius(InPoint, Settings->XYBounds);
			const float OuterRadius = InnerRadius + Settings->Radius;

			// Determine how many points to sample for this input point.
			int32 NumSamples = RandStream.RandRange(Settings->MinPoints, Settings->MaxPoints);

			TArray<float> ChosenAngles;
			ChosenAngles.Reserve(NumSamples);

			// If more than one sample is needed, use the Spread parameter to space out the angles.
			if (NumSamples > 1)
			{
				// Evenly space the angles over 360 degrees.
				float AngleSpacing = 360.f / NumSamples;
				// Pick a random starting angle.
				float BaseAngle = RandStream.FRandRange(0.f, 360.f);
				for (int32 i = 0; i < NumSamples; ++i)
				{
					float IdealAngle = BaseAngle + i * AngleSpacing;
					// Compute jitter: if Spread==1 then jitter is 0 (perfect spacing), if Spread==0 then jitter can be up to half the spacing.
					float MaxJitter = AngleSpacing * 0.5f * (1.f - Settings->Spread);
					float Jitter = RandStream.FRandRange(-MaxJitter, MaxJitter);
					float FinalAngle = IdealAngle + Jitter;
					// Ensure the angle stays within 0-360 degrees.
					FinalAngle = FMath::Fmod(FinalAngle, 360.f);
					if (FinalAngle < 0.f)
					{
						FinalAngle += 360.f;
					}
					ChosenAngles.Add(FinalAngle);
				}
			}
			else
			{
				// Only one sample: use a completely random angle.
				ChosenAngles.Add(RandStream.FRandRange(0.f, 360.f));
			}

			// For each chosen angle, generate a sample point.
			for (float AngleDeg : ChosenAngles)
			{
				float AngleRad = FMath::DegreesToRadians(AngleDeg);
				float Distance = RandStream.FRandRange(InnerRadius, OuterRadius);
				FVector2D Offset = FVector2D(FMath::Cos(AngleRad), FMath::Sin(AngleRad)) * Distance;
				FVector NewPos = Center + FVector(Offset.X, Offset.Y, 0.f);

				// Create a new point with the new position.
				FPCGPoint NewPoint;
				NewPoint.Transform.SetLocation(NewPos);
				NewPoint.BoundsMin = FVector(-Settings->XYBounds, -Settings->XYBounds, -1.f);
				NewPoint.BoundsMax = FVector(Settings->XYBounds, Settings->XYBounds, 1.f);
				NewPoint.Density = 1.0f;
				// Assign a new random seed for this point so that subsequent nodes see different seeds.
				NewPoint.Seed = RandStream.RandHelper(INT32_MAX);

				OutPoints.Add(NewPoint);
			}
		}
	}

	// Create output point data.
	UPCGPointData* OutPointData = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);
	OutPointData->InitializeFromData(nullptr);
	OutPointData->SetPoints(OutPoints);

	// Populate the output collection.
	Context->OutputData.TaggedData.Reset();
	FPCGTaggedData OutTagged;
	OutTagged.Data = OutPointData;
	OutTagged.Pin = PCGPinConstants::DefaultOutputLabel;
	Context->OutputData.TaggedData.Add(OutTagged);

	Context->OutputData.DataCrcs.Reset();
	Context->OutputData.DataCrcs.Add(OutPointData->GetOrComputeCrc(true));

	return true;
}

#undef LOCTEXT_NAMESPACE
