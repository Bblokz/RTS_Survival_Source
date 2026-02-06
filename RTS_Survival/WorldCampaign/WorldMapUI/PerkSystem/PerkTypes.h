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

USTRUCT(BlueprintType)
struct FPlayerPerkProgress
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	ERTSPerkType PerkType = ERTSPerkType::None;
		
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	int32 CurrentLevel = 0;
};
