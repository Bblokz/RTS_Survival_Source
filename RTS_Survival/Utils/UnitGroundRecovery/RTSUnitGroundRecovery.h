// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * @brief Keeps units out of and above the landscape without any periodic check.
 * Two event-driven entry points:
 * - At BeginPlay a unit that starts inside the landscape (spawn point set under the terrain, a squad grid point under a
 *   hill) is lifted onto the surface: one line trace per spawned unit.
 * - When a unit still falls through the world, the engine's own KillZ check (run inside character movement and the
 *   physics sync, see AActor::CheckStillInWorld) calls FellOutOfWorld; the unit overrides use TryPlaceBackOnGround.
 *   URTSKillZSubsystem sets a KillZ just under each map's landscape so that happens within a few seconds.
 */
struct RTS_SURVIVAL_API FRTSUnitGroundRecovery
{
	/**
	 * @brief Lifts a unit whose origin lies under the landscape surface onto it. Units above the surface (aircraft,
	 * tanks hanging under a transport, soldiers on a bridge) and units over a landscape hole (trenches) are left alone.
	 * @param Unit The unit, right after it was spawned.
	 * @param OriginAboveGround Height of the unit's origin above the ground it stands on (capsule half height for a
	 * soldier; a small drop for a physics vehicle, which settles).
	 * @param MinDepthCm How far the origin must be under the surface before the unit is moved: a vehicle whose origin
	 * is at its tracks normally sits a few centimetres into the ground.
	 * @return True when the unit was moved.
	 */
	static bool TryLiftOutOfLandscape(AActor& Unit, float OriginAboveGround, float MinDepthCm);

	/**
	 * @brief Puts a unit that fell out of the world back where it can stand: on the landscape above it, or, when there
	 * is no landscape at its X/Y (it went over the map border), on the nearest navmesh.
	 * @param Unit The fallen unit.
	 * @param OriginAboveGround Height of the unit's origin above the ground it stands on.
	 * @return False when there is no ground anywhere near; the caller then lets the unit die.
	 */
	static bool TryPlaceBackOnGround(AActor& Unit, float OriginAboveGround);
};
