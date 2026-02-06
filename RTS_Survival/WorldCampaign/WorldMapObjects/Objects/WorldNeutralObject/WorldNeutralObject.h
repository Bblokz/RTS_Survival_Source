// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/NeutralObjectType/Enum_MapNeutralObjectType.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMapObject.h"
#include "WorldNeutralObject.generated.h"

/**
 * @brief Spawned marker for neutral items placed on anchors.
 */
UCLASS()
class RTS_SURVIVAL_API AWorldNeutralObject : public AWorldMapObject
{
	GENERATED_BODY()

public:
	void InitializeForAnchorWithNeutralObjectType(AAnchorPoint* AnchorPoint, EMapNeutralObjectType NeutralObjectType);

	EMapNeutralObjectType GetNeutralObjectType() const { return M_NeutralObjectType; }

private:
	UPROPERTY(VisibleAnywhere, Category = "World Campaign|World Objects", meta = (AllowPrivateAccess = "true"))
	EMapNeutralObjectType M_NeutralObjectType = EMapNeutralObjectType::None;
};
