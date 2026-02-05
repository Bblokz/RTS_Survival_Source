#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GameUI/Archive/ArchiveItemTypes/ArchiveItemTypes.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"

#include "FArchiveSaveData.generated.h"

USTRUCT(BlueprintType)
struct FArchiveItemSaveData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	ERTSArchiveItem ItemType = ERTSArchiveItem::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	FTrainingOption OptionalUnit  = FTrainingOption();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	int32 SortingPriority = 0;
};

USTRUCT(BlueprintType)
struct FArchiveSaveData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	TArray<FArchiveItemSaveData> Items = {};

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	ERTSArchiveType ActiveSortingType = ERTSArchiveType::GameMechanics;
};