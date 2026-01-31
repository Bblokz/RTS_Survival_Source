#pragma once

#include "CoreMinimal.h"

#include "Enum_PlacementFailurePolicy.generated.h"

UENUM(BlueprintType)
enum class EPlacementFailurePolicy : uint8
{
	NotSet,
	InstantBackTrack,
	BreakDistanceRules_ThenBackTrack
};
