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
	GA_Barrage,
	GA_LargeBarrage,
	GA_3xJu87Strafing,
	GA_Ju87Strafing,
	GA_3xBf109Strafing,
	GA_HeavyCarpet_Ju390B_250Gr,
	GA_HeavyCarpet_Ju390B_500Gr,
	GA_HeavyCarpet_PE8_500Gr,
	GA_HeavyCarpet_PE8_1000Gr,
	GA_HeavyCarpet_PE8_3000Gr,
	GA_2xPE2Strafing,
	GA_2xHortentrafing,
};
