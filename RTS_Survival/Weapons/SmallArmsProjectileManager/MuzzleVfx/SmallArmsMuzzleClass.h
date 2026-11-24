// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "SmallArmsMuzzleClass.generated.h"

enum class EWeaponName : uint32;

/**
 * Which muzzle effect a hand-held weapon gets from the small arms muzzle batcher; picked from the weapon name.
 * None: the weapon keeps its own launch effect (vehicle MGs, launchers, lasers, flamethrowers).
 */
UENUM(BlueprintType)
enum class ESmallArmsMuzzleClass : uint8
{
	None,
	Pistol,
	SubmachineGun,
	AssaultRifle,
	Rifle,
	MachineGun,
	AntiTankRifle
};

/** How the muzzle flash and smoke of one class of small arms look; Project Settings → Game → RTS Weapon VFX. */
USTRUCT(BlueprintType)
struct FSmallArmsMuzzleProfile
{
	GENERATED_BODY()

	// Scales the size of the flash; 1 is a bolt-action rifle.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.05"))
	float FlashScale = 1.0f;

	// Scales the size of the smoke puff and how far it is blown out of the barrel; 1 is a bolt-action rifle.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.05"))
	float SmokeScale = 1.0f;

	// Scales how long the smoke hangs in the air.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.1"))
	float SmokeLifetimeMlt = 1.0f;

	// An automatic weapon puffs smoke at most this often; the rounds in between only flash. Keeps a long burst
	// from stacking dozens of overlapping translucent puffs, which is what costs GPU time.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.0"))
	float MinSecondsBetweenSmokePuffs = 0.0f;
};

namespace SmallArmsMuzzleClass
{
	/** @return The muzzle class of a hand-held weapon, None for every weapon the batcher does not draw. */
	RTS_SURVIVAL_API ESmallArmsMuzzleClass GetMuzzleClassForWeapon(const EWeaponName WeaponName);

	/** @return The profile the settings start with for a class, before a designer changes it. */
	RTS_SURVIVAL_API FSmallArmsMuzzleProfile GetDefaultProfile(const ESmallArmsMuzzleClass MuzzleClass);
}
