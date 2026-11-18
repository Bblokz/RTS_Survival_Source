// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;

namespace RTSPlayers
{
	// Owner of things that belong to nobody (supply points before capture): never registered as a unit, hostile to
	// nobody and allied to nobody.
	inline constexpr int32 NeutralOwner = 0;
	// The one human player; every other player is AI.
	inline constexpr int32 HumanPlayer = 1;
	// First AI player. An unset owner (222) keeps behaving as this player, as it always did.
	inline constexpr int32 FirstAIPlayer = 2;
	inline constexpr int32 MaxPlayers = 4;
	// Arrays indexed by player number use this size; slot 0 is the neutral owner.
	inline constexpr int32 PlayerArraySize = MaxPlayers + 1;

	inline constexpr uint8 HumanPlayerBit = 1 << (HumanPlayer - 1);
	inline constexpr uint8 AllPlayersMask = (1 << MaxPlayers) - 1;
	inline constexpr uint8 AIPlayersMask = AllPlayersMask & ~HumanPlayerBit;
	// Players every world starts with: the human and the first AI player, as in the two-player game.
	inline constexpr uint8 DefaultActivePlayersMask = HumanPlayerBit | (1 << (FirstAIPlayer - 1));
}

/**
 * @brief Answers who a player is and who it fights, for up to RTSPlayers::MaxPlayers players (1 human, the rest AI).
 * Relations come from teams: players on one team are allied, everyone else is hostile. The default is free for all.
 * Everything that reads teams or relations is game thread only; give other threads a copy of the masks they need
 * (see FGetAsyncTarget). IsValidPlayer, IsValidAIPlayer, IsNeutralOwner, IsHumanPlayer, IsAIPlayer, NormalizePlayer,
 * GetPlayerBit and IsPlayerInMask read no state and are safe on any thread.
 */
class RTS_SURVIVAL_API FRTSPlayers
{
public:
	// ----------------- Who is this player -----------------

	static FORCEINLINE bool IsValidPlayer(const int32 Player)
	{
		return Player >= RTSPlayers::HumanPlayer && Player <= RTSPlayers::MaxPlayers;
	}

	/** An AI player that can have its own controller: FirstAIPlayer..MaxPlayers, unlike IsAIPlayer no odd owners. */
	static FORCEINLINE bool IsValidAIPlayer(const int32 Player)
	{
		return Player >= RTSPlayers::FirstAIPlayer && Player <= RTSPlayers::MaxPlayers;
	}

	static FORCEINLINE bool IsNeutralOwner(const int32 Player)
	{
		return Player == RTSPlayers::NeutralOwner;
	}

	static FORCEINLINE bool IsHumanPlayer(const int32 Player)
	{
		return Player == RTSPlayers::HumanPlayer;
	}

	/**
	 * Everything that is not the human, neutral and unset owners included: the side that used to be "the enemy".
	 * Use it for "which side of the collision channels / which stat table"; use AreHostile for "do they fight".
	 */
	static FORCEINLINE bool IsAIPlayer(const int32 Player)
	{
		return Player != RTSPlayers::HumanPlayer;
	}

	/**
	 * @return The slot to store and compare this owner with: players 1..MaxPlayers and the neutral owner map to
	 * themselves, any other value (unset 222) to the first AI player.
	 */
	static FORCEINLINE int32 NormalizePlayer(const int32 Player)
	{
		return (Player >= RTSPlayers::NeutralOwner && Player <= RTSPlayers::MaxPlayers)
			       ? Player
			       : RTSPlayers::FirstAIPlayer;
	}

	/**
	 * @return The one bit of this player in every player mask (hostile, friendly, visible-to, collision filter);
	 * zero for the neutral owner, which is in no mask.
	 */
	static FORCEINLINE uint8 GetPlayerBit(const int32 Player)
	{
		const int32 Slot = NormalizePlayer(Player);
		return Slot == RTSPlayers::NeutralOwner ? 0 : static_cast<uint8>(1 << (Slot - 1));
	}

	/**
	 * @brief Exact test of a player against a mask copied to another thread: no normalizing of unset owners.
	 * @return False for every owner outside 1..MaxPlayers, so neutral and unset owners match no mask.
	 */
	static FORCEINLINE bool IsPlayerInMask(const int32 Player, const uint8 PlayerMask)
	{
		return IsValidPlayer(Player) && (PlayerMask & static_cast<uint8>(1 << (Player - 1))) != 0;
	}

	// ----------------- Who fights whom -----------------

	/** @return Bits of all players this player fights; none for the neutral owner. */
	static FORCEINLINE uint8 GetHostileMask(const int32 Player)
	{
		return M_HostileMaskByPlayer[NormalizePlayer(Player)];
	}

