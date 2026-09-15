// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "TeamWeaponCrewMontages.h"

const FTeamWeaponCrewMontageEntry& FTeamWeaponCrewMontage::ResolveEntry(
	const ESquadSubtype TeamWeaponSquadSubtype) const
{
	for (const FTeamWeaponCrewMontageOverride& Override : Overrides)
	{
		if (Override.GetAppliesToSubtype(TeamWeaponSquadSubtype))
		{
			return Override.Entry;
		}
	}
	return Base;
}

FTeamWeaponCrewMontages::FTeamWeaponCrewMontages()
{
	// Loaders are expected to react to the weapon reloading instead of looping.
	Loader.Base.bReactToWeaponFire = true;
	AdditionalLoader.Base.bReactToWeaponFire = true;
}

const FTeamWeaponCrewMontageEntry* FTeamWeaponCrewMontages::ResolveEntry(
	const ECrewPositionType CrewRole,
	const ESquadSubtype TeamWeaponSquadSubtype) const
{
	const FTeamWeaponCrewMontage* CrewMontage = GetMontageForRole(CrewRole);
	if (CrewMontage == nullptr)
	{
		return nullptr;
	}
	return &CrewMontage->ResolveEntry(TeamWeaponSquadSubtype);
}

const FTeamWeaponCrewMontage* FTeamWeaponCrewMontages::GetMontageForRole(const ECrewPositionType CrewRole) const
{
	switch (CrewRole)
	{
	case ECrewPositionType::Gunner:
		return &Gunner;
	case ECrewPositionType::Loader:
		return &Loader;
	case ECrewPositionType::Spotter:
		return &Spotter;
	case ECrewPositionType::AdditionalLoader:
		return &AdditionalLoader;
	case ECrewPositionType::Commander:
		return &Commander;
	case ECrewPositionType::None:
		break;
	}
	return nullptr;
}
