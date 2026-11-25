// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyItemStrengthSlot.h"

namespace EnemyItemStrengthSlotConstants
{
	const FColor SlotColor(200, 120, 60);
}

bool AEnemyItemStrengthSlot::GetShouldActivateForSite(const FEnemyItemSiteContext& SiteContext) const
{
	return EnemyItemSiteRules::GetShouldActivateStrengthSlot(Condition, SiteContext);
}

FColor AEnemyItemStrengthSlot::GetPlacementColor() const
{
	return EnemyItemStrengthSlotConstants::SlotColor;
}
