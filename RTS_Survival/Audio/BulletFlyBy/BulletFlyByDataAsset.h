// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "BulletFlyByDataAsset.generated.h"

class USoundAttenuation;
class USoundBase;
class USoundClass;
class USoundConcurrency;

/**
 * @brief The bullet fly-by sounds and how loud and how often they play.
 * UBulletFlyByAudioSubsystem plays one of FlyBySounds, picked at random, when a small-arms tracer passes close to the
 * point the camera looks at. Assigned in Project Settings > Game > RTS Audio Settings > Bullet Fly By Data.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UBulletFlyByDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	// ------------------------------- Sounds ------------------------------- //

	/** One of these is picked at random for every fly-by; the same sound is not picked twice in a row. Short sounds
	 * (under ~0.6 s) read as whizzes; long ones smear in a heavy fight. */
	UPROPERTY(EditAnywhere, Category = "Sounds")
	TArray<TObjectPtr<USoundBase>> FlyBySounds;

	/** Sound class the fly-bys play in, so the SFX volume slider governs them (the fly-by waves have no class). */
	UPROPERTY(EditAnywhere, Category = "Sounds")
	TObjectPtr<USoundClass> SoundClass = nullptr;

	/** Distance from the camera to the fly-by; must reach past MaxCameraDistance + PassRadius or zoomed-out fly-bys
	 * fade out a second time. Spatialized so the whiz pans left/right. */
	UPROPERTY(EditAnywhere, Category = "Sounds")
	TObjectPtr<USoundAttenuation> Attenuation = nullptr;

	/** Engine-side cap on fly-bys playing at once; the subsystem already limits them, this guards the voice count. */
	UPROPERTY(EditAnywhere, Category = "Sounds")
	TObjectPtr<USoundConcurrency> Concurrency = nullptr;

	/** Voice priority; below the gunfire and impact sounds (1.0) so the engine drops a fly-by first when out of voices. */
	UPROPERTY(EditAnywhere, Category = "Sounds", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float Priority = 0.4f;

	// ------------------------------- Loudness ------------------------------- //

	/** Volume of a bullet that passes right over the point the camera looks at. */
	UPROPERTY(EditAnywhere, Category = "Loudness", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float VolumeAtClosestPass = 0.6f;

	/** Volume of a bullet that passes at PassRadius; volumes in between blend linearly. */
	UPROPERTY(EditAnywhere, Category = "Loudness", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float VolumeAtPassRadius = 0.2f;

	/** Random pitch per fly-by, picked between X and Y. */
	UPROPERTY(EditAnywhere, Category = "Loudness")
	FVector2D PitchRange = FVector2D(0.92, 1.08);

	// ------------------------------- When a tracer counts as a fly-by ------------------------------- //

	/** A tracer must pass this close (cm, measured flat on the ground) to the point the camera looks at. */
	UPROPERTY(EditAnywhere, Category = "Trigger", meta = (ClampMin = "50.0"))
	float PassRadius = 700.f;

	/** No fly-bys when the camera is further than this (cm) from the point it looks at, i.e. zoomed out. The
	 * camera starts about 3000 cm out (the camera pawn's spring arm) and zooms between 100 and 5200. */
	UPROPERTY(EditAnywhere, Category = "Trigger", meta = (ClampMin = "100.0"))
	float MaxCameraDistance = 4000.f;

	/** Fly-bys play at full volume when the camera is this close (cm) or closer; they fade out towards
	 * MaxCameraDistance. */
	UPROPERTY(EditAnywhere, Category = "Trigger", meta = (ClampMin = "100.0"))
	float FullVolumeCameraDistance = 2600.f;

	/** Passes closer than this (cm) to the muzzle are skipped; the gunshot already covers them. */
	UPROPERTY(EditAnywhere, Category = "Trigger", meta = (ClampMin = "0.0"))
	float MinDistanceFromMuzzle = 250.f;

	/** Passes closer than this (cm) to the impact are skipped; the impact sound already covers them. */
	UPROPERTY(EditAnywhere, Category = "Trigger", meta = (ClampMin = "0.0"))
	float MinDistanceFromImpact = 150.f;

	// ------------------------------- Budget ------------------------------- //

	/** How many fly-bys may play at once; also the number of pooled audio components. */
	UPROPERTY(EditAnywhere, Category = "Budget", meta = (ClampMin = "1", ClampMax = "8"))
	int32 MaxSimultaneousFlyBys = 3;

	/** Average fly-bys per second at most; short bursts up to MaxSimultaneousFlyBys are allowed. */
	UPROPERTY(EditAnywhere, Category = "Budget", meta = (ClampMin = "0.1"))
	float MaxFlyBysPerSecond = 6.f;

	/** Two fly-bys never start closer together than this (s); the louder of two close ones is kept. */
	UPROPERTY(EditAnywhere, Category = "Budget", meta = (ClampMin = "0.0"))
	float MinSecondsBetweenFlyBys = 0.09f;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
