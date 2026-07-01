// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/BonusObjectives/BonusObjectives.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapEnemyItem.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/MapObjects/W_MissionReward/RewardStructs/FMissionRewardStructs.h"
#include "WorldData.generated.h"

/**
 * @brief Ordered primary reward pool used for one enemy map item type.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldDataEnemyPrimaryRewardPool
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Rewards")
	EMapEnemyItem EnemyItemType = EMapEnemyItem::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Rewards")
	TArray<FPrimaryReward> PrimaryRewards;
};

/**
 * @brief Bonus objective definition used to procedurally fill enemy secondary objectives.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldDataBonusObjectiveDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Bonus Objectives")
	EBonusObjective BonusObjective = EBonusObjective::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Bonus Objectives")
	FText SecondaryObjectiveText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Bonus Objectives")
	FSecondaryReward SecondaryReward;
};

/**
 * @brief Designer-authored enemy data loaded into generated enemy map objects after placement.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UWorldData : public UDataAsset
{
	GENERATED_BODY()

public:
	const FWorldDataEnemyPrimaryRewardPool* FindEnemyPrimaryRewardPool(EMapEnemyItem EnemyItemType) const;
	const TArray<FWorldDataBonusObjectiveDefinition>& GetBonusObjectiveDefinitions() const
	{
		return M_BonusObjectiveDefinitions;
	}

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Rewards",
		meta = (AllowPrivateAccess = "true"))
	TArray<FWorldDataEnemyPrimaryRewardPool> M_EnemyPrimaryRewardPools;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Bonus Objectives",
		meta = (AllowPrivateAccess = "true"))
	TArray<FWorldDataBonusObjectiveDefinition> M_BonusObjectiveDefinitions;
};
