// Copyright (C) Bas Blokzijl - All rights reserved.

#include "MinimapIconDataAsset.h"

const FMinimapIcon* UMinimapIconDataAsset::FindMinimapIcon(const EMinimapIconType IconType) const
{
	return M_MinimapIcons.Find(IconType);
}
