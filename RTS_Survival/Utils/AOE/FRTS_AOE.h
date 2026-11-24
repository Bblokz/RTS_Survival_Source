// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DamageEvents.h"

#include "RTS_Survival/Utils/CollisionSetup/TriggerOverlapLogic.h"
#include "RTS_Survival/RTSComponents/ShieldComponent/ShieldTypes.h"
#include "RTS_Survival/Weapons/WeaponData/RTSDamageTypes/RTSDamageTypes.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/VehicleModules/VehicleModuleTypes.h"

class UBehaviour;
class UBehaviourComp;
class UArmorCalculation;
class UPrimitiveComponent;

// How an AOE measures a victim's distance from the epicenter for the damage falloff.
enum class EAoeFalloffDistance : uint8
{
	// Distance to the actor origin; a vehicle's hull can be well inside the radius while its origin is not.
	ActorOrigin,
	// Distance to the nearest point of the actor's hit collision, and one damage application per actor.
	ClosestCollision
};

// What a falloff distance was measured to; lets tests see when the collision query had to fall back.
enum class EAoeDistanceSource : uint8
{
	ActorOrigin,
	Collision,
	// The component has no simple collision (complex-only meshes); its world bounds box is used instead.
	BoundsBox
};

// Whose units an area effect of a player reaches, seen from that player.
enum class EAoeTargets : uint8
{
	Hostiles,
	Allies,
	// Every unit in range, whoever owns it.
	HostilesAndAllies
};

/**
 * @brief Who causes an area effect and whom it reaches: use it for everything a player's unit or ability causes.
 * Hostile and allied AI players share one object channel, so such an effect checks the owner of every unit it finds.
 * The ETriggerOverlapLogic entry points stay for authored and mission callers, where "Enemy" means any AI player.
 */
struct RTS_SURVIVAL_API FAoeInstigator
{
	int32 OwningPlayer = 0;
	EAoeTargets Targets = EAoeTargets::Hostiles;

	static FAoeInstigator HostilesOf(int32 InOwningPlayer);
	static FAoeInstigator AlliesOf(int32 InOwningPlayer);

	/**
	 * @brief Reads an authored Player / Enemy / Both setting from the owner's point of view.
	 * @param InOwningPlayer Player whose unit or ability causes the effect.
	 * @param OwnerRelativeLogic OverlapPlayer = the owner and its allies, OverlapEnemy = its hostiles.
	 */
	static FAoeInstigator FromOwnerRelativeLogic(int32 InOwningPlayer, ETriggerOverlapLogic OwnerRelativeLogic);
};

/**
 * @brief Utility struct providing async area-of-effect helpers for damage and behaviours.
 */
struct RTS_SURVIVAL_API FRTS_AOE
{
	FRTS_AOE();
	~FRTS_AOE();

	/**
	 * @brief Owner-relative version of ApplyToActorsAsync: only units with the wanted relation to the instigator
	 * reach the callback; destructibles reach it as before.
	 * @param InstigatorActor Actor used for world access and self-ignoring.
	 * @param Epicenter Center of the area search.
	 * @param Radius Radius of the sphere sweep.
	 * @param Instigator Player that causes the effect and whose units it reaches.
	 * @param OnActorFound Callback invoked on the game thread for each hit actor.
	 */
	static void ApplyToActorsAsync(
		AActor* InstigatorActor,
		const FVector& Epicenter,
		float Radius,
		const FAoeInstigator& Instigator,
		TFunction<void(AActor&)> OnActorFound
	);

