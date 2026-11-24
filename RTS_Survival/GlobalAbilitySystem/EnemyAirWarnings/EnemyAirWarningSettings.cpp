// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyAirWarningSettings.h"

UEnemyAirWarningSettings::UEnemyAirWarningSettings()
{
	CategoryName = TEXT("Game");
	// The Soviet air abilities the enemy AI fires (UEnemyStrategicAIComponent buckets); config can add more.
	ScoutAircraftLines.Abilities = {EGlobalAbility::GA_SovietReconFlight};
	AttackAircraftLines.Abilities = {EGlobalAbility::GA_2xPE2Strafing, EGlobalAbility::GA_4xPE2Strafing};
	ParatrooperPlaneLines.Abilities = {EGlobalAbility::GA_SovietParatroopers};
	BomberLines.Abilities = {
		EGlobalAbility::GA_HeavyCarpet_PE8_500Gr,
		EGlobalAbility::GA_HeavyCarpet_PE8_1000Gr,
		EGlobalAbility::GA_HeavyCarpet_PE8_3000Gr
	};
}

const FEnemyAirWarningLines& UEnemyAirWarningSettings::GetLines(const EEnemyAirWarning Warning) const
{
	switch (Warning)
	{
	case EEnemyAirWarning::AttackAircraft:
		return AttackAircraftLines;
	case EEnemyAirWarning::ParatrooperPlane:
		return ParatrooperPlaneLines;
	case EEnemyAirWarning::Bombers:
		return BomberLines;
	case EEnemyAirWarning::ScoutAircraft:
	case EEnemyAirWarning::None:
	default:
		return ScoutAircraftLines;
	}
}

EEnemyAirWarning UEnemyAirWarningSettings::GetWarningForAbility(const EGlobalAbility Ability) const
{
	constexpr EEnemyAirWarning Warnings[] = {
		EEnemyAirWarning::ScoutAircraft,
		EEnemyAirWarning::AttackAircraft,
		EEnemyAirWarning::ParatrooperPlane,
		EEnemyAirWarning::Bombers
	};
	for (const EEnemyAirWarning Warning : Warnings)
	{
		if (GetLines(Warning).Abilities.Contains(Ability))
		{
			return Warning;
		}
	}
	return EEnemyAirWarning::None;
}
