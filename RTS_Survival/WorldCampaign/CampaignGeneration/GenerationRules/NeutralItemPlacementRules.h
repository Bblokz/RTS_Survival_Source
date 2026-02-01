#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/TopologySearchStrategy/Enum_TopologySearchStrategy.h"

#include "NeutralItemPlacementRules.generated.h"

USTRUCT(BlueprintType)
struct FNeutralItemPlacementRules
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	int32 MinHopsFromHQ = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	int32 MaxHopsFromHQ = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	int32 MinHopsFromOtherNeutralItems = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	int32 MaxHopsFromOtherNeutralItems = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	ETopologySearchStrategy Preference = ETopologySearchStrategy::NotSet;
};
