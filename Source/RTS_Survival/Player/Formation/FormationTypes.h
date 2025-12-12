#pragma once

#include "CoreMinimal.h"

#include "FormationTypes.generated.h"

UENUM(BlueprintType)
enum class EFormation : uint8
{
	RectangleFormation,
	SpearFormation,
	ThinSpearFormation,
	SemiCircleFormation,
};
