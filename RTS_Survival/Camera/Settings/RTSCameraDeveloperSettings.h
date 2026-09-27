// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTSCameraDeveloperSettings.generated.h"

/**
 * @brief Zoom, pitch and movement speed tuning for the player camera.
 * One instance holds the normal gameplay values, another holds the cinematic override values.
 */
USTRUCT(BlueprintType)
struct FRTSCameraTuningSettings
{
	GENERATED_BODY()

	/**
	 * @brief Closest the spring arm may bring the camera to its target, in Unreal units.
	 * Lower values let the player push further into the battlefield.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Camera", meta=(ClampMin="0.0", UIMin="0.0"))
	float M_MinZoomLimit = 150.f;

	/**
	 * @brief Furthest the spring arm may pull the camera away from its target, in Unreal units.
	 * 4800 corresponds with 8500 range in the game (weapon on left side engaging weapon just visible on right side).
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Camera", meta=(ClampMin="0.0", UIMin="0.0"))
	float M_MaxZoomLimit = 4800.f;

	/**
	 * @brief Spring arm length added or removed per zoom input step, in Unreal units.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Camera", meta=(ClampMin="0.0", UIMin="0.0"))
	float M_ZoomSpeed = 150.f;

	/**
	 * @brief Maximum pitch in degrees the player may tilt the camera towards the horizon.
	 * Keep this low enough that the skybox stays out of view.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Camera", meta=(ClampMin="0.0", UIMin="0.0"))
	float M_CameraPitchLimit = 18.f;

	/**
	 * @brief Base speed used by the blueprint driven camera pan, before the player's pan speed multiplier.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Camera", meta=(ClampMin="0.0", UIMin="0.0"))
	float M_CameraPanSpeed = 5.f;

	/**
	 * @brief Base translation speed applied to keyboard driven forward/right camera movement.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Camera", meta=(ClampMin="0.0", UIMin="0.0"))
	float M_DefaultCameraMovementSpeed = 15.f;

	/**
	 * @brief Starting value of the runtime movement speed modifier (the sprint/slow key multiplier).
	 * Gameplay overwrites the live modifier through the player controller; this is only its initial value.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Camera", meta=(ClampMin="0.0", UIMin="0.0"))
	float M_DefaultModifierCameraMovementSpeed = 1.f;

	/**
	 * @brief Builds the cinematic preset: a wider zoom range and a higher pitch limit for capture work.
	 * All other values match the gameplay defaults.
	 */
	static FRTSCameraTuningSettings MakeCinematicDefaults();
};

/**
 * @brief Global project settings that tune the player camera zoom range, zoom speed, pitch limit and movement speeds.
 * The cinematic override lets designers switch the whole camera to a wider, more permissive set of
 * limits for trailers and cutscene capture without editing the gameplay values.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="RTS Camera"))
class RTS_SURVIVAL_API URTSCameraDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URTSCameraDeveloperSettings();

	/**
	 * @brief Makes the camera use the cinematic tuning instead of the gameplay tuning.
	 * This is read once when the game starts, so toggling it mid-session has no effect.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Camera|Cinematic")
	bool bM_UseCinematicCameraSettings = false;

	/**
	 * @brief Camera tuning used during normal gameplay.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Camera|Gameplay")
	FRTSCameraTuningSettings M_GameplayCameraSettings = FRTSCameraTuningSettings();

	/**
	 * @brief Camera tuning used when the cinematic override is enabled.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Camera|Cinematic")
	FRTSCameraTuningSettings M_CinematicCameraSettings = FRTSCameraTuningSettings::MakeCinematicDefaults();

	static const URTSCameraDeveloperSettings* Get();

	/**
	 * @brief Picks the gameplay or cinematic tuning based on the cinematic override flag.
	 * @return The tuning the camera should be driven with this session.
	 */
	const FRTSCameraTuningSettings& GetActiveCameraSettings() const;
};
