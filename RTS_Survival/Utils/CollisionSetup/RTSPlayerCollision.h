// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "TriggerOverlapLogic.h"

class AActor;
class UPrimitiveComponent;
struct FCollisionQueryParams;
struct FCollisionObjectQueryParams;

/**
 * @brief The one place that turns an owning player into collision channels and filters, for up to four players.
 * Channels only split human from AI. Which players a shot can hit is decided by the owner bit on the bodies of units
 * (their collision mask filter) and the ignore mask on the weapon's query: a shot skips the shooter and its allies.
 * @note Every weapon trace and sweep must take its channel AND its ignore mask from here.
 */
class RTS_SURVIVAL_API FRTSPlayerCollision
{
public:
	// ----------------- Weapon queries -----------------

	/** @return The channel a weapon of this player traces on: human weapons and AI weapons each have one. */
	static ECollisionChannel GetWeaponTraceChannel(int32 OwningPlayer);

	/** @return The mask filter bits a weapon query of this player ignores: the player itself and its allies. */
	static uint8 GetWeaponIgnoreMask(int32 OwningPlayer);

	/**
	 * @brief Makes a CHANNEL query skip everything owned by the shooter or its allies. Without it an AI shot hits the
	 * units of its own player, as every AI unit blocks the AI weapon channel.
	 * @param QueryParams Params of a trace, sweep or overlap by channel; object-type queries do not read this mask.
	 * @param OwningPlayer Player that fires.
	 */
	static void ApplyWeaponIgnoreMask(FCollisionQueryParams& QueryParams, int32 OwningPlayer);

	/**
	 * @brief The same for an OBJECT-TYPE query, which takes its mask from the object params. It also skips owned
	 * destructibles (own walls), so area damage does not use it: it filters its results with IsHitHostileToPlayer.
	 * @param ObjectQueryParams Object params of a trace, sweep or overlap by object type.
	 * @param OwningPlayer Player that fires.
	 */
	static void ApplyWeaponIgnoreMask(FCollisionObjectQueryParams& ObjectQueryParams, int32 OwningPlayer);

	// ----------------- Bodies of owned units -----------------

	/** @return Object channel of things this player owns: one for the human, one shared by all AI players. */
	static ECollisionChannel GetOwnerObjectChannel(int32 OwningPlayer);

	/**
	 * @brief Lets hostile weapons hit this component and friendly weapons pass through it: blocks the weapon
	 * channel(s) that can carry hostile shots and marks the body with the owner bit.
	 * @param Component Component weapons should be able to hit.
	 * @param OwningPlayer Owner of the component.
	 * @param Response Block by default; pass ECR_Ignore to stop weapons hitting the component.
	 */
	static void SetWeaponHitResponses(UPrimitiveComponent* Component, int32 OwningPlayer,
	                                  ECollisionResponse Response = ECR_Block);

	/**
	 * @brief For things every player's weapons hit, allied shots included (neutral objects, walls that stop
	 * friendly fire): blocks both weapon channels and clears the owner bit.
	 */
	static void SetWeaponHitResponsesForAllPlayers(UPrimitiveComponent* Component,
	                                               ECollisionResponse Response = ECR_Block);

	/** Marks the bodies of the component as owned by this player, so queries of its allies skip them. */
	static void SetOwnerMaskFilter(UPrimitiveComponent* Component, int32 OwningPlayer);

	static void ClearOwnerMaskFilter(UPrimitiveComponent* Component);

	/**
	 * @brief Marks every colliding primitive of a unit and of its child actors (turrets, weapons) as owned by this
	 * player: also the Blueprint-authored colliders and the bodies no setup function touches. Only queries that pass
	 * an ignore mask see the bit. Call it when the unit begins play and again when its owner changes.
	 * @param Actor Unit, building or other owned actor.
	 * @param OwningPlayer Owner of the actor.
	 * @note Components that every player must hit (SetWeaponHitResponsesForAllPlayers) have to be set up after it.
	 */
	static void ApplyOwnerMaskToActor(AActor* Actor, int32 OwningPlayer);

