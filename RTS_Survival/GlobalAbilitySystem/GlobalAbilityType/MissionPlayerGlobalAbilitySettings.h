// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityTier.h"

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
