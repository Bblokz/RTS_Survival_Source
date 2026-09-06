// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyDirectorUnitOverrideSettings.h"

#include "EnemyDirectorUnitOverridesDataAsset.h"

UEnemyDirectorUnitOverrideSettings::UEnemyDirectorUnitOverrideSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("Enemy Director Unit Overrides");
}

const UEnemyDirectorUnitOverridesDataAsset*
UEnemyDirectorUnitOverrideSettings::GetUnitOverridesDataAsset() const
{
	if (M_UnitOverridesDataAsset.IsNull())
	{
		return nullptr;
	}

	return M_UnitOverridesDataAsset.LoadSynchronous();
}