	/**
	 * @brief Owner-relative version of DealDamageInRadiusAsync: damages the units hostile to the instigator (or the
	 * relation it asks for) and every destructible in range.
	 * @param DamageCauser Actor credited as damage causer.
	 * @param Epicenter Center of the area search.
	 * @param Radius Radius of the damage sphere.
	 * @param BaseDamage Damage applied at the epicenter before falloff.
	 * @param DamageFalloffExponent Exponent controlling how aggressively damage decays towards the edge.
	 * @param DamageType Damage type to assign to the generated damage event.
	 * @param Instigator Player that causes the damage and whose units it reaches.
	 * @param ShieldDamageSource Identifies shieldable AOE versus explicitly bypassing mine damage.
	 * @param ActorsToIgnore Actors that should be excluded from the sweep and from receiving damage.
	 */
	static void DealDamageInRadiusAsync(
		AActor* DamageCauser,
		const FVector& Epicenter,
		float Radius,
		float BaseDamage,
		float DamageFalloffExponent,
		ERTSDamageType DamageType,
		const FAoeInstigator& Instigator,
		EShieldDamageSource ShieldDamageSource,
		const TArray<TWeakObjectPtr<AActor>>& ActorsToIgnore = TArray<TWeakObjectPtr<AActor>>()
	);

	/**
	 * @brief Owner-relative version of DealDamageVsRearArmorInRadiusAsync; the parameters it shares mean the same.
	 * @param Instigator Player that causes the damage and whose units it reaches.
	 */
	static void DealDamageVsRearArmorInRadiusAsync(
		AActor* DamageCauser,
		const FVector& Epicenter,
		float Radius,
		float BaseDamage,
		float DamageFalloffExponent,
		float FullDmgArmorPen,
		float ArmorPenFallOff,
		float MaxArmorPen,
		ERTSDamageType DamageType,
		const FAoeInstigator& Instigator,
		EShieldDamageSource ShieldDamageSource,
		const TArray<TWeakObjectPtr<AActor>>& ActorsToIgnore = TArray<TWeakObjectPtr<AActor>>(),
		EVehicleModuleSplashPolicy ModuleSplashPolicy = EVehicleModuleSplashPolicy::Ignore,
		EWeaponShellType ModuleDamageShellType = static_cast<EWeaponShellType>(0),
		EAoeFalloffDistance FalloffDistance = EAoeFalloffDistance::ActorOrigin
	);

	/**
	 * @brief Apply a callback to all actors found via async sphere sweep.
	 * @param InstigatorActor Actor used for world access and self-ignoring.
	 * @param Epicenter Center of the area search.
	 * @param Radius Radius of the sphere sweep.
	 * @param OverlapLogic Determines whether to target player units, enemies or both.
	 * @param OnActorFound Callback invoked on the game thread for each hit actor.
	 */
	static void ApplyToActorsAsync(
		AActor* InstigatorActor,
		const FVector& Epicenter,
		float Radius,
		ETriggerOverlapLogic OverlapLogic,
		TFunction<void(AActor&)> OnActorFound
	);

	/**
	 * @brief Deal damage with falloff to all actors in range.
	 * @param DamageCauser Actor credited as damage causer.
	 * @param Epicenter Center of the area search.
	 * @param Radius Radius of the damage sphere.
	 * @param BaseDamage Damage applied at the epicenter before falloff.
	 * @param DamageFalloffExponent Exponent controlling how aggressively damage decays towards the edge.
	 * @param DamageType Damage type to assign to the generated damage event.
	 * @param OverlapLogic Determines whether to target player units, enemies or both.
	 * @param ActorsToIgnore Actors that should be excluded from the sweep and from receiving damage.
	 * @param ShieldDamageSource Identifies shieldable AOE versus explicitly bypassing mine damage.
	 */
	static void DealDamageInRadiusAsync(
		AActor* DamageCauser,
		const FVector& Epicenter,
		float Radius,
		float BaseDamage,
		float DamageFalloffExponent,
		ERTSDamageType DamageType,
		ETriggerOverlapLogic OverlapLogic,
		EShieldDamageSource ShieldDamageSource,
		const TArray<TWeakObjectPtr<AActor>>& ActorsToIgnore = TArray<TWeakObjectPtr<AActor>>()
	);

