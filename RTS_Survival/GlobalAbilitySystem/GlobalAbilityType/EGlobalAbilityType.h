#pragma once


#include "CoreMinimal.h"

#include "EGlobalAbilityType.generated.h"

UENUM(BlueprintType)
enum class EGlobalAbility : uint8
{
	GA_None,
	GA_Me410Airstrike,
	GA_InfantryDrop,
	GA_HarvesterDrop,
	GA_Barrage
};
