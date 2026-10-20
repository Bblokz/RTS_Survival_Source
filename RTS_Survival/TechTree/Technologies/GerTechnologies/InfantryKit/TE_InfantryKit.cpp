// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "TE_InfantryKit.h"

#include "RTS_Survival/Units/SquadController.h"

namespace InfantryKitTechnologySettings
{
	constexpr float HealthMultiplier = 1.15f;
}

UTE_InfantryKit::UTE_InfantryKit()
	: M_HealthMultiplier(InfantryKitTechnologySettings::HealthMultiplier)
{
	Technology = ETechnology::Tech_Infantry_Kit;
	SquadsToApplyTo = {
		ESquadSubtype::Squad_Ger_JagerTruppKar98k,
		ESquadSubtype::Squad_Ger_PanzerGrenadiere,
		ESquadSubtype::Squad_Ger_Gebirgsjagerin,
		ESquadSubtype::Squad_Ger_LMGSquad,
	};
}

void UTE_InfantryKit::ApplyOnSquad_Internal(ASquadController* Squad)
{
	if (not IsValid(Squad))
	{
		return;
	}
	Squad->ApplyTechMaxHealthMultiplier(M_HealthMultiplier);
}
