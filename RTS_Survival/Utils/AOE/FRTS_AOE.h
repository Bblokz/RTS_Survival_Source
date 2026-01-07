// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DamageEvents.h"

#include "RTS_Survival/Utils/CollisionSetup/TriggerOverlapLogic.h"
#include "RTS_Survival/Weapons/WeaponData/RTSDamageTypes/RTSDamageTypes.h"

class UBehaviour;
class UBehaviourComp;
class UArmorCalculation;

/**
 * @brief Utility struct providing async area-of-effect helpers for damage and behaviours.
 */
struct RTS_SURVIVAL_API FRTS_AOE
{
	FRTS_AOE();
	~FRTS_AOE();

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
	 */
	static void DealDamageInRadiusAsync(
		AActor* DamageCauser,
		const FVector& Epicenter,
		float Radius,
		float BaseDamage,
		float DamageFalloffExponent,
		ERTSDamageType DamageType,
		ETriggerOverlapLogic OverlapLogic,
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
		const TArray<TWeakObjectPtr<AActor>>& ActorsToIgnore = TArray<TWeakObjectPtr<AActor>>()
	);

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
	 */
	static void DealDamageAndCustomArmorHandlingInRadiusAsync(
		AActor* DamageCauser,
		const FVector& Epicenter,
		float Radius,
		float BaseDamage,
		float DamageFalloffExponent,
		ERTSDamageType DamageType,
		ETriggerOverlapLogic OverlapLogic,
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
	static FCollisionObjectQueryParams BuildObjectQueryParams(ETriggerOverlapLogic OverlapLogic);
	static void StartAsyncSphereSweep(
		AActor* InstigatorActor,
		const FVector& Epicenter,
		float Radius,
		const FCollisionObjectQueryParams& ObjectQueryParams,
		const TArray<TWeakObjectPtr<AActor>>& ActorsToIgnore,
		TFunction<void(TArray<FHitResult>&&)> OnSweepComplete
	);
};
