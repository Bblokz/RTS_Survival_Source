// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "TE_RepairUpgrade.h"

#include "RTS_Survival/RTSComponents/RepairComponent/RepairComponent.h"
#include "RTS_Survival/TechTree/Technologies/GerTechnologies/RepairUpgrade/BehaviourRepairUpgrade.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"

namespace RepairUpgradeTechnologySettings
{
	constexpr float RepairMultiplierIncrease = 1.33f;
}

UTE_RepairUpgrade::UTE_RepairUpgrade()
	: M_RepairMultiplier(RepairUpgradeTechnologySettings::RepairMultiplierIncrease)
{
	Technology = ETechnology::Tech_RepairUpgrade;
	BehaviourClassToApply = UBehaviourRepairUpgrade::StaticClass();
	SquadsToApplyTo = {
		ESquadSubtype::Squad_Ger_Scavengers,
		ESquadSubtype::Squad_Ger_SturmPionieren,
	};
}

void UTE_RepairUpgrade::ApplyOnSquad_Internal(ASquadController* Squad)
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

		URepairComponent* RepairComponent = SquadUnit->GetRepairComponent();
		if (not IsValid(RepairComponent))
		{
			continue;
		}

		RepairComponent->ApplyRepairMultiplier(M_RepairMultiplier);
	}
}
