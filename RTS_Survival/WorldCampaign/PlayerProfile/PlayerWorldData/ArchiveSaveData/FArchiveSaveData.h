#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GameUI/Archive/ArchiveItemTypes/ArchiveItemTypes.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"

#include "FArchiveSaveData.generated.h"

USTRUCT(BlueprintType)
struct FArchiveItemSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
	ERTSArchiveItem ItemType = ERTSArchiveItem::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
	FTrainingOption OptionalUnit  = FTrainingOption();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
	int32 SortingPriority = 0;
};

USTRUCT(BlueprintType)
struct FArchiveSaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, meta=(TitleProperty="ItemType"))
	TArray<FArchiveItemSaveData> Items;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
	ERTSArchiveType ActiveSortingType = ERTSArchiveType::GameMechanics;
};
