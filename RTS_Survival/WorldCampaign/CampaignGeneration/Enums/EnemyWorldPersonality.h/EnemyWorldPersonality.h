#pragma once

#include "CoreMinimal.h"
#include "EnemyWorldPersonality.generated.h"

UENUM(BlueprintType)
enum class EEnemyWorldPersonality : uint8
{
	Balanced,
	Aggressive,
	Defensive,
	Expansive
};