// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyAbilityUsageProfiles.h"

#include "RTS_Survival/RTSComponents/AbilityComponents/ApplyBehaviourAbilityComponent/BehaviourAbilityTypes/BehaviourAbilityTypes.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/GrenadeComponent/GrenadeAbilityTypes/GrenadeAbilityTypes.h"

EEnemyBehaviourUsage FEnemyAbilityUsageProfiles::GetBehaviourUsage(const EBehaviourAbilityType BehaviourAbility)
{
	// New behaviour types fall through to CombatAnyTarget; add them here when they need another situation.
	switch (BehaviourAbility)
	{
	case EBehaviourAbilityType::DefaultSprint:
	case EBehaviourAbilityType::EngineOverclocking:
		return EEnemyBehaviourUsage::Mobility;
	case EBehaviourAbilityType::WeakSpotAim:
	case EBehaviourAbilityType::SuperCharge:
	case EBehaviourAbilityType::HighVoltage:
	case EBehaviourAbilityType::ArmorPiercingBullets:
	case EBehaviourAbilityType::APCR_Rounds:
		return EEnemyBehaviourUsage::CombatVsArmor;
	case EBehaviourAbilityType::FlakShrapnelRounds:
	case EBehaviourAbilityType::FlameNapalmAbility:
	case EBehaviourAbilityType::SuppressiveShockline:
		return EEnemyBehaviourUsage::CombatVsInfantry;
	case EBehaviourAbilityType::AdrenalineShot:
		return EEnemyBehaviourUsage::CombatCloseRange;
	case EBehaviourAbilityType::OverwatchFireSupport:
		return EEnemyBehaviourUsage::CombatStationary;
	case EBehaviourAbilityType::PulseRepair:
		return EEnemyBehaviourUsage::Repair;
	default:
		return EEnemyBehaviourUsage::CombatAnyTarget;
	}
}

EEnemyGrenadeUsage FEnemyAbilityUsageProfiles::GetGrenadeUsage(const EGrenadeAbilityType GrenadeAbility)
{
	switch (GrenadeAbility)
	{
	case EGrenadeAbilityType::GerAtGrenade:
		return EEnemyGrenadeUsage::AntiArmor;
	case EGrenadeAbilityType::DefaultGerBundleGrenade:
	case EGrenadeAbilityType::BundleSturmKommando:
		return EEnemyGrenadeUsage::AnyTarget;
	default:
		return EEnemyGrenadeUsage::AntiInfantry;
	}
}
