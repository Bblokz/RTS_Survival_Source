// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DamageEvents.h"

#include "RTS_Survival/Utils/CollisionSetup/TriggerOverlapLogic.h"
#include "RTS_Survival/Weapons/WeaponData/RTSDamageTypes/RTSDamageTypes.h"

class UBehaviour;
class UBehaviourComp;

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
	 */
	static void DealDamageInRadiusAsync(
		AActor* DamageCauser,
		const FVector& Epicenter,
		float Radius,
		float BaseDamage,
		float DamageFalloffExponent,
		ERTSDamageType DamageType,
		ETriggerOverlapLogic OverlapLogic
	);

	/**
	 * @brief Apply a behaviour class to all actors within the provided radius.
	 * @param InstigatorActor Actor used for world access and self-ignoring.
	 * @param Epicenter Center of the area search.
	 * @param Radius Radius of the behaviour application sphere.
	 * @param BehaviourClass Behaviour class to add to each found actor.
	 * @param OverlapLogic Determines whether to target player units, enemies or both.
	 */
	static void ApplyBehaviourInRadiusAsync(
		AActor* InstigatorActor,
		const FVector& Epicenter,
		float Radius,
		TSubclassOf<UBehaviour> BehaviourClass,
		ETriggerOverlapLogic OverlapLogic
	);

private:
	static FCollisionObjectQueryParams BuildObjectQueryParams(ETriggerOverlapLogic OverlapLogic);
	static void StartAsyncSphereSweep(
		AActor* InstigatorActor,
		const FVector& Epicenter,
		float Radius,
		const FCollisionObjectQueryParams& ObjectQueryParams,
		TFunction<void(TArray<FHitResult>&&)> OnSweepComplete
	);
};
