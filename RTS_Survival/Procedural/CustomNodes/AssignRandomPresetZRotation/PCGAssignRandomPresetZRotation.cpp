// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.
#include "PCGAssignRandomPresetZRotation.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"
#include "Math/UnrealMathUtility.h"

#define LOCTEXT_NAMESPACE "PCGAssignRandomPresetZRotation"

#if WITH_EDITOR
FName UPCGAssignRandomPresetZRotationSettings::GetDefaultNodeName() const
{
	return FName(TEXT("AssignRandomPresetZRotation"));
}

FText UPCGAssignRandomPresetZRotationSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Assign Random Preset Z Rotation");
}

FText UPCGAssignRandomPresetZRotationSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Assigns a random preset Z rotation (in degrees) to each input point using the context's seed.");
}
#endif

FPCGElementPtr UPCGAssignRandomPresetZRotationSettings::CreateElement() const
{
	return MakeShared<FPCGAssignRandomPresetZRotationElement>();
}

TArray<FPCGPinProperties> UPCGAssignRandomPresetZRotationSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	// Expect point data on the default input.
	Pins.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point).SetRequiredPin();
	return Pins;
}

TArray<FPCGPinProperties> UPCGAssignRandomPresetZRotationSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	// Output point data.
	Pins.Emplace_GetRef(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);
	return Pins;
}

bool FPCGAssignRandomPresetZRotationElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);

	// Retrieve settings.
	const UPCGAssignRandomPresetZRotationSettings* Settings = Context->GetInputSettings<UPCGAssignRandomPresetZRotationSettings>();
	if (!Settings)
	{
		return false;
	}

	// Ensure there is at least one preset rotation.
	if (Settings->PresetZRotations.Num() == 0)
	{
		// No presets available; nothing to assign.
		return false;
	}

	// Retrieve the input data from the default input pin.
	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	if (Inputs.IsEmpty())
	{
		return false;
	}

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	// Initialize a random stream using the context seed.
	FRandomStream RandStream(Context->GetSeed());

	// Process each input tagged data.
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
		OutPoints.Reserve(InPoints.Num());

		// For each point, assign a random preset Z rotation.
		for (const FPCGPoint& InPoint : InPoints)
		{
			FPCGPoint NewPoint = InPoint; // Copy the original point

			// Randomly pick an index from the PresetZRotations array.
			const int32 NumPresets = Settings->PresetZRotations.Num();
			const int32 RandomIndex = RandStream.RandRange(0, NumPresets - 1);
			const float ChosenZRotation = Settings->PresetZRotations[RandomIndex];

			// Get the current rotation as a rotator.
			FRotator CurrentRot = NewPoint.Transform.GetRotation().Rotator();
			// Update the yaw (Z rotation) with the chosen preset.
			CurrentRot.Yaw = ChosenZRotation;
			// Set the new rotation.
			NewPoint.Transform.SetRotation(FQuat(CurrentRot));

			OutPoints.Add(NewPoint);
		}

		// Add the processed point data to the output.
		FPCGTaggedData OutTagged;
		OutTagged.Data = OutPointData;
		OutTagged.Pin = PCGPinConstants::DefaultOutputLabel;
		Outputs.Add(OutTagged);

		// Update the output data CRC.
		Context->OutputData.DataCrcs.Add(OutPointData->GetOrComputeCrc(true));
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
