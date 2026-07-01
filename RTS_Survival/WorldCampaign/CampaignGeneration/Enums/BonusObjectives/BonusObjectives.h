#pragma once

#include "CoreMinimal.h"

#include "BonusObjectives.generated.h"

UENUM(BlueprintType)
enum class EBonusObjective : uint8
{
	None,
	Bonus_ScavRadixObjects,
	Bonus_DestroyNearbyCamp,
};
