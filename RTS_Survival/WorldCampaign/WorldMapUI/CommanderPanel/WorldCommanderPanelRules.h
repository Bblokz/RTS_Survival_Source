// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

// Kept free of widget headers so the tests can include it on its own.
#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/CommanderMapKit/WorldCommanderMapKitTypes.h"

/**
 * @brief What the round ability button of the commander panel shows.
 */
enum class EWorldCommanderAbilityButtonState : uint8
{
	// No commander kit or no map ability: dimmed.
	Unavailable,
	// Lit ring and glyph.
	Ready,
	// Grey with the turns left on it.
	CoolingDown,
	// Waiting for the player to click a target on the map.
	PickingTarget
};

/**
 * @brief What the player clicked while picking a target, sorted by the panel from the actor under the cursor.
 */
enum class EWorldCommanderPickedThing : uint8
{
	// Nothing under the cursor.
	Nothing,
	// A mission or enemy item that is not cleared and not hidden in the fog.
	OperationTarget,
	// Any other map object: the player's own, a neutral one, or a cleared target.
	OtherMapObject,
	Division,
	BuildNode,
	// The ground, or an object the fog hides.
	Ground
};

/**
 * @brief The click the panel turns into a map ability target; only the fields that fit Thing are set.
 */
struct RTS_SURVIVAL_API FWorldCommanderPickCandidate
{
	EWorldCommanderPickedThing Thing = EWorldCommanderPickedThing::Nothing;
	FGuid AnchorKey;
	FGuid DivisionKey;
	FName BuildNodeId = NAME_None;
	FVector WorldLocation = FVector::ZeroVector;
	bool bHasWorldLocation = false;
};

/**
 * @brief Texts and decisions of the commander panel as pure functions, so the widget and the tests share them.
 */
namespace WorldCommanderPanelRules
{
	/**
	 * @brief Missing ability wins over the cooldown, the cooldown over picking: a used ability ends any picking.
	 * @param bHasAbility Whether the campaign commander has a map ability.
	 * @param CooldownTurnsRemaining Turns until the ability is ready again; 0 = ready.
	 * @param bIsPickingTarget Whether the panel waits for a target click.
	 * @return The look of the ability button.
	 */
	RTS_SURVIVAL_API EWorldCommanderAbilityButtonState GetAbilityButtonState(bool bHasAbility,
	                                                                        int32 CooldownTurnsRemaining,
	                                                                        bool bIsPickingTarget);

	/** The number of turns left while cooling down; empty otherwise (the glyph is drawn instead). */
	RTS_SURVIVAL_API FText GetAbilityButtonLabel(EWorldCommanderAbilityButtonState State,
	                                             int32 CooldownTurnsRemaining);

	/**
	 * @brief The state line under the ability description on the hover plate.
	 * @param State Look of the ability button.
	 * @param CooldownTurnsRemaining Turns until the ability is ready again.
	 * @param AbilityCooldownTurns The cooldown a use starts.
	 * @return "READY · cooldown N turns", "READY", "READY IN N TURNS", or empty without an ability.
	 */
	RTS_SURVIVAL_API FText GetAbilityStateLine(EWorldCommanderAbilityButtonState State,
	                                           int32 CooldownTurnsRemaining, int32 AbilityCooldownTurns);

	/** What the player has to click for a target kind ("an enemy target"); empty for None. */
	RTS_SURVIVAL_API FText GetTargetKindNoun(EWorldCommanderAbilityTargetKind Kind);

	/** "Pick a target: <noun>"; empty for None. */
	RTS_SURVIVAL_API FText GetTargetPromptText(EWorldCommanderAbilityTargetKind Kind);

	/**
	 * @brief Turns a click into the target the ability reads; a wrong kind of click keeps the panel picking.
	 * @param Kind The target kind the ability needs.
	 * @param Candidate What was clicked.
	 * @param OutTarget Receives the target; only the field Kind reads is set.
	 * @param OutFailReason Player-facing reason when the click does not fit Kind.
	 * @return false when the click is not a target of Kind.
	 */
	RTS_SURVIVAL_API bool BuildTargetFromPick(EWorldCommanderAbilityTargetKind Kind,
	                                          const FWorldCommanderPickCandidate& Candidate,
	                                          FWorldCommanderMapAbilityTarget& OutTarget, FText& OutFailReason);
}
