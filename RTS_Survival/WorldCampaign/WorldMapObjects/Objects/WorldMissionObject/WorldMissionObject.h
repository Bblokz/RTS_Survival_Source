// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapMission.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMapObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/MapObjects/W_EnemyMapItem/EnemyMapItemUIData/FEnemyMapItemUIData.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/MapObjects/W_MissionReward/RewardStructs/FMissionRewardStructs.h"
#include "WorldMissionObject.generated.h"

/**
 * @brief Spawned marker for mission items placed on anchors.
 */
UCLASS()
class RTS_SURVIVAL_API AWorldMissionObject : public AWorldMapObject
{
	GENERATED_BODY()

public:
	void InitializeForAnchorWithMissionType(AAnchorPoint* AnchorPoint, EMapMission MissionType);

	EMapMission GetMissionType() const { return M_MissionType; }
	const FEnemyOrMissionMapItemUIData& GetMapItemUIData() const { return M_MapItemUIData; }
	const FPrimaryReward& GetPrimaryReward() const { return M_PrimaryReward; }
	const FSecondaryReward& GetSecondaryReward() const { return M_SecondaryReward; }

private:
	UPROPERTY(VisibleAnywhere, Category = "World Campaign|World Objects", meta = (AllowPrivateAccess = "true"))
	EMapMission M_MissionType = EMapMission::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Map Item", meta = (AllowPrivateAccess = "true", ShowOnlyInnerProperties))
	FEnemyOrMissionMapItemUIData M_MapItemUIData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Map Item", meta = (AllowPrivateAccess = "true", ShowOnlyInnerProperties))
	FPrimaryReward M_PrimaryReward;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Map Item", meta = (AllowPrivateAccess = "true", ShowOnlyInnerProperties))
	FSecondaryReward M_SecondaryReward;
};
