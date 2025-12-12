#pragma once
#include "CoreMinimal.h"

UENUM(BlueprintType)
enum class ERTSInfantryBalanceSetting: uint8
{
	RTS_Inf_SlowSpeed,
	RTS_Inf_SlowAcceleration,
	RTS_Inf_BasicSpeed,
	RTS_Inf_BasicAcceleration,
	RTS_Inf_FastAcceleration,
	RTS_Inf_FastSpeed,
};
