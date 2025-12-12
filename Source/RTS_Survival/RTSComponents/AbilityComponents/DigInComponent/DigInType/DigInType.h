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
};
