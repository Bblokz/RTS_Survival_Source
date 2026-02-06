// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapMission.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMapObject.h"
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

private:
	UPROPERTY(VisibleAnywhere, Category = "World Campaign|World Objects", meta = (AllowPrivateAccess = "true"))
	EMapMission M_MissionType = EMapMission::None;
};
