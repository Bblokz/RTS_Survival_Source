#pragma once

#include "CoreMinimal.h"

#include "Enum_EnemyRuleVariantSelectionMode.generated.h"

UENUM(BlueprintType)
enum class EEnemyRuleVariantSelectionMode : uint8
{
	None,
	CycleByPlacementIndex   // VariantIndex = InstanceIndex % Variants.Num()
};
