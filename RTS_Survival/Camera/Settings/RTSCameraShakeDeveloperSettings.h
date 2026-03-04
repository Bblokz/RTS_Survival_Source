#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTSCameraShakeDeveloperSettings.generated.h"

class UCameraShakeBase;

/**
 * @brief Global project settings used to tune camera shake from projectile weapon fire and explosions.
 * Designers use this in Project Settings to control thresholds, distance response, and anti-spam behaviour
 * without touching gameplay code.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="RTS Camera Shake"))
class RTS_SURVIVAL_API URTSCameraShakeDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URTSCameraShakeDeveloperSettings();

	/**
	 * @brief Enables all weapon fire camera shake requests coming from projectile weapon systems.
	 * Disable this when tuning only explosion shake to isolate feedback while keeping code paths active.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Camera Shake|Global")
	bool bM_EnableWeaponFireShake = true;

	/**
	 * @brief Enables all explosion camera shake requests coming from projectile impact/explosion events.
	 * Disable this when designers want only muzzle/fire shake without explosive impact influence.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Camera Shake|Global")
	bool bM_EnableExplosionShake = true;

	/**
	 * @brief Requires weapon fire events to be in camera view before they can shake the camera.
	 * Near-distance safety rules can still allow very close events to pass for readability.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Camera Shake|Visibility")
	bool bM_RequireInFovForWeaponFire = true;

	/**
	 * @brief Requires explosion events to be in camera view before they can shake the camera.
	 * This keeps distant off-screen explosions from constantly nudging the camera.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Camera Shake|Visibility")
	bool bM_RequireInFovForExplosion = false;

	/**
	 * @brief Minimum calibre in millimetres allowed to request weapon-fire shake.
	 * Values below this threshold are discarded early for performance and readability.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Camera Shake|Thresholds", meta=(ClampMin="1", UIMin="1"))
	int32 M_MinWeaponFireCalibreMm = 20;

	/**
	 * @brief Minimum calibre in millimetres allowed to request explosion shake.
	 * This lets designers keep micro impacts visually loud while avoiding camera noise.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Camera Shake|Thresholds", meta=(ClampMin="1", UIMin="1"))
	int32 M_MinExplosionCalibreMm = 35;

	/**
	 * @brief Reference calibre used to normalize intensity and distance calculations into [0..1].
	 * Raising this value makes all calibres feel lighter; lowering makes more events saturate faster.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Camera Shake|Thresholds", meta=(ClampMin="1", UIMin="1"))
	int32 M_MaxCalibreForNormalizationMm = 160;

	/**
	 * @brief Distance in centimetres where events can always shake regardless of in-view requirement.
	 * This protects local readability for very nearby heavy fire even during camera pans.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Camera Shake|Distance", meta=(ClampMin="0.0", UIMin="0.0"))
	float M_AlwaysShakeDistanceCm = 1800.0f;

	/**
	 * @brief Base maximum distance in centimetres for shake eligibility before calibre scaling.
	 * Final per-event max distance is this base multiplied by calibre-derived distance scaling.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Camera Shake|Distance", meta=(ClampMin="0.0", UIMin="0.0"))
	float M_BaseMaxShakeDistanceCm = 9000.0f;

	/**
	 * @brief Distance scale applied at minimum normalized calibre when building max shake distance.
	 * Lower values make small calibres drop out sooner even if they are visible.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Camera Shake|Distance", meta=(ClampMin="0.0", UIMin="0.0"))
	float M_MinCalibreDistanceScale = 0.4f;

	/**
	 * @brief Distance scale applied at maximum normalized calibre when building max shake distance.
	 * Higher values allow heavy calibre events to remain eligible at longer visible ranges.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Camera Shake|Distance", meta=(ClampMin="0.0", UIMin="0.0"))
	float M_MaxCalibreDistanceScale = 1.0f;

	/**
	 * @brief Distance multiplier applied to shake intensity at zero range.
	 * Use this to globally boost close-range punch without changing long-range behaviour.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Camera Shake|Distance", meta=(ClampMin="0.0", UIMin="0.0"))
	float M_DistanceIntensityNearMultiplier = 1.0f;

	/**
	 * @brief Distance multiplier applied to shake intensity at the event-specific max distance.
	 * Keep this lower than near multiplier to produce natural falloff with range.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Camera Shake|Distance", meta=(ClampMin="0.0", UIMin="0.0"))
	float M_DistanceIntensityFarMultiplier = 0.05f;

	/**
	 * @brief Weight used when aggregating weapon-fire requests inside one accumulation window.
	 * Increase to favour muzzle-driven shake relative to explosion-driven shake.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Camera Shake|Aggregation", meta=(ClampMin="0.0", UIMin="0.0"))
	float M_WeaponFireAggregationWeight = 1.0f;

	/**
	 * @brief Weight used when aggregating explosion requests inside one accumulation window.
	 * Increase to make impacts dominate over muzzle events during chaotic engagements.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Camera Shake|Aggregation", meta=(ClampMin="0.0", UIMin="0.0"))
	float M_ExplosionAggregationWeight = 1.25f;

	/**
	 * @brief Time window in seconds that batches incoming requests into one aggregated shake.
	 * Larger windows reduce shake spam and cost, but can feel less reactive.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Camera Shake|Aggregation", meta=(ClampMin="0.01", UIMin="0.01"))
	float M_AggregationWindowSeconds = 0.05f;

	/**
	 * @brief Hard cap on accepted requests per second before dropping additional requests.
	 * This guards against pathological stress cases with large projectile counts.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Camera Shake|Aggregation", meta=(ClampMin="1", UIMin="1"))
	int32 M_MaxRequestsPerSecond = 120;

	/**
	 * @brief Intensity threshold that upgrades an aggregated pulse to the heavy shake asset.
	 * Values below this threshold use the normal shake class.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Camera Shake|Aggregation", meta=(ClampMin="0.0", UIMin="0.0"))
	float M_HeavyShakeThreshold = 0.7f;

	/**
	 * @brief Minimum time in seconds between heavy shake plays.
	 * Prevents repeated heavy shakes from stacking into unreadable camera motion.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Camera Shake|Aggregation", meta=(ClampMin="0.0", UIMin="0.0"))
	float M_MinTimeBetweenHeavyShakesSeconds = 0.2f;

	/**
	 * @brief Global multiplier applied to all aggregated intensity before shake class selection.
	 * Use for final balancing pass once individual parameters are tuned.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Camera Shake|Aggregation", meta=(ClampMin="0.0", UIMin="0.0"))
	float M_GlobalIntensityMultiplier = 1.0f;

	/**
	 * @brief Camera shake asset used for normal aggregated pulses.
	 * Keep this short and subtle; it will be triggered often in intense firefights.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Camera Shake|Assets")
	TSoftClassPtr<UCameraShakeBase> M_NormalShakeClass;

	/**
	 * @brief Camera shake asset used when aggregated intensity crosses heavy threshold.
	 * Keep this impactful and less frequent than the normal asset.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Camera Shake|Assets")
	TSoftClassPtr<UCameraShakeBase> M_HeavyShakeClass;
};
