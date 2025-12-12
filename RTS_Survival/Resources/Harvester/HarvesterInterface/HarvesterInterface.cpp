// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "HarvesterInterface.h"


// Add default functionality here for any IHarvesterInterface functions that are not pure virtual.
void IHarvesterInterface::OnResourceStorageChanged(int32 PercentageResourcesFilled, const ERTSResourceType ResourceType)
{
	return;
}

void IHarvesterInterface::OnResourceStorageEmpty()
{
}
