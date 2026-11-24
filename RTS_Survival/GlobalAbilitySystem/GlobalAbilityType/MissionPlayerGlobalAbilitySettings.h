// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityTier.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"

#include "MissionPlayerGlobalAbilitySettings.generated.h"

/**
 * @brief Per-mission rules for the player's commander kit, set on the map's mission manager. Enemy ability managers
 * ignore them. Missions without a mission manager (skirmish, survival) use the defaults: the whole kit, with costs.
 */
USTRUCT(BlueprintType)
struct FMissionPlayerGlobalAbilitySettings
{
	GENERATED_BODY()

	// Highest tier of the commander's kit the player may use here. Abilities above it are not loaded at all, so the
	// side panel does not show them. Set per mission, independent of the campaign's EMissionTier.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EGlobalAbilityTier MaxAllowedTier = EGlobalAbilityTier::TierIV;

	// The player's global abilities cost no resources in this mission; cooldowns still apply.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bDisableResourceCosts = false;
};

/**
 * @brief A global ability a mission hands the player at a scripted moment (UGlobalAbilitiesManager::
 * GrantMissionGlobalAbility). It is added to the side panel even when the mission hid the commander's kit; with
 * charges it leaves the panel again once they are used up.
 */
USTRUCT(BlueprintType)
struct FMissionGrantedGlobalAbility
{
	GENERATED_BODY()

	// GA_None grants nothing.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EGlobalAbility Ability = EGlobalAbility::GA_None;

	// Uses the player gets; 0 = unlimited.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	int32 Charges = 0;

	// Seconds before the ability can be used again; 0 keeps the ability's own cooldown.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	int32 CooldownSeconds = 0;

	// No resource cost, whatever the ability's template costs.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bFreeOfCost = true;

	// Strafing abilities only: aircraft per strike; 0 keeps the ability's own count.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	int32 StrafeAircraftCount = 0;

	// Strafing abilities only: seconds each aircraft keeps attacking; 0 keeps the ability's own time.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float StrafeSeconds = 0.f;

	// Strafing abilities only: every attack run goes over the targeted area instead of hunting enemies anywhere.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bStrafeStaysOnTarget = true;
};
