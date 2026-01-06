#pragma once

#include "CoreMinimal.h"
#include "FieldConstructionTypes.generated.h"

UENUM(BlueprintType)
enum class EFieldConstructionType : uint8
{
	DefaultGerHedgeHog,
	GerBarbedWire,
	GerSandbagWall,
	Ger
	RussianHedgeHog,
	RussianBarrier,
	RussianWire,
	
};