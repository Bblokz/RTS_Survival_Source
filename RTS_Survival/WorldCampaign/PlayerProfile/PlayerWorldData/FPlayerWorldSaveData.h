#pragma once
#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/PerkSystem/PerkTypes.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/PerkSystem/PlayerRank/PlayerRanks.h"

#include "FPlayerWorldSaveData.generated.h"

enum class ERTSFaction : uint8;

USTRUCT(BlueprintType)
struct FPlayerPerkSaveData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	int32 UnspentPerkPoints = 0;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	FPlayerRankProgress RankProgress;

	// Contains one progress data struct for each perk type supported in the UI.
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	TArray<FPlayerPerkProgress> PerkProgress;

	
};

USTRUCT(BlueprintType)
struct FPlayerWorldSaveData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	FPlayerPerkSaveData PerkData;
	

	
};