	/**
	 * @brief Moves the collision of a unit and its child actors to the other side when its owner changes between
	 * the human and an AI player at runtime: swaps the human/AI object channel and the responses to them, and gives
	 * every body the weapon responses of the new side (what it did to the old hostile weapons it now does to the new
	 * ones; AI bodies also stay hittable by other AI players). Call before ApplyOwnerMaskToActor.
	 * @param Actor Unit, building or other owned actor.
	 * @param OldOwningPlayer Owner before the change.
	 * @param NewOwningPlayer Owner after the change; nothing happens when both are on the same side.
	 * @note Bodies the setup functions own (infantry capsule, standalone turret, field construction) are set up again
	 * by their own owner-change listeners afterwards; this covers everything else (hulls, expansion meshes, walls).
	 */
	static void ApplySideChangeToActor(AActor* Actor, int32 OldOwningPlayer, int32 NewOwningPlayer);

	/**
	 * @brief Gives a component that took over the collision of another one (a wreck mesh, a gate leaf) the same
	 * owner: copying the responses with SetCollisionResponseToChannels does not copy whose body it is.
	 * @param SourceComponent Component the responses were copied from.
	 * @param TargetComponent Component that now stands in for it.
	 */
	static void CopyOwnerMaskFilter(const UPrimitiveComponent* SourceComponent, UPrimitiveComponent* TargetComponent);

	/**
	 * @brief Writes the owner bit of a component to its physics bodies again. A skeletal mesh makes new bodies when
	 * its mesh or physics asset is swapped, and those start without the bit.
	 */
	static void RefreshOwnerMaskFilter(UPrimitiveComponent* Component);

	// ----------------- Who was hit -----------------

	/**
	 * @brief Resolves the owner of something a query hit: from the owner bit on the hit body, else from the RTS
	 * component of the actor or its parent actors, else from the side of the body's object channel.
	 * @param HitActor Actor of the hit or overlap; may be a child actor (turret).
	 * @param HitComponent Component of the hit or overlap; may be null.
	 * @return The owning player, or INDEX_NONE for things nobody owns (environment, destructibles).
	 */
	static int32 GetOwningPlayerOfHit(const AActor* HitActor, const UPrimitiveComponent* HitComponent);

	/** @return True when the hit thing is owned by a player hostile to Player; unowned things are never hostile. */
	static bool IsHitHostileToPlayer(const AActor* HitActor, const UPrimitiveComponent* HitComponent, int32 Player);

	/**
	 * @brief The filter for hostile-seeking area damage and searches on the unit object channels.
	 * @return True when the hit thing has a hostile owner, or a neutral owner and Player is the human
	 * (FRTSPlayers::CanPlayerDamageOwner); in a two-player game that is everything on the other side's channel.
	 */
	static bool IsHitDamageableByPlayer(const AActor* HitActor, const UPrimitiveComponent* HitComponent, int32 Player);

	/** @return True when the hit thing is owned by Player or one of its allies; unowned things are never allied. */
	static bool IsHitAlliedToPlayer(const AActor* HitActor, const UPrimitiveComponent* HitComponent, int32 Player);

	// ----------------- Overlap triggers and area queries -----------------

	/**
	 * @return The object channels on which units hostile to this player can be right now: only players that take
	 * part in the world count, so in a two-player game this is the one channel of the other side. Allied AI units
	 * share a channel with hostile ones, so the result is still filtered with IsHitHostileToPlayer.
	 * @note For a query issued now. A trigger that lives on (mine) overlaps both sides and filters in its handler.
	 */
	static ETriggerOverlapLogic GetOverlapLogicForHostilesOf(int32 OwningPlayer);

	/** @return The object channels on which this player's own and allied units can be; results are filtered by owner. */
	static ETriggerOverlapLogic GetOverlapLogicForAlliesOf(int32 OwningPlayer);

	/** Adds the object channel(s) of ETriggerOverlapLogic to an object query. */
	static void AddObjectTypesForOverlapLogic(FCollisionObjectQueryParams& ObjectQueryParams,
	                                          ETriggerOverlapLogic OverlapLogic);

private:
	static void ApplyMaskFilter(UPrimitiveComponent* Component, uint8 MaskFilter);
	static void ApplySideChangeToComponent(UPrimitiveComponent& Component, bool bIsNewOwnerHuman);
};
