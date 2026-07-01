#pragma once

#include "CoreMinimal.h"

#include "WorldTurnType.generated.h"

UENUM(BlueprintType)
enum class EWorldTurnType : uint8
{
	None,
	Player,
	Enemy
};