	/**
	 * @brief Deal damage with rear-armor-aware falloff to all actors in range.
	 * @param DamageCauser Actor credited as damage causer.
	 * @param Epicenter Center of the area search.
	 * @param Radius Radius of the damage sphere.
	 * @param BaseDamage Damage applied at the epicenter before falloff.
	 * @param DamageFalloffExponent Exponent controlling how aggressively damage decays towards the edge.
	 * @param FullDmgArmorPen Armor value threshold that still receives full damage.
	 * @param ArmorPenFallOff Controls how quickly damage decays as armor approaches MaxArmorPen.
	 * @param MaxArmorPen Armor value at which damage is fully negated.
	 * @param DamageType Damage type to assign to the generated damage event.
	 * @param OverlapLogic Determines whether to target player units, enemies or both.
	 * @param ActorsToIgnore Actors that should be excluded from the sweep and from receiving damage.
	 * @param ShieldDamageSource Identifies shieldable AOE versus explicitly bypassing mine damage.
	 * @param ModuleSplashPolicy Opt-in splash damage to external vehicle modules; Ignore keeps hull-only damage.
	 * @param ModuleDamageShellType Shell responsible for module splash, or Shell_None for non-shell sources.
	 * @param FalloffDistance ActorOrigin keeps the historic falloff; ClosestCollision measures to the hull.
	 */
	static void DealDamageVsRearArmorInRadiusAsync(
		AActor* DamageCauser,
		const FVector& Epicenter,
		float Radius,
		float BaseDamage,
		float DamageFalloffExponent,
		float FullDmgArmorPen,
		float ArmorPenFallOff,
		float MaxArmorPen,
		ERTSDamageType DamageType,
		ETriggerOverlapLogic OverlapLogic,
		EShieldDamageSource ShieldDamageSource,
		const TArray<TWeakObjectPtr<AActor>>& ActorsToIgnore = TArray<TWeakObjectPtr<AActor>>(),
		EVehicleModuleSplashPolicy ModuleSplashPolicy = EVehicleModuleSplashPolicy::Ignore,
		EWeaponShellType ModuleDamageShellType = static_cast<EWeaponShellType>(0),
		EAoeFalloffDistance FalloffDistance = EAoeFalloffDistance::ActorOrigin
	);

	/**
	 * @brief Distance used by EAoeFalloffDistance::ClosestCollision for one hit component of a victim.
	 * @param HitActor Victim, used for its origin when there is no component.
	 * @param HitComponent Component the sweep hit; may be null.
	 * @param Epicenter Center of the explosion.
	 * @param OutSource What the distance was measured to.
	 * @return Distance in cm, 0 when the epicenter is inside the collision.
	 */
	static float GetFalloffDistance(
		const AActor& HitActor,
		const UPrimitiveComponent* HitComponent,
		const FVector& Epicenter,
		EAoeDistanceSource& OutSource);

	/**
	 * @brief Damage multiplier the rear-armor AOE applies for a victim's rear armor.
	 * @param RearArmor Rear hull armor of the victim.
	 * @param FullDmgArmorPen Armor that still takes full damage.
	 * @param ArmorPenFallOff How fast damage decays between full and max armor pen.
	 * @param MaxArmorPen Armor at which damage is fully negated.
	 * @return 0 to 1.
	 */
	static float GetRearArmorDamageMultiplier(
		float RearArmor,
		float FullDmgArmorPen,
		float ArmorPenFallOff,
		float MaxArmorPen);

	/**
	 * @brief Deal damage with falloff while allowing custom handling for actors with armor calculation.
	 * @param DamageCauser Actor credited as damage causer.
	 * @param Epicenter Center of the area search.
	 * @param Radius Radius of the damage sphere.
	 * @param BaseDamage Damage applied at the epicenter before falloff.
	 * @param DamageFalloffExponent Exponent controlling how aggressively damage decays towards the edge.
	 * @param DamageType Damage type to assign to the generated damage event.
	 * @param OverlapLogic Determines whether to target player units, enemies or both.
	 * @param OnArmorComponentHit Custom handler invoked when an actor with armor is hit.
	 * @param ActorsToIgnore Actors that should be excluded from the sweep and from receiving damage.
	 * @param ShieldDamageSource Identifies shieldable AOE versus explicitly bypassing mine damage.
	 */
	static void DealDamageAndCustomArmorHandlingInRadiusAsync(
		AActor* DamageCauser,
		const FVector& Epicenter,
		float Radius,
		float BaseDamage,
		float DamageFalloffExponent,
		ERTSDamageType DamageType,
		ETriggerOverlapLogic OverlapLogic,
		EShieldDamageSource ShieldDamageSource,
		TFunction<void(UArmorCalculation*, AActor*)> OnArmorComponentHit,
		const TArray<TWeakObjectPtr<AActor>>& ActorsToIgnore = TArray<TWeakObjectPtr<AActor>>()
	);

