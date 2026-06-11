// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "TE_ScavengingReward.h"

#include "RTS_Survival/Scavenging/ScavengerComponent/ScavengerComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"

namespace ScavengingRewardTechnologySettings
{
	constexpr float ScavengingRewardMultiplier = 1.2f;
}

UTE_ScavengingReward::UTE_ScavengingReward()
	: M_ScavengingRewardMultiplier(ScavengingRewardTechnologySettings::ScavengingRewardMultiplier)
{
	Technology = ETechnology::Tech_ScavengingReward;
	SquadsToApplyTo = {
		ESquadSubtype::Squad_Ger_Scavengers,
		ESquadSubtype::Squad_Ger_SturmPionieren,
	};
}

void UTE_ScavengingReward::ApplyOnSquad_Internal(ASquadController* Squad)
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

		ScavengerComponent->ApplyScavengeRewardMultiplier(M_ScavengingRewardMultiplier);
	}
}
