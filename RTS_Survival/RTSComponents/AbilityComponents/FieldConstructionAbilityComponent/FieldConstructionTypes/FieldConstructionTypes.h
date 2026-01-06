#pragma once

#include "CoreMinimal.h"
#include "FieldConstructionTypes.generated.h"

UENUM(BlueprintType)
enum class EFieldConstructionType : uint8
{
	DefaultGerHedgeHog,
	GerBarbedWire,
	GerSandbagWall,
	GerAntiInfantryMine,
	GerAntiInfantryStickMine,
	GerLightATMine,
	GerMediumATMine,
	GerHeavyATMine,
	RussianHedgeHog,
	RussianBarrier,
	RussianWire,
	RussianAntiInfantryStickMine,
	RussianLightATMine,
	RussianMediumATMine,
	RussianHeavyATMine
	
};