#pragma once

#include "CoreMinimal.h"

UENUM(BlueprintType)
enum class EHealthLevel: uint8
{
	Level_NoAction,
	Level_100Percent,
	Level_75Percent,
	Level_66Percent,
	Level_50Percent,
	Level_33Percent,
	Level_25Percent
};