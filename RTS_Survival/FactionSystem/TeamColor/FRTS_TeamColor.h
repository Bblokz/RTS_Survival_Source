// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * @brief Gives the player's vehicles their team colour: writes it to custom primitive data 4-7 of every primitive of a
 * vehicle and its child actors (turrets), where the German vehicle materials (MF_TeamColor_CamoSwap) repaint their camo
 * bands with it. Called from ATankMaster::BeginPlay like the faction flags; the faction menu re-applies it to its preview.
 * Alpha 0, the default, keeps every camo exactly as authored. See Docs/Rendering/TeamColorShaders.md.
 */
class RTS_SURVIVAL_API FRTS_TeamColor
{
public:
	/** Writes the game instance's team colour to the actor when the player owns it and picked a colour. */
	static void ApplyPlayerTeamColorToActor(AActor* Actor);

	/**
	 * @brief Writes the current team colour to every player vehicle in the world, also when it went back to the default,
	 * so a colour picked in the faction menu shows on the hangar preview at once.
	 * @param WorldContextObject Any object in the world to update.
	 */
	static void ApplyPlayerTeamColorToWorldVehicles(const UObject* WorldContextObject);

	/** @return The team colour the player picked; alpha 0 when the vehicles keep their camo. */
	static FLinearColor GetPlayerTeamColor(const UObject* WorldContextObject);

	static constexpr int32 TeamColorPrimitiveDataIndex = 4;

private:
	static void SetTeamColorOnActorAndChildren(const AActor* TargetActor, const FLinearColor& TeamColor);
	static bool GetIsOwnedByPlayer(const AActor* Actor);
};