	/**
	 * @return Bits of the player itself and its allies; weapon queries ignore bodies carrying one of these bits.
	 * Zero for the neutral owner: it has no allies, and a full mask would let its shots pass through every unit.
	 */
	static FORCEINLINE uint8 GetFriendlyMask(const int32 Player)
	{
		const int32 Slot = NormalizePlayer(Player);
		if (IsNeutralOwner(Slot))
		{
			return 0;
		}
		return static_cast<uint8>(~M_HostileMaskByPlayer[Slot] & RTSPlayers::AllPlayersMask);
	}

	/** False when either side is the neutral owner. */
	static FORCEINLINE bool AreHostile(const int32 PlayerA, const int32 PlayerB)
	{
		return (GetHostileMask(PlayerA) & GetPlayerBit(PlayerB)) != 0;
	}

	/** True for the same player and for players on one team; false when either side is the neutral owner. */
	static FORCEINLINE bool AreAllied(const int32 PlayerA, const int32 PlayerB)
	{
		return not IsNeutralOwner(NormalizePlayer(PlayerA)) && not IsNeutralOwner(NormalizePlayer(PlayerB))
			&& not AreHostile(PlayerA, PlayerB);
	}

	/**
	 * @brief Whether damage of a player's weapon or area effect applies to something with this owner.
	 * @return True for hostile owners. Neutral things can only be damaged by the human, as before four players:
	 * they sit on the AI side of the collision channels, where only the human's weapons reach them.
	 */
	static FORCEINLINE bool CanPlayerDamageOwner(const int32 AttackingPlayer, const int32 TargetOwner)
	{
		return AreHostile(AttackingPlayer, TargetOwner)
			|| (IsNeutralOwner(TargetOwner) && IsHumanPlayer(AttackingPlayer));
	}

	static FORCEINLINE bool IsHostileToHuman(const int32 Player)
	{
		return (GetHostileMask(Player) & RTSPlayers::HumanPlayerBit) != 0;
	}

	/** @return Whether at least one AI player is hostile to this player. */
	static FORCEINLINE bool HasHostileAIPlayer(const int32 Player)
	{
		return (GetHostileMask(Player) & RTSPlayers::AIPlayersMask) != 0;
	}

	/**
	 * @return A number that changes whenever teams change or a unit changes owner. Whoever holds on to a target
	 * (weapons) stores it and re-checks hostility only when it differs, instead of on every tick.
	 */
	static FORCEINLINE uint32 GetRelationsVersion()
	{
		return M_RelationsVersion;
	}

	/** Called by URTSComponent after a unit changed owner at runtime, so held targets get re-checked. */
	static void NotifyUnitOwnerChanged();

	// ----------------- Who takes part -----------------

	/**
	 * @return Bits of the players that take part in this world: the human, the first AI player and every player that
	 * has had a unit. Per-shot area queries use it to leave out channels no hostile unit can be on.
	 */
	static FORCEINLINE uint8 GetActivePlayersMask()
	{
		return M_ActivePlayersMask;
	}

	/** @return Bits of the players this player fights that take part in this world. */
	static FORCEINLINE uint8 GetActiveHostileMask(const int32 Player)
	{
		return GetHostileMask(Player) & M_ActivePlayersMask;
	}

	/** Called by the unit manager when a unit of the player registers; a player stays active for the whole world. */
	static void MarkPlayerActive(int32 Player);

	// ----------------- Teams -----------------

	static int32 GetPlayerTeam(int32 Player);

	/**
	 * @brief Puts a player on a team; players sharing a team stop fighting each other from the next shot on.
	 * @param Player Player to move, 1..MaxPlayers.
	 * @param Team Any team number; the default is the player's own number (free for all).
	 */
	static void SetPlayerTeam(int32 Player, int32 Team);

	/** Every player on its own team: the state each world starts in. */
	static void ResetToFreeForAll();

	/** Free for all and only the default players active: called once for every new game world. */
	static void ResetForNewWorld();

	// ----------------- Actors -----------------

	/**
	 * @brief Finds who owns an actor; turrets and infantry weapons are child actors without an RTS component of
	 * their own, so the parent actors are asked too.
	 * @return Owning player of the first RTS component found on the actor or its parent actors (can be the neutral
	 * owner), or INDEX_NONE when there is none.
	 */
	static int32 GetOwningPlayerOfActor(const AActor* Actor);

	/** @return True when the actor is owned by a player hostile to Player; unowned and neutral actors never are. */
	static bool IsActorHostileToPlayer(const AActor* Actor, int32 Player);

	/** @return True when the actor is owned by Player or one of its allies; unowned and neutral actors never are. */
	static bool IsActorAlliedToPlayer(const AActor* Actor, int32 Player);

private:
	static void RebuildHostileMasks();

	static uint8 M_TeamByPlayer[RTSPlayers::PlayerArraySize];
	// Cached per player so the hot paths (every trace, every target request) are one array read.
	static uint8 M_HostileMaskByPlayer[RTSPlayers::PlayerArraySize];
	static uint8 M_ActivePlayersMask;
	static uint32 M_RelationsVersion;
};
