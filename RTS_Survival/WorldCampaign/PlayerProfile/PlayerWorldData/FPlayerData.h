#pragma once
#include "CoreMinimal.h"
#include "ArchiveSaveData/FArchiveSaveData.h"
#include "RTS_Survival/WorldCampaign/PlayerProfile/PlayerCardData/PlayerCardData.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/PerkSystem/PerkTypes.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/PerkSystem/PlayerRank/PlayerRanks.h"

#include "FPlayerData.generated.h"

enum class ERTSFaction : uint8;

USTRUCT(BlueprintType)
struct FPlayerPerkSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
	int32 UnspentPerkPoints = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
	FPlayerRankProgress RankProgress;

	// Contains one progress data struct for each perk type supported in the UI.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, meta=(TitleProperty="PerkType"))
	TArray<FPlayerPerkProgress> PerkProgress;

};

USTRUCT(BlueprintType)
struct FPlayerData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
	FPlayerPerkSaveData PerkData;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
	FArchiveSaveData ArchiveData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
	FPlayerCardSaveData CardData;
	
	
};
