// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyDirectorUnitOverridesDataAsset.h"

#include "EnemyDirector.h"

const FEnemyDirectorUnitOverrides* UEnemyDirectorUnitOverridesDataAsset::FindOverrides(
	const EEnemyDirector EnemyDirector) const
{
	switch (EnemyDirector)
	{
	case EEnemyDirector::DirectorOfForge:
		return &M_DirectorOfForgeOverrides;
	case EEnemyDirector::DirectorOfHarvest:
		return &M_DirectorOfHarvestOverrides;
	case EEnemyDirector::DirectorOfShield:
	default:
		return nullptr;
	}
}
