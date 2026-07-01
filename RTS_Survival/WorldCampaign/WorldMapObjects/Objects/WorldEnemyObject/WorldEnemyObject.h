// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/BonusObjectives/BonusObjectives.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapEnemyItem.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMapObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/MapObjects/W_EnemyMapItem/EnemyMapItemUIData/FEnemyMapItemUIData.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/MapObjects/W_MissionReward/RewardStructs/FMissionRewardStructs.h"
#include "WorldEnemyObject.generated.h"

/**
 * @brief Spawned marker for enemy items placed on anchors.
 */
UCLASS()
class RTS_SURVIVAL_API AWorldEnemyObject : public AWorldMapObject
{
	GENERATED_BODY()

public:
	void InitializeForAnchorWithEnemyItem(AAnchorPoint* AnchorPoint, EMapEnemyItem EnemyItemType);

	EMapEnemyItem GetEnemyItemType() const { return M_EnemyItemType; }
	const FEnemyOrMissionMapItemUIData& GetMapItemUIData() const { return M_MapItemUIData; }
	const FPrimaryReward& GetPrimaryReward() const { return M_PrimaryReward; }
	const FSecondaryReward& GetSecondaryReward() const { return M_SecondaryReward; }
	EBonusObjective GetBonusObjective() const { return M_BonusObjective; }
	void SetPrimaryReward(const FPrimaryReward& PrimaryReward);

	/**
	 * @brief Applies generated secondary objective data while preserving designer-authored primary objective text.
	 * @param BonusObjective Bonus objective assigned from world data.
	 * @param SecondaryObjectiveText Text shown for the generated bonus objective.
	 * @param SecondaryReward Reward assigned for completing the generated bonus objective.
	 */
	void SetSecondaryObjectiveData(EBonusObjective BonusObjective,
	                               const FText& SecondaryObjectiveText,
	                               const FSecondaryReward& SecondaryReward);

private:
	UPROPERTY(VisibleAnywhere, Category = "World Campaign|World Objects", meta = (AllowPrivateAccess = "true"))
	EMapEnemyItem M_EnemyItemType = EMapEnemyItem::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|World Objects", meta = (AllowPrivateAccess = "true"))
	EBonusObjective M_BonusObjective = EBonusObjective::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Map Item", meta = (AllowPrivateAccess = "true", ShowOnlyInnerProperties))
	FEnemyOrMissionMapItemUIData M_MapItemUIData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Map Item", meta = (AllowPrivateAccess = "true", ShowOnlyInnerProperties))
	FPrimaryReward M_PrimaryReward;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Map Item", meta = (AllowPrivateAccess = "true", ShowOnlyInnerProperties))
	FSecondaryReward M_SecondaryReward;
};
