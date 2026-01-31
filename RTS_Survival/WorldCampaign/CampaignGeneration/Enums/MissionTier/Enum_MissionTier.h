#pragma once

#include "CoreMinimal.h"

#include "Enum_MissionTier.generated.h"

UENUM(BlueprintType)
enum class EMissionTier : uint8
{
	NotSet,
	Tier1,
	Tier2,
	Tier3,
	Tier4
};
