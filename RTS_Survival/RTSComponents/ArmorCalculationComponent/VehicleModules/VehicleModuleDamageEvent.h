// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DamageEvents.h"
#include "VehicleModuleTypes.h"

class UArmorCalculation;

/** @brief Weapon-side description of one resolved armor hit, used to build a module damage event. */
struct FVehicleModuleBallisticHit
{
	UArmorCalculation* ArmorCalculation = nullptr;
	FVector HitLocation = FVector::ZeroVector;
	EArmorPlate PlateHit = EArmorPlate::Plate_Front;
	int32 PlateRegistrationId = INDEX_NONE;
	float EffectiveArmor = 0.f;
	float ProjectileBaseDamage = 0.f;
	float ProjectileCalibre = 0.f;
	bool bPenetrated = false;
	bool bOverpenetrating = false;
	EWeaponShellType ShellType = static_cast<EWeaponShellType>(0);
	uint32 ShotActivationId = 0;
	uint32 ImpactOrdinal = 0;
};

/**
 * @brief Carries a resolved armor hit through the normal TakeDamage call.
 * AHpPawnMaster applies module damage after the health component reports the actual applied hull damage,
 * and only when the vehicle survived; plain FDamageEvent callers (e.g. ICBMs) never reach module damage.
 */
struct RTS_SURVIVAL_API FVehicleModuleDamageEvent : public FPointDamageEvent
{
	// 'VMDE'; distinct from the engine's point (1) and radial (2) damage event IDs.
	static constexpr int32 ClassID = 0x564D4445;

	virtual int32 GetTypeID() const override
	{
		return ClassID;
	}

	virtual bool IsOfType(const int32 InID) const override
	{
		return InID == ClassID || FPointDamageEvent::IsOfType(InID);
	}

	TWeakObjectPtr<UArmorCalculation> ArmorCalculation;
	FVehicleModuleHitContext HitContext;
	EArmorPlate PlateHit = EArmorPlate::Plate_Front;
	float EffectiveArmor = 0.f;
	// Weapon base damage for ballistic hits; mine damage or AOE-attenuated damage for the other deliveries.
	float ProjectileBaseDamage = 0.f;
	float ProjectileCalibre = 0.f;
	bool bPenetrated = false;
	// Mine and splash deliveries resolve their candidates from the explosion location.
	FVector ExplosionLocation = FVector::ZeroVector;

	/**
	 * @brief Builds the event for a projectile or trace hit on a registered armor plate.
	 * @param BallisticHit Resolved plate, penetration result and shot identity of the impact.
	 * @param InDamageTypeClass Damage type forwarded to TakeDamage.
	 * @return Event whose module damage is resolved once, after hull damage.
	 */
	static FVehicleModuleDamageEvent MakeBallisticEvent(
		const FVehicleModuleBallisticHit& BallisticHit,
		TSubclassOf<UDamageType> InDamageTypeClass);

	/**
	 * @brief Builds the event for a mine or splash explosion.
	 * @param Armor Armor component of the victim.
	 * @param Delivery Mine or Splash.
	 * @param InExplosionLocation World location of the explosion.
	 * @param SourceDamage Mine damage, or splash damage after the AOE falloff for this victim.
	 * @param ShotActivationId Identity of the explosion for deterministic rolls.
	 * @param InDamageTypeClass Damage type forwarded to TakeDamage.
	 * @return Event whose module damage is resolved once, after hull damage.
	 */
	static FVehicleModuleDamageEvent MakeExplosionEvent(
		UArmorCalculation* Armor,
		EVehicleModuleDelivery Delivery,
		const FVector& InExplosionLocation,
		float SourceDamage,
		uint32 ShotActivationId,
		TSubclassOf<UDamageType> InDamageTypeClass);

	/** @brief Called by the damaged pawn after accepted hull damage; rejected (zero) damage never damages modules. */
	void ApplyModuleDamageAfterHullDamage(float AppliedHullDamage) const;

	/** @brief For non-penetrating bounces that deal no hull damage at all. */
	void ApplyModuleDamageWithoutHullDamage() const;

private:
	void ResolveModuleDamage(float AppliedHullDamage) const;
};
