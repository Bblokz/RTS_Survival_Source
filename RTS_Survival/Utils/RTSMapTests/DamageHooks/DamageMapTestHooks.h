// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

/** One loss of health as the health component applied it, with the player whose weapon caused it when known. */
struct FMapTestDamageReport
{
	// Owning player of the weapon or effect that dealt the damage; INDEX_NONE when no damage source claimed it
	// (cook-off, engine fire, radiation).
	int32 AttackingPlayer = INDEX_NONE;
	// Actor whose weapon fired (tank, turret child actor, infantry weapon); null when the source did not name one.
	TWeakObjectPtr<AActor> AttackingActor;
	// Actor that owns the damaged health component; a soldier for squads.
	TWeakObjectPtr<AActor> Victim;
	// Owner of the victim (parent actors are asked too), INDEX_NONE for unowned things.
	int32 VictimOwner = INDEX_NONE;
	// Health really taken off, after reductions and clamped to what was left.
	float AppliedDamage = 0.f;
	bool bKilled = false;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnMapTestDamageApplied, const FMapTestDamageReport&);

/**
 * @brief Lets map tests such as the four-player skirmish see every loss of health together with the player that
 * caused it. Damage sources name their owner with RTS_MAPTEST_DAMAGE_ATTACKER_SCOPE around the call that applies
 * the damage; UHealthComponent::TakeDamage reports. Game thread only; compiled only with RTS_WITH_MAP_TESTS.
 */
namespace FDamageMapTestHooks
{
	RTS_SURVIVAL_API FOnMapTestDamageApplied& OnDamageApplied();

	/** @return Whether any map test listens; the health component skips all report work when none does. */
	RTS_SURVIVAL_API bool GetHasListeners();

	/** @return The player named by the innermost attacker scope, or INDEX_NONE outside every scope. */
	RTS_SURVIVAL_API int32 GetCurrentAttacker();

	/**
	 * @brief Tells the listeners that health was taken off an actor; called by the health component only.
	 * @param Victim Actor that owns the health component.
	 * @param AppliedDamage Health really lost.
	 * @param bKilled Whether this damage brought the health to zero.
	 */
	RTS_SURVIVAL_API void ReportDamageApplied(AActor* Victim, float AppliedDamage, bool bKilled);

	/** @return The actor named by the innermost attacker scope, or null. */
	RTS_SURVIVAL_API AActor* GetCurrentAttackerActor();

	/** @return The actor that carries the weapon of this weapon state (its mesh's owner), or null. */
	RTS_SURVIVAL_API AActor* GetFiringActorOfWeaponState(const UObject* WeaponState);

	/**
	 * @brief Names the attacking player (and optionally the firing actor) for all damage applied while it is alive;
	 * scopes nest and restore the outer one.
	 */
	class RTS_SURVIVAL_API FAttackerScope
	{
	public:
		explicit FAttackerScope(int32 AttackingPlayer, AActor* AttackingActor = nullptr);
		~FAttackerScope();

		FAttackerScope(const FAttackerScope&) = delete;
		FAttackerScope& operator=(const FAttackerScope&) = delete;

	private:
		int32 M_OuterAttacker;
		TWeakObjectPtr<AActor> M_OuterAttackerActor;
	};
}

// Put this on the line before a weapon, projectile, bomb, mine or area effect applies its damage.
#define RTS_MAPTEST_DAMAGE_ATTACKER_SCOPE(AttackingPlayer) \
	const FDamageMapTestHooks::FAttackerScope RTSMapTestDamageAttackerScope(AttackingPlayer)

// The same, also naming the weapon state that fired so map tests can credit the exact unit.
#define RTS_MAPTEST_DAMAGE_ATTACKER_WEAPON_SCOPE(AttackingPlayer, WeaponState) \
	const FDamageMapTestHooks::FAttackerScope RTSMapTestDamageAttackerScope( \
		AttackingPlayer, FDamageMapTestHooks::GetFiringActorOfWeaponState(WeaponState))

#else

#define RTS_MAPTEST_DAMAGE_ATTACKER_SCOPE(AttackingPlayer)
#define RTS_MAPTEST_DAMAGE_ATTACKER_WEAPON_SCOPE(AttackingPlayer, WeaponState)

#endif
