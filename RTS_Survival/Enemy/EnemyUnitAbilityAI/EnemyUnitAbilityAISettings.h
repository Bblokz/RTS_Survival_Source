// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "EnemyUnitAbilityAISettings.generated.h"

/**
 * @brief Project settings (Game section) that tune when enemy squads and tanks use their command card abilities.
 * Read by UEnemyUnitAbilityAIComponent on the enemy controller; every map uses the same values.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="RTS Enemy Unit Ability AI"))
class RTS_SURVIVAL_API UEnemyUnitAbilityAISettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UEnemyUnitAbilityAISettings();

	// Turns the enemy unit ability AI off on every map.
	UPROPERTY(Config, EditAnywhere, Category="General")
	bool bEnabled = true;

	// Lowest mission difficulty at which enemy squads use their abilities.
	UPROPERTY(Config, EditAnywhere, Category="General|Difficulty")
	ERTSGameDifficulty MinDifficultySquadsUseAbilities = ERTSGameDifficulty::Normal;

	// Lowest mission difficulty at which enemy tanks use their abilities.
	UPROPERTY(Config, EditAnywhere, Category="General|Difficulty")
	ERTSGameDifficulty MinDifficultyTanksUseAbilities = ERTSGameDifficulty::Hard;

	// Seconds between two evaluation ticks.
	UPROPERTY(Config, EditAnywhere, Category="Timing", meta=(ClampMin="0.1"))
	float EvaluationInterval = 0.5f;

	// Units looked at per evaluation tick; the roster is walked round robin.
	UPROPERTY(Config, EditAnywhere, Category="Timing", meta=(ClampMin="1"))
	int32 UnitsEvaluatedPerTick = 6;

	// Seconds between two scans of the enemy units for new or dead ones.
	UPROPERTY(Config, EditAnywhere, Category="Timing", meta=(ClampMin="0.5"))
	float RosterRefreshInterval = 3.f;

	// A unit that just used an ability waits this long before it may use another one.
	UPROPERTY(Config, EditAnywhere, Category="Timing", meta=(ClampMin="0"))
	float MinSecondsBetweenAbilityUsesPerUnit = 4.f;

	// Anti-infantry grenades need at least this many player soldiers inside the blast.
	UPROPERTY(Config, EditAnywhere, Category="Grenades", meta=(ClampMin="1"))
	int32 MinSoldiersInGrenadeArea = 2;

	// Off hold position a squad may walk to a target up to this many throw ranges away before throwing.
	UPROPERTY(Config, EditAnywhere, Category="Grenades", meta=(ClampMin="1"))
	float GrenadeWalkRangeMultiplier = 1.5f;

	// A squad with fewer soldiers alive keeps shooting instead of throwing.
	UPROPERTY(Config, EditAnywhere, Category="Grenades", meta=(ClampMin="1"))
	int32 MinAliveSoldiersToThrow = 2;

	// Vehicles and buildings below this health ratio die to normal fire; no grenade is spent on them.
	UPROPERTY(Config, EditAnywhere, Category="Grenades", meta=(ClampMin="0", ClampMax="1"))
	float SkipTargetsBelowHealthRatio = 0.2f;

	// Seconds a grenade target area stays claimed so other squads do not throw at the same spot.
	UPROPERTY(Config, EditAnywhere, Category="Grenades", meta=(ClampMin="0"))
	float TargetClaimSeconds = 5.f;

	// Repair behaviours are used below this health ratio.
	UPROPERTY(Config, EditAnywhere, Category="Behaviours", meta=(ClampMin="0", ClampMax="1"))
	float RepairBelowHealthRatio = 0.6f;

	// Close range behaviours (AdrenalineShot) need the target within this fraction of the unit's weapon range.
	UPROPERTY(Config, EditAnywhere, Category="Behaviours", meta=(ClampMin="0", ClampMax="1"))
	float CloseRangeFraction = 0.4f;
};
