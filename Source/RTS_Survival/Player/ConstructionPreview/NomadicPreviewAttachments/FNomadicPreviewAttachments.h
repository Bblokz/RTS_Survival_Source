#pragma once

#include "CoreMinimal.h"

#include "FNomadicPreviewAttachments.generated.h"

class ANomadicVehicle;

USTRUCT()
struct FNomadicPreviewAttachments
{
	GENERATED_BODY()

	// If the nomadic vehicle has build radius after placement the radius is non zero.
	float BuildingRadius = 0.f;

	FVector AttachmentOffset = FVector::ZeroVector;

};
