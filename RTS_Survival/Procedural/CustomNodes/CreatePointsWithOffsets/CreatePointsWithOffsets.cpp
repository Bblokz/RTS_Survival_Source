#include "CreatePointsWithOffsets.h"

#include "PCGContext.h"
#include "PCGPin.h"
#include "Algo/Sort.h"
#include "Data/PCGPointData.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"


#if WITH_EDITOR
FName UPCGCreatePointsWithOffetsSettings::GetDefaultNodeName() const
{
	return FName(TEXT("CreatePointsWithOffsets"));
}

FText UPCGCreatePointsWithOffetsSettings::GetDefaultNodeTitle() const
{
	return FText::FromString("Create Points With Offsets");
}

FText UPCGCreatePointsWithOffetsSettings::GetNodeTooltipText() const
{
	return FText::FromString("Creates points with offsets from the input points.");
}
#endif


TArray<FPCGPinProperties> UPCGCreatePointsWithOffetsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	// Require point data on the default input.
	PinProperties.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point).SetRequiredPin();
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGCreatePointsWithOffetsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	// Output point data.
	Pins.Emplace_GetRef(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);
	return Pins;
}

FPCGElementPtr UPCGCreatePointsWithOffetsSettings::CreateElement() const
{
	return MakeShared<FPCGCreatePointsWithOffsetsElement>();
}

bool FPCGCreatePointsWithOffsetsElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);

	// Get the settings.
	const UPCGCreatePointsWithOffetsSettings* Settings = Context->GetInputSettings<
		UPCGCreatePointsWithOffetsSettings>();
	if (!Settings)
	{
		return false;
	}
	// Ensure there is at least one point to create.
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
	// Go through all the input tagged data.
	for (const FPCGTaggedData& CurrentInput : Inputs)
	{
		// Cast the input data to point data.
		const UPCGPointData* InputPointData = Cast<UPCGPointData>(CurrentInput.Data);
		if (!InputPointData)
		{
			continue;
		}
		// Get the original points.
		const TArray<FPCGPoint>& InPoints = InputPointData->GetPoints();

		// Create new point data for output.
		UPCGPointData* OutPointData = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);
		OutPointData->InitializeFromData(InputPointData);

		TArray<FPCGPoint>& OutPoints = OutPointData->GetMutablePoints();
		// Reserve space for all new points (for each original point, we create as many new points as there are offsets)
		OutPoints.Reserve(InPoints.Num() * Settings->PointsToCreate.Num());

		// For each point, generate the new points.
		for (const FPCGPoint& InPoint : InPoints)
		{
			// Get the original location and rotation.
			const FVector OriginalLocation = InPoint.Transform.GetLocation();
			const FRotator OriginalRotator = InPoint.Transform.GetRotation().Rotator();

			// For each offset to create a new point.
			for (const FPCGPointOffsetToCreate& PointToCreate : Settings->PointsToCreate)
			{
				FPCGPoint NewPoint = InPoint;

				// Compute the total yaw: the original yaw plus the offset angle.
				float TotalYaw = OriginalRotator.Yaw + PointToCreate.Angle;
				// Convert the total yaw to radians.
				const float Radians = FMath::DegreesToRadians(TotalYaw);
				// Compute the offset vector (only in the XY plane, Z remains the same).
				FVector OffsetVector(
					PointToCreate.Offset * FMath::Cos(Radians),
					PointToCreate.Offset * FMath::Sin(Radians),
					0.0f
				);
				// Set the new location.
				FVector NewLocation = OriginalLocation + OffsetVector;
				NewPoint.Transform.SetLocation(NewLocation);
				// Create the rotator of the original point with the added yaw.
				FRotator NewRotator = OriginalRotator;
				NewRotator.Yaw += PointToCreate.PointAddedYaw;
				NewPoint.Transform.SetRotation(NewRotator.Quaternion());

				NewPoint.BoundsMin = -1 * PointToCreate.Bounds;
				NewPoint.BoundsMax = PointToCreate.Bounds;

				// Save the new point to output.
				OutPoints.Add(NewPoint);
			}
		}

		// Add the processed point data to the output.
		FPCGTaggedData OutTagged;
		OutTagged.Data = OutPointData;
		OutTagged.Pin = PCGPinConstants::DefaultOutputLabel;
		Outputs.Add(OutTagged);

		// Update the output data CRC, this is needed for the undo/redo system.
		Context->OutputData.DataCrcs.Add(OutPointData->GetOrComputeCrc(true));

	}

	return true;
}
