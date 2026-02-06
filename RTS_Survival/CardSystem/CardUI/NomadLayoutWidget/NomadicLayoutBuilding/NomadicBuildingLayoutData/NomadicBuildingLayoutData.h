#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/CardSystem/CardUI/NomadLayoutWidget/NomadicLayoutBuilding/NomadicLayoutBuildingType.h"
#include "RTS_Survival/CardSystem/ERTSCard/ERTSCard.h"

#include "NomadicBuildingLayoutData.generated.h"


USTRUCT(BlueprintType)
struct FNomadicBuildingLayoutData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
	ENomadicLayoutBuildingType BuildingType = ENomadicLayoutBuildingType::Building_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
	TArray<ERTSCard> Cards = {};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame)
	int32 Slots = 0;
};
