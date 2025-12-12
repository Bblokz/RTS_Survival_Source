#pragma once


#include "CoreMinimal.h"

#include "RepairState.generated.h"

UENUM()
enum class ERepairState : uint8
{
	None,
	MovingToRepairableActor,
	MovingToRepairLocation,
	Repairing
};
