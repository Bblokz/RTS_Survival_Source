#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/TopologySearchStrategy/Enum_TopologySearchStrategy.h"

#include "EnemyHQPlacementRules.generated.h"

class AAnchorPoint;

USTRUCT(BlueprintType)
struct FEnemyHQPlacementRules
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	TArray<TObjectPtr<AAnchorPoint>> AnchorCandidates;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	int32 MinAnchorDegree = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	int32 MaxAnchorDegree = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules")
	ETopologySearchStrategy AnchorDegreePreference = ETopologySearchStrategy::NotSet;
};
