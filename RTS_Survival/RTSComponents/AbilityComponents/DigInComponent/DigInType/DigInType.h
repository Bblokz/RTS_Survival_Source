#pragma once

#include "CoreMinimal.h"

#include "DigInType.generated.h"

UENUM(BlueprintType)
enum class EDigInType : uint8
{
	None,
	GerLightTankWall,
	GerMediumTankWall,
	GerHeavyTankWall,
	RusLightTankWall,
	RusMediumTankWall,
	RusHeavyTankWall,
	// Closed sandbag ring around a soft-skinned Soviet AA truck (GAZ-MM 72-K): front, sides and rear.
	RusAATruckRingWall,
};
