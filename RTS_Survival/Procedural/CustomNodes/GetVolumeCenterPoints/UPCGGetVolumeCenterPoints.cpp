// UPCGGetVolumeCenterPoints.cpp

#include "UPCGGetVolumeCenterPoints.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Math/UnrealMathUtility.h"

#define LOCTEXT_NAMESPACE "PCGGetVolumeCenterPoints"

#if WITH_EDITOR
FName UPCGGetVolumeCenterPointsSettings::GetDefaultNodeName() const
{
	return FName(TEXT("PCGGetVolumeCenterPoints"));
}

FText UPCGGetVolumeCenterPointsSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Get Volume Center Points");
}

FText UPCGGetVolumeCenterPointsSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Computes the center of each input volume and clamps the result within specified bounds.");
}
#endif

FPCGElementPtr UPCGGetVolumeCenterPointsSettings::CreateElement() const
{
	return MakeShared<FPCGGetVolumeCenterPointsElement>();
}

TArray<FPCGPinProperties> UPCGGetVolumeCenterPointsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	// Expect spatial (volume) data on the default input.
	Pins.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Spatial).SetRequiredPin();
	return Pins;
}

TArray<FPCGPinProperties> UPCGGetVolumeCenterPointsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	// Output is point data.
	Pins.Emplace_GetRef(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);
	return Pins;
}

bool FPCGGetVolumeCenterPointsElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);

	// Retrieve the input spatial data (volumes) from the default input pin.
	const TArray<FPCGTaggedData>& Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	if (Inputs.IsEmpty())
	{
		return false;
	}

	// Retrieve the settings.
	const UPCGGetVolumeCenterPointsSettings* Settings = Context->GetInputSettings<UPCGGetVolumeCenterPointsSettings>();
	if (!Settings)
	{
		return false;
	}

	// Container for the generated points.
	TArray<FPCGPoint> OutPoints;

	// Process each tagged input.
	for (const FPCGTaggedData& Tagged : Inputs)
	{
		// Cast input data to spatial data.
		const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Tagged.Data.Get());
		if (!SpatialData)
		{
			continue;
		}

		// Compute the center of the volume.
		// Instead of using the full bounds (which may include extra padding), use the strict bounds.
		const FBox StrictBox = SpatialData->GetBounds();
		FVector Center = StrictBox.GetCenter();
		Center.Z = Settings->ZValueOfCenter;


		// Create a new PCG point.
		FPCGPoint NewPoint;
		NewPoint.Transform.SetLocation(Center);
		NewPoint.BoundsMin = FVector(Settings->CenterBoundsMin.X, Settings->CenterBoundsMin.Y, Settings->CenterBoundsMin.Z);
		NewPoint.BoundsMax = FVector(Settings->CenterBoundsMax.X, Settings->CenterBoundsMax.Y, Settings->CenterBoundsMax.Z);
		NewPoint.Density = 1.0f;
		OutPoints.Add(NewPoint);
	}

	// If no valid points were generated, fail.
	if (OutPoints.IsEmpty())
	{
		return false;
	}

	// Create the output point data.
	UPCGPointData* OutPointData = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);
	OutPointData->InitializeFromData(nullptr);
	OutPointData->SetPoints(OutPoints);

	// Update the context's output collection.
	Context->OutputData.TaggedData.Reset();
	FPCGTaggedData OutTagged;
	OutTagged.Data = OutPointData;
	OutTagged.Pin = PCGPinConstants::DefaultOutputLabel;
	Context->OutputData.TaggedData.Add(OutTagged);

	// Update the output data CRCs.
	Context->OutputData.DataCrcs.Add(OutPointData->GetOrComputeCrc(true));

	return true;
}

#undef LOCTEXT_NAMESPACE
