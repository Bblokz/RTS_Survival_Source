// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

class ASquadUnit;
class UWorld;

/**
 * @brief Spawns any unit Blueprint for a chosen player, outside the training flow: map tests and the main menu
 * battles use it to put German and Soviet units on the field as player 1 and player 2.
 */
namespace FRTSUnitOwnerSpawning
{
	/**
	 * @brief Spawns a unit with its owner set before BeginPlay, so it registers, collides and fights as that player.
	 * Soldiers a squad spawns later keep the owner of their own Blueprint; see MatchSoldierToSquadOwner.
	 * @param World Game world.
	 * @param UnitClass Loaded class of the squad, tank, aircraft or other unit.
	 * @param Transform Spawn transform, used as given.
	 * @param OwningPlayer Player the unit belongs to.
	 * @param CollisionHandling What to do when the spawn spot is occupied; the async spawner nudges units free.
	 * @return The unit, or null when the class could not be spawned.
	 */
	RTS_SURVIVAL_API AActor* SpawnUnitForPlayer(UWorld& World, UClass* UnitClass, const FTransform& Transform,
	                                            uint8 OwningPlayer,
	                                            ESpawnActorCollisionHandlingMethod CollisionHandling =
		                                            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	/**
	 * @brief Owner to hand to the async spawner (OwningPlayerOverride) for a unit made for a player.
	 * @param OwningPlayer Player the unit is meant for.
	 * @return INDEX_NONE for the human and the first AI player, whose units keep the owner of their Blueprint as
	 * they always did; the player itself for every later AI player.
	 */
	RTS_SURVIVAL_API int32 GetOwnerOverrideForAIPlayer(int32 OwningPlayer);

	/**
	 * @brief A squad spawns its soldiers after the owner override of SpawnUnitForPlayer is restored, so a soldier can
	 * come up as its class default's player and fight its own squad: gives it, its weapons and its capsule the owner.
	 * @param Soldier Soldier of the squad.
	 * @param SquadOwner Owning player of the squad controller.
	 */
	RTS_SURVIVAL_API void MatchSoldierToSquadOwner(ASquadUnit* Soldier, uint8 SquadOwner);
}
