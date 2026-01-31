#pragma once

#include "CoreMinimal.h"

#include "Enum_TopologySearchStrategy.generated.h"

UENUM(BlueprintType)
enum class ETopologySearchStrategy : uint8
{
	NotSet,
	PreferMax,
	PreferMin
};
