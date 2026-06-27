#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/CardSystem/ERTSCard/ERTSCard.h"
#include "RTS_Survival/UnitData/UnitCost.h"


#include "FMissionRewardStructs.generated.h"

struct FUnitCost;

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
	TArray<ERTSCard> CardRewards = {};
	
};

USTRUCT(BlueprintType, Blueprintable)
struct FSecondaryReward
{
	GENERATED_BODY()
	
	// Can be extra blueprints or radixite
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FUnitCost Blueprints;
	
};
