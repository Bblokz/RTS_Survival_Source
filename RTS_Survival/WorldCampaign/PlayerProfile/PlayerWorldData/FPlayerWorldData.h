#pragma once
#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/PerkSystem/PerkTypes.h"

#include "FPlayerWorldData.generated.h"

enum class ERTSFaction : uint8;

USTRUCT(BlueprintType)
struct FPlayerPerkData
{
	GENERATED_BODY()

	UPROPERTY()
	int32 UnspentPerkPoints = 0;

	UPROPERTY()
	TArray<FPlayerPerkProgress> PerkProgress;
	
};

USTRUCT(BlueprintType)
struct FPlayerWorldData
{
	GENERATED_BODY()

	

	
};
