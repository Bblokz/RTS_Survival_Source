// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapEnemyItem.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMapObject.h"
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

private:
	UPROPERTY(VisibleAnywhere, Category = "World Campaign|World Objects", meta = (AllowPrivateAccess = "true"))
	EMapEnemyItem M_EnemyItemType = EMapEnemyItem::None;
};
