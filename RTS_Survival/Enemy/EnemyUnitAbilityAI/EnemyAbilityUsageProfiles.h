// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

enum class EBehaviourAbilityType : uint8;
enum class EGrenadeAbilityType : uint8;

// The situation in which an enemy unit gets value out of a behaviour ability.
enum class EEnemyBehaviourUsage : uint8
{
	// Damage or fire rate buff: in combat with a target the unit's weapons are engaging.
	CombatAnyTarget,
	// Anti-tank buff: in combat with a vehicle.
	CombatVsArmor,
	// Anti-infantry buff (flak, flame, machine guns): in combat with infantry.
	CombatVsInfantry,
	// Close range buff (AdrenalineShot): the target is near.
	CombatCloseRange,
	// Makes the vehicle stationary for more range (Overwatch): only for units that are not moving or in a formation.
	CombatStationary,
	// Speed buff: in combat while moving, or chasing an attack target that is out of reach.
	Mobility,
	// Self repair: the unit is damaged.
	Repair
};

// The targets an enemy grenade is worth throwing at.
enum class EEnemyGrenadeUsage : uint8
{
	AntiInfantry,
	AntiArmor,
	// Bundle charges: vehicles, buildings and groups of soldiers.
	AnyTarget
};

namespace FEnemyAbilityUsageProfiles
{
	EEnemyBehaviourUsage GetBehaviourUsage(const EBehaviourAbilityType BehaviourAbility);
	EEnemyGrenadeUsage GetGrenadeUsage(const EGrenadeAbilityType GrenadeAbility);
}
