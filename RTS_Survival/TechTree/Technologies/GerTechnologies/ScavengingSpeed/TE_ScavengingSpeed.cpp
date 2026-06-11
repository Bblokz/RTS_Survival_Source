// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "TE_ScavengingSpeed.h"

#include "RTS_Survival/Scavenging/ScavengerComponent/ScavengerComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"

namespace ScavengingSpeedTechnologySettings
{
	constexpr float ScavengingSpeedMultiplier = 1.25f;
}

UTE_ScavengingSpeed::UTE_ScavengingSpeed()
	: M_ScavengingSpeedMultiplier(ScavengingSpeedTechnologySettings::ScavengingSpeedMultiplier)
{
	Technology = ETechnology::Tech_ScavengingSpeed;
	SquadsToApplyTo = {
		ESquadSubtype::Squad_Ger_Scavengers,
		ESquadSubtype::Squad_Ger_SturmPionieren,
	};
}

void UTE_ScavengingSpeed::ApplyOnSquad_Internal(ASquadController* Squad)
{
	if (not IsValid(Squad))
	{
		return;
	}

	for (ASquadUnit* SquadUnit : Squad->GetSquadUnitsChecked())
	{
		if (not IsValid(SquadUnit))
		{
			continue;
		}

		UScavengerComponent* ScavengerComponent = SquadUnit->GetScavengerComponent();
		if (not IsValid(ScavengerComponent))
		{
			continue;
		}

		ScavengerComponent->ApplyScavengeSpeedMultiplier(M_ScavengingSpeedMultiplier);
	}
}
