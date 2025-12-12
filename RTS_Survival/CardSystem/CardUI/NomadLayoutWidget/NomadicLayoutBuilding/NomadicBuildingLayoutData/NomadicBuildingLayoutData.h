#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/CardSystem/CardUI/NomadLayoutWidget/NomadicLayoutBuilding/NomadicLayoutBuildingType.h"
#include "RTS_Survival/CardSystem/ERTSCard/ERTSCard.h"

#include "NomadicBuildingLayoutData.generated.h"



USTRUCT()
struct FNomadicBuildingLayoutData
{
	GENERATED_BODY()
	
	UPROPERTY(SaveGame)
	ENomadicLayoutBuildingType BuildingType = ENomadicLayoutBuildingType::Building_None;

	UPROPERTY(SaveGame)
	TArray<ERTSCard> Cards = {};

	UPROPERTY(SaveGame)
	int32 Slots = 0;
};