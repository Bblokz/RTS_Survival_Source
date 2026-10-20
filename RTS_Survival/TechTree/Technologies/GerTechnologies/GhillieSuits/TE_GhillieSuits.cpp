// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "TE_GhillieSuits.h"

#include "RTS_Survival/Units/SquadController.h"

namespace GhillieSuitsTechnologySettings
{
	constexpr float HealthMultiplier = 1.33f;
}

UTE_GhillieSuits::UTE_GhillieSuits()
	: M_HealthMultiplier(GhillieSuitsTechnologySettings::HealthMultiplier)
{
	Technology = ETechnology::Tech_Sniper_Ghillie;
	SquadsToApplyTo = {
		ESquadSubtype::Squad_Ger_SniperTeam,
		ESquadSubtype::Squad_Ger_Gebirgsjagerin,
	};
}

void UTE_GhillieSuits::ApplyOnSquad_Internal(ASquadController* Squad)
{
	if (not IsValid(Squad))
	{
		return;
	}
	Squad->ApplyTechMaxHealthMultiplier(M_HealthMultiplier);
}
