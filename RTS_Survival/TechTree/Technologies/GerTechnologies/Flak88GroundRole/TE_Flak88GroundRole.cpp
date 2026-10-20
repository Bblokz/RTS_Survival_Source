// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "TE_Flak88GroundRole.h"

#include "RTS_Survival/Behaviours/Behaviour.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/ModeAbilityComponent/ModeAbilityComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace Flak88GroundRoleTechnologySettings
{
	// Free slot on the team weapon command card (Attack, Move, Stop, -, -, DigIn, Rotate, -, ...).
	constexpr int32 PreferredModeAbilityIndex = 7;
}

UTE_Flak88GroundRole::UTE_Flak88GroundRole()
	: PreferredModeAbilityIndex(Flak88GroundRoleTechnologySettings::PreferredModeAbilityIndex)
{
	Technology = ETechnology::Tech_Flak88_GroundRole;
	SquadsToApplyTo = {
		ESquadSubtype::Squad_Ger_88mmFlak,
	};
}

void UTE_Flak88GroundRole::ApplyOnSquad_Internal(ASquadController* Squad)
{
	if (not IsValid(Squad))
	{
		return;
	}
	if (not GroundRoleModeBehaviour)
	{
		RTSFunctionLibrary::ReportError("Tech_Flak88_GroundRole has no GroundRoleModeBehaviour set in its TEBP.");
		return;
	}
	FModeAbilitySettings ModeSettings;
	ModeSettings.ModeType = EModeAbilityType::Flak88GroundRole;
	ModeSettings.BehavioursToApply = {GroundRoleModeBehaviour};
	ModeSettings.PreferredAbilityIndex = PreferredModeAbilityIndex;

	UModeAbilityComponent* const ModeComponent = NewObject<UModeAbilityComponent>(Squad);
	// Before registering: BeginPlay adds the mode to the command card with these settings.
	ModeComponent->InitModeAbilitySettings(ModeSettings);
	ModeComponent->RegisterComponent();
	Squad->AddInstanceComponent(ModeComponent);
}
