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
	case EBehaviourAbilityType::PzJagerAmbush:
	case EBehaviourAbilityType::HEATOverload:
	case EBehaviourAbilityType::E25HunterAmbush:
		return EEnemyBehaviourUsage::CombatVsArmor;
	case EBehaviourAbilityType::FlakShrapnelRounds:
	case EBehaviourAbilityType::FlameNapalmAbility:
	case EBehaviourAbilityType::SuppressiveShockline:
		return EEnemyBehaviourUsage::CombatVsInfantry;
	case EBehaviourAbilityType::AdrenalineShot:
	case EBehaviourAbilityType::RusCloseAssault:
		return EEnemyBehaviourUsage::CombatCloseRange;
	case EBehaviourAbilityType::OverwatchFireSupport:
	case EBehaviourAbilityType::PzJagerBarrage:
		return EEnemyBehaviourUsage::CombatStationary;
	case EBehaviourAbilityType::PulseRepair:
		return EEnemyBehaviourUsage::Repair;
	// Reload / rate-of-fire buffs that help against any target.
	case EBehaviourAbilityType::HastyReload:
	case EBehaviourAbilityType::TwinAutocannonOverload:
		return EEnemyBehaviourUsage::CombatAnyTarget;
	default:
		return EEnemyBehaviourUsage::CombatAnyTarget;
	}
}

EEnemyGrenadeUsage FEnemyAbilityUsageProfiles::GetGrenadeUsage(const EGrenadeAbilityType GrenadeAbility)
{
	switch (GrenadeAbility)
	{
	case EGrenadeAbilityType::GerAtGrenade:
	case EGrenadeAbilityType::RusRPG43:
		return EEnemyGrenadeUsage::AntiArmor;
	case EGrenadeAbilityType::DefaultGerBundleGrenade:
	case EGrenadeAbilityType::BundleSturmKommando:
		return EEnemyGrenadeUsage::AnyTarget;
	default:
		return EEnemyGrenadeUsage::AntiInfantry;
	}
}
