#pragma once

#include "CoreMinimal.h"

#include "PerkTypes.generated.h"

UENUM(BlueprintType)
enum class ERTSPerkType : uint8
{
	None,
	InfantryAndMechanized,
	LightAndMediumArmor,
	HeavyArmor,
	Aircraft
};

USTRUCT()
struct FPlayerPerkProgress
{
	GENERATED_BODY()
	UPROPERTY()
	ERTSPerkType PerkType = ERTSPerkType::None;
		
	UPROPERTY()
	int32 CurrentLevel = 0;
};
