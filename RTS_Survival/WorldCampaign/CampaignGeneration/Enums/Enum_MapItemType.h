#pragma once

#include "CoreMinimal.h"
#include "Enum_MapItemType.generated.h"

UENUM(BlueprintType)
enum class EMapItemType : uint8
{
	None,
	Empty,
	Mission,
	EnemyItem,
	PlayerItem,
	NeutralItem
};
