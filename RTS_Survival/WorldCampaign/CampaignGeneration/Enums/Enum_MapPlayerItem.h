#pragma once

#include "CoreMinimal.h"
#include "Enum_MapPlayerItem.generated.h"

UENUM(BlueprintType)
enum class EMapPlayerItem : uint8
{
	None,
	Factory,
	Airfield,
	Barracks
};
