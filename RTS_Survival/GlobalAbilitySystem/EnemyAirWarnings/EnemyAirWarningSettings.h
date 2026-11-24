// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTS_Survival/GameUI/Portrait/RTSPortraitTypes.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"

#include "EnemyAirWarningSettings.generated.h"

class USoundBase;

/** The kinds of enemy air activity the announcer warns the player about. */
UENUM(BlueprintType)
enum class EEnemyAirWarning : uint8
{
	None,
	ScoutAircraft,
	AttackAircraft,
	ParatrooperPlane,
	Bombers
};

/** Which enemy abilities count as one kind of air activity, and the portrait lines that announce it. */
USTRUCT(BlueprintType)
struct FEnemyAirWarningLines
{
	GENERATED_BODY()

	// Global abilities that trigger this warning when the enemy fires them.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Enemy Air Warnings")
	TArray<EGlobalAbility> Abilities;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Enemy Air Warnings")
	ERTSPortraitTypes PortraitType = ERTSPortraitTypes::GermanAnnouncer;

	// Sound waves (not cues): only waves get word-timed subtitles under the portrait. Empty keeps this warning silent.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Enemy Air Warnings")
	TArray<TSoftObjectPtr<USoundBase>> VoiceLines;
};

/**
 * @brief Project settings for the announcer portrait lines played when the enemy sends aircraft at the player.
 * Read by UEnemyAirWarningSubsystem; edit under Project Settings > RTS Enemy Air Warnings.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "RTS Enemy Air Warnings"))
class RTS_SURVIVAL_API UEnemyAirWarningSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UEnemyAirWarningSettings();

	/** @return The lines of this warning; the scout lines for None (never asked for in practice). */
	const FEnemyAirWarningLines& GetLines(EEnemyAirWarning Warning) const;

	/** @return The warning the enemy firing this ability triggers, or None when it is not an air ability. */
	EEnemyAirWarning GetWarningForAbility(EGlobalAbility Ability) const;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Lines")
	FEnemyAirWarningLines ScoutAircraftLines;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Lines")
	FEnemyAirWarningLines AttackAircraftLines;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Lines")
	FEnemyAirWarningLines ParatrooperPlaneLines;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Lines")
	FEnemyAirWarningLines BomberLines;

	// The same warning stays silent this long after it played, so an enemy firing a strafing run twice in a row is
	// announced once.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0.0"))
	float SameWarningCooldownSeconds = 45.f;

	// No warning plays this soon after any other warning: the portrait queues lines, so back-to-back air abilities
	// would otherwise stack up warnings that arrive late.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0.0"))
	float MinSecondsBetweenWarnings = 12.f;
};
