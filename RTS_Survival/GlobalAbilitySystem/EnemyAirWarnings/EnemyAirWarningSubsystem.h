// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/GlobalAbilitySystem/EnemyAirWarnings/EnemyAirWarningSettings.h"
#include "RTS_Survival/SupplyPoints/SupplyPointSubsystem.h"

#include "EnemyAirWarningSubsystem.generated.h"

class USoundBase;

/** What happened to one enemy ability: the reason a warning did or did not play (read by the map test). */
enum class EEnemyAirWarningResult : uint8
{
	Played,
	NotAnAirAbility,
	NoLines,
	SameWarningCooldown,
	TooSoonAfterOtherWarning,
	NoPortrait
};

/**
 * @brief Warns the player through the announcer portrait when the enemy sends aircraft (scout, strafing,
 * paratroopers, bombers). UGlobalAbilitiesManager calls OnEnemyAbilityExecuted for every ability a non-player
 * manager fires; lines and timing come from UEnemyAirWarningSettings.
 */
UCLASS()
class RTS_SURVIVAL_API UEnemyAirWarningSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UEnemyAirWarningSubsystem* Get(const UObject* WorldContextObject);

	/** @return Played when a warning portrait line was queued, otherwise why it stayed silent. */
	EEnemyAirWarningResult OnEnemyAbilityExecuted(EGlobalAbility Ability);

	/** The line queued by the last Played result (the map test checks it). */
	USoundBase* GetLastPlayedVoiceLine() const { return M_LastPlayedVoiceLine.Get(); }

private:
	// One shuffled deck per warning: every line plays once before any repeats.
	TMap<EEnemyAirWarning, FSupplyAnnouncementDeck> M_WarningDecks;

	// World seconds each warning last played; a warning not in the map never played.
	TMap<EEnemyAirWarning, double> M_LastPlayedSecondsPerWarning;

	double M_LastAnyWarningSeconds = -1.0;

	UPROPERTY()
	TWeakObjectPtr<USoundBase> M_LastPlayedVoiceLine;

	EEnemyAirWarningResult GetBlockingCooldown(EEnemyAirWarning Warning, double NowSeconds) const;
	EEnemyAirWarningResult PlayWarningLine(EEnemyAirWarning Warning, double NowSeconds);
};
