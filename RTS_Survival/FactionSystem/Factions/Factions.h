#pragma once

#include "CoreMinimal.h"

#include "Factions.generated.h"

UENUM(BlueprintType)
enum class ERTSFaction : uint8
{
	NotInitialised,
	GerBreakthroughDoctrine,
	GerStrikeDivision,
	GerItalianFaction
};