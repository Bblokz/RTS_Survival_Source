#pragma once

#include "CoreMinimal.h"

class ICaptureInterface;

struct FCaptureMechanicHelpers
{
	static ICaptureInterface* GetValidCaptureInterface(AActor* TargetActor);
};
