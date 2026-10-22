#pragma once

#include "CoreMinimal.h"

#include "BehaviourAbilityTypes.generated.h"

UENUM(BlueprintType)
enum class EBehaviourAbilityType : uint8
{
	// This will use the default icon and description from the Action UI data table.
	// Which is the implementation for the sprint ability.
	DefaultSprint,
	WeakSpotAim,
	VultureRapidFire,
	EnhancedShells,
	BrummbarHugeShells,
	VultureSnipeDamage,
	// Pz II , jaguar, sdkfz 140.
	RapidFireAutoCannon,
	// Stug
	OverwatchFireSupport,
	// PZ 3 AA.
	LongAmmoBelt,
	// Used on pz 38t.
	AdrenalineShot,
	FlakShrapnelRounds,
	FlameNapalmAbility,
	PulseRepair,
	SuperCharge,
	HighVoltage,
	SuppressiveShockline,
	OverpressureRounds,
	EngineOverclocking,
	ArmorPiercingBullets,
	APCR_Rounds,
	// Soviet PPSh assault squads: faster fire and reloads at close range. The values are card keys in
	// Buttons_DataTableb (IdApplyBehaviour), so new types are only appended.
	RusCloseAssault,
	// Soviet mortar teams: rapid barrage, the mortar fires and reloads faster for a short time.
	RusMortarBarrage,
	// Panzerjager I Ambush Shield: short burst of extra mag size, faster shots and faster reloads.
	PzJagerAmbush,
	// Panzerjager I Mobile Battery: faster rate of fire while the vehicle stays immobile.
	PzJagerBarrage,
	// Marder Dual Barrel: both barrels load HEAT rounds and reload faster for a short time.
	HEATOverload,
	// Unit variant batch 2.
	// Brummbar Direct Siege: the crew rams shells in faster for a short time (faster reload).
	HastyReload,
	// E25 Hunter: its own ambush volley (own card and duration; otherwise like PzJagerAmbush).
	E25HunterAmbush,
	// Tiger II Rocketeer: only the twin 35mm barrels; less accurate, but faster reload and shorter cooldown.
	TwinAutocannonOverload,
};