	/**
	 * @brief Apply a behaviour class to all actors within the provided radius.
	 * @param InstigatorActor Actor used for world access and self-ignoring.
	 * @param Epicenter Center of the area search.
	 * @param Radius Radius of the behaviour application sphere.
	 * @param BehaviourClass Behaviour class to add to each found actor.
	 * @param OverlapLogic Determines whether to target player units, enemies or both.
	 * @param ActorsToIgnore Actors that should be excluded from the sweep and from behaviour application.
	 */
	static void ApplyBehaviourInRadiusAsync(
		AActor* InstigatorActor,
		const FVector& Epicenter,
		float Radius,
		TSubclassOf<UBehaviour> BehaviourClass,
		ETriggerOverlapLogic OverlapLogic,
		const TArray<TWeakObjectPtr<AActor>>& ActorsToIgnore = TArray<TWeakObjectPtr<AActor>>()
	);

private:
	// What one sweep looks for: its object channels and, for an owner-relative effect, whose units it keeps.
	struct FAoeSweepTargets
	{
		ETriggerOverlapLogic OverlapLogic = ETriggerOverlapLogic::OverlapBoth;
		// Not set by the authored and mission entry points: they keep every unit they find, as they always did.
		TOptional<FAoeInstigator> UnitOwnerFilter;
		// Player the damage is attributed to in map tests; INDEX_NONE for the authored and mission entry points.
		int32 AttackingPlayer = INDEX_NONE;
	};

	static FAoeSweepTargets MakeLiteralSweepTargets(ETriggerOverlapLogic OverlapLogic);
	static FAoeSweepTargets MakeOwnerRelativeSweepTargets(const FAoeInstigator& Instigator);

	/** @return Whether an owner-relative effect reaches this hit: every destructible, and units by their owner. */
	static bool GetIsHitReachedByInstigator(const FHitResult& Hit, const FAoeInstigator& Instigator);

	static void SweepAndApplyToActors(
		AActor* InstigatorActor,
		const FVector& Epicenter,
		float Radius,
		const FAoeSweepTargets& SweepTargets,
		TFunction<void(AActor&)> OnActorFound
	);

	static void SweepAndDealDamageInRadius(
		AActor* DamageCauser,
		const FVector& Epicenter,
		float Radius,
		float BaseDamage,
		float DamageFalloffExponent,
		ERTSDamageType DamageType,
		const FAoeSweepTargets& SweepTargets,
		EShieldDamageSource ShieldDamageSource,
		const TArray<TWeakObjectPtr<AActor>>& ActorsToIgnore
	);

	static void SweepAndDealDamageVsRearArmor(
		AActor* DamageCauser,
		const FVector& Epicenter,
		float Radius,
		float BaseDamage,
		float DamageFalloffExponent,
		float FullDmgArmorPen,
		float ArmorPenFallOff,
		float MaxArmorPen,
		ERTSDamageType DamageType,
		const FAoeSweepTargets& SweepTargets,
		EShieldDamageSource ShieldDamageSource,
		const TArray<TWeakObjectPtr<AActor>>& ActorsToIgnore,
		EVehicleModuleSplashPolicy ModuleSplashPolicy,
		EWeaponShellType ModuleDamageShellType,
		EAoeFalloffDistance FalloffDistance
	);

	static FCollisionObjectQueryParams BuildObjectQueryParams(ETriggerOverlapLogic OverlapLogic);
	static void StartAsyncSphereSweep(
		AActor* InstigatorActor,
		const FVector& Epicenter,
		float Radius,
		const FAoeSweepTargets& SweepTargets,
		const TArray<TWeakObjectPtr<AActor>>& ActorsToIgnore,
		TFunction<void(TArray<FHitResult>&&)> OnSweepComplete
	);
};
