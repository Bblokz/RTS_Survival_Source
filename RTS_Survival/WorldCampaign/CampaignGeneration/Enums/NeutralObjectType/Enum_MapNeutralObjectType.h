#pragma once

#include "CoreMinimal.h"

#include "Enum_MapNeutralObjectType.generated.h"

UENUM(BlueprintType)
enum class EMapNeutralObjectType : uint8
{
	None,
	RadixiteField,
	RuinedCity,
	DenseForrest
};
