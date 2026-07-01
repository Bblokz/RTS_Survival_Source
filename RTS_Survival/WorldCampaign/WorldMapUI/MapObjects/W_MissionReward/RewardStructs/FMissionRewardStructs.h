#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/CardSystem/ERTSCard/ERTSCard.h"
#include "RTS_Survival/FactionSystem/Factions/Factions.h"
#include "RTS_Survival/UnitData/UnitCost.h"


#include "FMissionRewardStructs.generated.h"

struct FUnitCost;

USTRUCT(BlueprintType, Blueprintable)
struct FFactionCardRewards
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	ERTSFaction Faction = ERTSFaction::NotInitialised;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<ERTSCard> Cards = {};
};

USTRUCT(BlueprintType, Blueprintable)
struct FPrimaryReward
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 RadixiteReward = 0;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 CommanderXpReward= 0;
	
	// Contains mapping from blueprint ERTSResource to amount
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FUnitCost Blueprints;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FFactionCardRewards> CardRewards = {};

	const TArray<ERTSCard>& GetCardRewardsForFaction(ERTSFaction PlayerFaction) const;
	
};

USTRUCT(BlueprintType, Blueprintable)
struct FSecondaryReward
{
	GENERATED_BODY()
	
	// Can be extra blueprints or radixite
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FUnitCost SecondaryReward;
	
};
