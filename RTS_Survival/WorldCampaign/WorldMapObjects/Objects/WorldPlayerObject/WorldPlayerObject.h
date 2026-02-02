// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapPlayerItem.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMapObject.h"
#include "WorldPlayerObject.generated.h"

/**
 * @brief Spawned marker for player-owned campaign items placed on anchors.
 */
UCLASS()
class RTS_SURVIVAL_API AWorldPlayerObject : public AWorldMapObject
{
	GENERATED_BODY()

public:
	void InitializeForAnchor(AAnchorPoint* AnchorPoint, EMapPlayerItem PlayerItemType);

	EMapPlayerItem GetPlayerItemType() const { return M_PlayerItemType; }

private:
	UPROPERTY(VisibleAnywhere, Category = "World Campaign|World Objects", meta = (AllowPrivateAccess = "true"))
	EMapPlayerItem M_PlayerItemType = EMapPlayerItem::None;
};
