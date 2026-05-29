// Copyright (C) Bas Blokzijl - All rights reserved.

#include "BehaviourIconStyleDataAsset.h"

const FBehaviourIconWidgetStyle* UBehaviourIconStyleDataAsset::FindBehaviourIconStyle(const EBehaviourIcon BehaviourIcon) const
{
	return BehaviourIconStyles.Find(BehaviourIcon);
}
