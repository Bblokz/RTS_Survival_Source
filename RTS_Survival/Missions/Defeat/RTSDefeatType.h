#pragma once

#include "CoreMinimal.h"
#include "RTSDefeatType.generated.h"

UENUM(BlueprintType)
enum class ERTSDefeatType : uint8
{
	Uninitialized,
	LostHQ,
	LostCommandVehicle,
	LostMainObjective
};
