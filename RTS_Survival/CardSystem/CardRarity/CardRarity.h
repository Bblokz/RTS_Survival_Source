#pragma once


#include "CoreMinimal.h"

#include "CardRarity.generated.h"

UENUM(BlueprintType)
enum class ERTSCardRarity : uint8
{
	Rarity_Common,
	Rarity_UnCommon,
	Rarity_Rare,
	Rarity_VeryRare,
	Rarity_Legendary,
	Rarity_Mythic
};
