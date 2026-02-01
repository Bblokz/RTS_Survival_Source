#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/AdjacencyPolicy/Enum_AdjacencyPolicy.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapItemType.h"

#include "MapObjectAdjacencyRequirement.generated.h"

USTRUCT(BlueprintType)
struct FMapObjectAdjacencyRequirement
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirement")
	bool bEnabled = false;

	// What must be nearby (one connection away):
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirement", meta = (EditCondition = "bEnabled"))
	EMapItemType RequiredItemType = EMapItemType::NeutralItem;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirement", meta = (EditCondition = "bEnabled"))
	uint8 RawByteSpecificItemSubtype = 0; // use static cast to set EMapNeutralObjectType, EMapMission, etc.

	// How many of these nearby?
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirement", meta = (EditCondition = "bEnabled"))
	int32 MinMatchingCount = 1;

	// Define “nearby”: the max connections away
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Adjacency", meta = (EditCondition = "bEnabled"))
	int32 MaxHops = 1;

	// What to do if missing:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirement", meta = (EditCondition = "bEnabled"))
	EAdjacencyPolicy Policy = EAdjacencyPolicy::RejectIfMissing;
};
