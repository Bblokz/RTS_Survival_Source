// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "TE_EntrenchingTools.h"

#include "RTS_Survival/RTSComponents/AbilityComponents/FieldConstructionAbilityComponent/FieldConstructionAbilityComponent.h"
#include "RTS_Survival/Units/SquadController.h"

namespace EntrenchingToolsTechnologySettings
{
	constexpr float ConstructionSpeedMultiplier = 1.35f;
}

UTE_EntrenchingTools::UTE_EntrenchingTools()
	: M_ConstructionSpeedMultiplier(EntrenchingToolsTechnologySettings::ConstructionSpeedMultiplier)
{
	Technology = ETechnology::Tech_Pioneer_Entrenching;
	SquadsToApplyTo = {
		ESquadSubtype::Squad_Ger_SturmPionieren,
		ESquadSubtype::Squad_Ger_Scavengers,
		// The medic scavenger squad builds field defences too.
		ESquadSubtype::Squad_Ger_Medic,
	};
}

void UTE_EntrenchingTools::ApplyOnSquad_Internal(ASquadController* Squad)
{
	if (not IsValid(Squad))
	{
		return;
	}
	TArray<UFieldConstructionAbilityComponent*> ConstructionComponents;
	Squad->GetComponents(ConstructionComponents);
	for (UFieldConstructionAbilityComponent* const ConstructionComponent : ConstructionComponents)
	{
		ConstructionComponent->ApplyConstructionSpeedMultiplier(M_ConstructionSpeedMultiplier);
	}
}
