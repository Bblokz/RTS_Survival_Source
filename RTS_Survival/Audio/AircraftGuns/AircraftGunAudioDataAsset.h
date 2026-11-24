// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "AircraftGunAudioDataAsset.generated.h"

class USoundAttenuation;
class USoundBase;
class USoundConcurrency;
class USoundCue;

/** Which sound set an aircraft gun uses. */
UENUM(BlueprintType)
enum class EAircraftGunAudioKind : uint8
{
	// Rifle-calibre strafe MGs (UWeaponStateAircraftStrafeGun).
	MachineGun,
	// 15-25 mm aircraft cannons fired as projectiles (the MG 151/20 of the Me 410 and Fw 189).
	Cannon,
	// 30 mm strafe cannons (strafe guns from HeavyCannonFromCalibreMm up: the MK 108 of the Ho 229).
	HeavyCannon
};

/**
 * @brief The sounds of one kind of aircraft gun. A burst holds one looping voice on the aircraft (FireLoop*) and, for the
 * strafe MGs, one looping voice at the beaten zone (ImpactLoop*); both are baked at the real rate of fire and end with a
 * tail. See Source/Docs/Units/Aircraft/AircraftStrafeGun.md, section "Strafe sound".
 */
USTRUCT(BlueprintType)
struct FAircraftGunSoundSet
{
	GENERATED_BODY()

	// ------------------------------- Fire ------------------------------- //

	/** Looping wave of the guns firing, for 1-2 guns. Must loop seamlessly (SoundWave Looping on). */
	UPROPERTY(EditAnywhere, Category = "Fire")
	TObjectPtr<USoundBase> FireLoopFewGuns = nullptr;

	/** Looping wave for ManyGunsFromSockets or more guns; falls back to FireLoopFewGuns when empty. */
	UPROPERTY(EditAnywhere, Category = "Fire")
	TObjectPtr<USoundBase> FireLoopManyGuns = nullptr;

	/** One of these plays on the aircraft when a burst ends: the last round and its echo. */
	UPROPERTY(EditAnywhere, Category = "Fire")
	TArray<TObjectPtr<USoundBase>> FireTails;

