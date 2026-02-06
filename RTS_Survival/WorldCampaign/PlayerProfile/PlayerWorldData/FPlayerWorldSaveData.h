#pragma once
#include "CoreMinimal.h"
#include "ArchiveSaveData/FArchiveSaveData.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/PerkSystem/PerkTypes.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/PerkSystem/PlayerRank/PlayerRanks.h"

#include "FPlayerWorldSaveData.generated.h"

enum class ERTSFaction : uint8;

USTRUCT(BlueprintType)
struct FPlayerPerkSaveData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	int32 UnspentPerkPoints = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	FPlayerRankProgress RankProgress;

	// Contains one progress data struct for each perk type supported in the UI.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	TArray<FPlayerPerkProgress> PerkProgress;

};

USTRUCT(BlueprintType)
struct FPlayerWorldSaveData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	FPlayerPerkSaveData PerkData;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	FArchiveSaveData ArchiveData;
	
};
