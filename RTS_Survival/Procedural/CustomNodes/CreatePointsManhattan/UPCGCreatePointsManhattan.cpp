#include "UPCGCreatePointsManhattan.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"

TArray<FPCGPinProperties> UPCGCreatePointsManhattan::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	// Require point data on the default input.
	PinProperties.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point).SetRequiredPin();
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGCreatePointsManhattan::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	// Output point data.
	PinProperties.Emplace_GetRef(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);
	return PinProperties;
}

FPCGElementPtr UPCGCreatePointsManhattan::CreateElement() const
{
	return MakeShared<FPCGCreatePointsManhattanElement>();
}

bool FPCGCreatePointsManhattanElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);

	// Retrieve the settings from the input.
	const UPCGCreatePointsManhattan* Settings = Context->GetInputSettings<UPCGCreatePointsManhattan>();
	if (!Settings)
	{
		return false;
	}
	// Ensure there is at least one offset defined.
	if (Settings->PointsToCreate.Num() == 0)
	{
		return false;
	}

	// Get input data from the default input pin.
	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	if (Inputs.IsEmpty())
	{
		return false;
	}

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	// Process each input data set.
	for (const FPCGTaggedData& CurrentInput : Inputs)
	{
		// Cast input data to point data.
		const UPCGPointData* InputPointData = Cast<UPCGPointData>(CurrentInput.Data);
		if (!InputPointData)
		{
			continue;
		}
		const TArray<FPCGPoint>& InPoints = InputPointData->GetPoints();
		
		// Create new point data for output.
		UPCGPointData* OutPointData = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);
		OutPointData->InitializeFromData(InputPointData);

		TArray<FPCGPoint>& OutPoints = OutPointData->GetMutablePoints();
		OutPoints.Reserve(InPoints.Num() * Settings->PointsToCreate.Num());

		// For each original point, generate new points based on the Manhattan offsets.
		for (const FPCGPoint& InPoint : InPoints)
		{
			const FVector OriginalLocation = InPoint.Transform.GetLocation();
			const FRotator OriginalRotator = InPoint.Transform.GetRotation().Rotator();

			// Create a new point for each defined offset.
			for (const FPCGPointManhattanOffsetToCreate& ManhattanOffset : Settings->PointsToCreate)
			{
				FPCGPoint NewPoint = InPoint;
				// Compute the offset vector relative to the original point's rotation.
				const FVector LocalOffset(ManhattanOffset.XOffset, ManhattanOffset.YOffset, 0.0f);
				const FVector RotatedOffset = OriginalRotator.RotateVector(LocalOffset);
				FVector NewLocation = OriginalLocation + RotatedOffset;
				NewLocation.Z += ManhattanOffset.ZOffset;
				NewPoint.Transform.SetLocation(NewLocation);

				// Adjust the yaw rotation using the provided PointAddedYaw.
				FRotator NewRotator = OriginalRotator;
				NewRotator.Yaw += ManhattanOffset.PointAddedYaw;
				NewPoint.Transform.SetRotation(NewRotator.Quaternion());
				NewPoint.BoundsMin = -1 * ManhattanOffset.Bounds/2;
				NewPoint.BoundsMax = ManhattanOffset.Bounds/2;

				// Add the newly created point to the output.
				OutPoints.Add(NewPoint);
			}
		}

		// Package the output point data.
		FPCGTaggedData OutTagged;
		OutTagged.Data = OutPointData;
		OutTagged.Pin = PCGPinConstants::DefaultOutputLabel;
		Outputs.Add(OutTagged);

		// Update the output data CRC for the undo/redo system.
		Context->OutputData.DataCrcs.Add(OutPointData->GetOrComputeCrc(true));
	}

	return true;
}
