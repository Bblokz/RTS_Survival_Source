// Copyright (C) Bas Blokzijl - All rights reserved.

#include "UnitVariantSettings.h"

#include "UnitVariantDataAsset.h"

const UUnitVariantDataAsset* UUnitVariantSettings::LoadVariantDataAsset() const
{
	if (const UUnitVariantDataAsset* LoadedAsset = VariantDataAsset.LoadSynchronous())
	{
		return LoadedAsset;
	}
	return GetDefault<UUnitVariantDataAsset>();
}
