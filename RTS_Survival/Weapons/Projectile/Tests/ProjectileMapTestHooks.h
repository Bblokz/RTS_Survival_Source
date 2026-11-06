// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "RTS_Survival/RTSComponents/ArmorComponent/Armor.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponSystems.h"

enum class EWeaponShellType : uint8;

/** What a kinetic shell did to the armor it hit, as the projectile resolved it. */
enum class EMapTestArmorHitVerdict : uint8
{
	Bounce,
	Penetration,
	OverPenetration,
	// Realistic vehicle balance overmatch: "ARMOR DESTROYED!!".
	ArmorDestroyed,
	// Realistic vehicle balance side or rear hit that detonated the ammunition.
	FlankAmmoDetonation
};

/** One kinetic shell impact on an armored unit, with the health the victim really lost to it. */
struct FMapTestArmorHitReport
{
	// Launch counter of the shell; an over-penetrating shell reports every plate it passed with the same id.
	uint32 ShellLaunchId = 0;
	TWeakObjectPtr<UObject> WeaponOwner;
	TWeakObjectPtr<AActor> HitActor;
	// A killing hit can remove the victim before the report goes out; the id still names it.
	uint32 HitActorUniqueId = 0;
	EWeaponName WeaponName = EWeaponName::DEFAULT_WEAPON;
	EWeaponShellType ShellType = {};
	int32 WeaponCalibre = 0;
	EArmorPlate PlateHit = EArmorPlate::Plate_Front;
	float RawArmor = 0.f;
	float EffectiveArmor = 0.f;
	float PenAtImpact = 0.f;
	float ShellDamage = 0.f;
	float DamageMlt = 0.f;
	float HealthBefore = 0.f;
	float HealthAfter = 0.f;
	EMapTestArmorHitVerdict Verdict = EMapTestArmorHitVerdict::Bounce;
};

/** One shell leaving a projectile weapon. */
struct FMapTestShellLaunchReport
{
	uint32 ShellLaunchId = 0;
	TWeakObjectPtr<UObject> WeaponOwner;
	EWeaponName WeaponName = EWeaponName::DEFAULT_WEAPON;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnMapTestArmorHit, const FMapTestArmorHitReport&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMapTestShellLaunched, const FMapTestShellLaunchReport&);

/**
 * @brief Lets map tests such as the vehicle duel bench see every shell fired and every armor hit it made, with the
 * health the victim really lost. Compiled only with RTS_WITH_MAP_TESTS; the projectile broadcasts, tests bind.
 */
namespace FProjectileMapTestHooks
{
	RTS_SURVIVAL_API FOnMapTestArmorHit& OnArmorHit();
	RTS_SURVIVAL_API FOnMapTestShellLaunched& OnShellLaunched();

	/** @return Whether any map test listens; the projectile skips all report work when none does. */
	RTS_SURVIVAL_API bool GetHasListeners();

	/** Gives the pooled projectile a fresh launch id and tells the listeners a shell left the weapon. */
	RTS_SURVIVAL_API void ReportShellLaunched(const UObject* Projectile, UObject* WeaponOwner, EWeaponName WeaponName);

	/** @return The launch id of the projectile's current flight, or 0 when it was launched before anyone listened. */
	RTS_SURVIVAL_API uint32 GetShellLaunchId(const UObject* Projectile);

	/** @return Current health of the actor's health component, or 0 for a destroyed actor or one without health. */
	RTS_SURVIVAL_API float GetHealthOrZero(const AActor* Actor);
}

#endif
