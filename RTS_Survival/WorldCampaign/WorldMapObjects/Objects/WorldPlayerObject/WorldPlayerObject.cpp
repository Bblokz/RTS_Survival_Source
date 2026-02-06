// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldPlayerObject/WorldPlayerObject.h"

void AWorldPlayerObject::InitializeForAnchorWithPlayerItemType(AAnchorPoint* AnchorPoint, EMapPlayerItem PlayerItemType)
{
	AWorldMapObject::InitializeForAnchor(AnchorPoint);
	M_PlayerItemType = PlayerItemType;
}
