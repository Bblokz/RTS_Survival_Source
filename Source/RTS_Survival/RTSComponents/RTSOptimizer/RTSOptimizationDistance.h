#pragma once

#include "CoreMinimal.h"

#include "RTSOptimizationDistance.generated.h"

UENUM()
enum class ERTSOptimizationDistance :uint8
{
	None,
	InFOV,
	OutFOVClose,
	OutFOVFar
};