	UPROPERTY(EditAnywhere, Category = "Fire", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float FireVolume = 0.8f;

	// ------------------------------- Impacts (strafe MGs) ------------------------------- //

	/** Looping churn of rounds landing in dirt or sand, for 1-2 guns. */
	UPROPERTY(EditAnywhere, Category = "Impacts")
	TObjectPtr<USoundBase> ImpactLoopSoftFewGuns = nullptr;

	UPROPERTY(EditAnywhere, Category = "Impacts")
	TObjectPtr<USoundBase> ImpactLoopSoftManyGuns = nullptr;

	/** Looping churn of rounds landing on stone: more ricochets, less thud. Falls back to the soft loops. */
	UPROPERTY(EditAnywhere, Category = "Impacts")
	TObjectPtr<USoundBase> ImpactLoopHardFewGuns = nullptr;

	UPROPERTY(EditAnywhere, Category = "Impacts")
	TObjectPtr<USoundBase> ImpactLoopHardManyGuns = nullptr;

	/** One of these plays at the zone when the rounds stop arriving: the last hits thinning out, dirt pattering. */
	UPROPERTY(EditAnywhere, Category = "Impacts")
	TArray<TObjectPtr<USoundBase>> ImpactTails;

	/** Rounds hammering a unit (3-6 clangs and a ricochet); at most one per UnitHitMinIntervalSeconds per gun. */
	UPROPERTY(EditAnywhere, Category = "Impacts")
	TArray<TObjectPtr<USoundBase>> UnitHitClusters;

	/** Cannon: replaces the weapon's sand and stone impact sound, so every projectile that hits the ground plays one
	 * (a cue: the projectile impact path plays cues). */
	UPROPERTY(EditAnywhere, Category = "Impacts")
	TObjectPtr<USoundCue> GroundImpactPerProjectile = nullptr;

	UPROPERTY(EditAnywhere, Category = "Impacts", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float ImpactVolume = 0.85f;

	UPROPERTY(EditAnywhere, Category = "Impacts", meta = (ClampMin = "0.02"))
	float UnitHitMinIntervalSeconds = 0.12f;

	// ------------------------------- Timing ------------------------------- //

	/** A gun with this many fire sockets or more uses the ManyGuns loops. */
	UPROPERTY(EditAnywhere, Category = "Timing", meta = (ClampMin = "2"))
	int32 ManyGunsFromSockets = 3;

	/** The burst counts as ended when no volley came for this many volley intervals (measured while firing). */
	UPROPERTY(EditAnywhere, Category = "Timing", meta = (ClampMin = "1.2"))
	float StopAfterMissedVolleys = 2.5f;

	/** Never end a burst sooner than this after the last volley (s); also the gap used before an interval is known. */
	UPROPERTY(EditAnywhere, Category = "Timing", meta = (ClampMin = "0.02"))
	float MinStopGapSeconds = 0.14f;

	/** Never wait longer than this after the last volley before ending a burst (s). */
	UPROPERTY(EditAnywhere, Category = "Timing", meta = (ClampMin = "0.05"))
	float MaxStopGapSeconds = 0.45f;

	/** Fade-out of a loop when its burst ends (s); the tail covers it. */
	UPROPERTY(EditAnywhere, Category = "Timing", meta = (ClampMin = "0.0"))
	float LoopFadeOutSeconds = 0.06f;

	/** Random pitch per burst, picked between X and Y. */
	UPROPERTY(EditAnywhere, Category = "Timing")
	FVector2D PitchRange = FVector2D(0.96, 1.04);

	/** The impact loop follows the zone; each arriving round pulls it this share of the way to the round. */
	UPROPERTY(EditAnywhere, Category = "Timing", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float ImpactCentreFollowShare = 0.3f;

	bool HasFireLoop() const { return FireLoopFewGuns != nullptr || FireLoopManyGuns != nullptr; }
	bool HasImpactLoop() const { return ImpactLoopSoftFewGuns != nullptr || ImpactLoopSoftManyGuns != nullptr; }
};

/**
 * @brief Aircraft gun sounds: rifle-calibre strafe MGs and 15-25 mm cannons. Played by UAircraftGunAudioPlayer, made by
 * UAircraftWeapon for every SetupMultiTraceWeapon MG and every SetupMultiProjectileWeapon cannon in the cannon calibre
 * range. Assigned in Project Settings > Game > RTS Audio Settings > Aircraft Gun Audio Data.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UAircraftGunAudioDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Sets")
	FAircraftGunSoundSet MachineGun;

	UPROPERTY(EditAnywhere, Category = "Sets")
	FAircraftGunSoundSet Cannon;

	/** 30 mm strafe cannons; without a fire loop they keep their Blueprint sounds. */
	UPROPERTY(EditAnywhere, Category = "Sets")
	FAircraftGunSoundSet HeavyCannon;

	/** Strafe guns with this calibre (mm) or more use the HeavyCannon set. */
	UPROPERTY(EditAnywhere, Category = "Sets")
	float HeavyCannonFromCalibreMm = 28.f;

	/** Projectile aircraft weapons with a calibre (mm) in this range use the Cannon set. */
	UPROPERTY(EditAnywhere, Category = "Sets")
	FVector2D CannonCalibreRangeMm = FVector2D(15.0, 25.0);

	/** Gunfire, spatialized; aircraft fire from high up, so this reaches further than ground small arms. */
	UPROPERTY(EditAnywhere, Category = "Settings")
	TObjectPtr<USoundAttenuation> FireAttenuation = nullptr;

	UPROPERTY(EditAnywhere, Category = "Settings")
	TObjectPtr<USoundConcurrency> FireConcurrency = nullptr;

	UPROPERTY(EditAnywhere, Category = "Settings")
	TObjectPtr<USoundAttenuation> ImpactAttenuation = nullptr;

	UPROPERTY(EditAnywhere, Category = "Settings")
	TObjectPtr<USoundConcurrency> ImpactConcurrency = nullptr;

	const FAircraftGunSoundSet& GetSet(const EAircraftGunAudioKind Kind) const
	{
		switch (Kind)
		{
		case EAircraftGunAudioKind::Cannon:
			return Cannon;
		case EAircraftGunAudioKind::HeavyCannon:
			return HeavyCannon;
		default:
			return MachineGun;
		}
	}

	bool GetIsHeavyCannonCalibre(const float CalibreMm) const { return CalibreMm >= HeavyCannonFromCalibreMm; }

	bool GetIsCannonCalibre(const float CalibreMm) const
	{
		return CalibreMm >= CannonCalibreRangeMm.X && CalibreMm <= CannonCalibreRangeMm.Y;
	}

	/** The asset set in RTS Audio Settings, loaded once; nullptr (reported once) when unset or missing. */
	static const UAircraftGunAudioDataAsset* Get();

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
