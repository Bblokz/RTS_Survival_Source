#pragma once

#include "CoreMinimal.h"

#include "Enum_AdjacencyPolicy.generated.h"

UENUM(BlueprintType)
enum class EAdjacencyPolicy : uint8
{
	NotSet,
	RejectIfMissing,         // mission candidate anchor is invalid unless requirement already satisfied
	TryAutoPlaceCompanion    // if missing, try to place the required object on a connected yet empty anchor or
	                         // in case this about a neutral object and having a mission or map object spawned on top of it;
	                         // try spawn the neutral object on an yet empty anchor and place our object on top of that.
};
