// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Audio/RTSVoiceLineHelpers/RTS_VoiceLineHelpers.h"
#include "SpatialVoiceLinePlayer.generated.h"

enum class ERTSVoiceLine : uint8;
class ACPPController;

/**
 * @brief Plays 3D “spatial” voice lines for units the local player controls.
 *
 * Call PlaySpatialVoiceLine() to request a voice line at Location;
 * the component will throttle repeats and add a random start delay
 * to avoid spam and synchronized playback across multiple units.
 *
 * @note PlaySpatialVoiceLine: call whenever this unit should speak in world.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API USpatialVoiceLinePlayer : public UActorComponent
{
	GENERATED_BODY()

public:
	USpatialVoiceLinePlayer();

	/**
	 * @brief Request a spatial voice line at the given location.
	 * @param VoiceLineType  Which event (Select, Attack, etc.) to play.
	 * @param Location       World-space point to emit the sound.
	 * @param bIgnorePlayerCooldown The player can only have one type of sound of one type of unit at the time; this means that
	 * if a spacial idle voice line is played by a unit somewhere else, other units cannot play idle voice lines until that one is finished.
	 * Setting this to true will ignore that check and play the sound anyway.
	 * @note Difference with ForcePlaySpatialVoiceLine: Force always plays and always overrides the player cooldown.
	 * @note The spatial audio is subject to various checks before allowing it to be played:
	 * @note 1) Cannot play if on cooldown (see developer settings)
	 * @note 2) Probability test against the per-line probability (see developer settings)
	 * @post If the voice line could be played, the spawn audio component is saved in M_LastSpacialAudio.
	 */
	void PlaySpatialVoiceLine(
		ERTSVoiceLine VoiceLineType,
		const FVector& Location, const bool bIgnorePlayerCooldown
	);

	/**
	 * @brief Kills the last spatial audio component if it is valid then plays the voice line on a new spatial audio component.
	 * @param VoiceLineType The voice line to play.
	 * @param Location Where to play the spatial voice line.
	 */
	void ForcePlaySpatialVoiceLine(
		ERTSVoiceLine VoiceLineType,
		const FVector& Location);

	/** @brief The regular way of playing a voice line to the player should only be called when selected. */
	void PlayVoiceLineOverRadio(
		ERTSVoiceLine VoiceLineType,
		const bool bForcePlay,
		const bool bQueueIfNotPlayed) const;

protected:
	virtual void BeginPlay() override;

	/**
     * @brief Roll a random test against the per-line probability.
     * @param VoiceLineType  The type of spatial line being requested.
     * @return true if the random roll passes and the line may proceed.
     */
	virtual bool ShouldPlaySpatialVoiceLine(ERTSVoiceLine VoiceLineType) const;

	virtual float GetBaseIntervalBetweenSpatialVoiceLines(float& OutFluxPercentage) const;

	/** Check that this component should play spatial audio (owner owned by local player). */
	virtual bool BeginPlay_CheckEnabledForSpatialAudio();

	virtual void OverrideAttenuation(UAudioComponent* AudioComp) const ;
	
	/** Disable all spatial audio if not owned by player. */
	bool bIsEnabledForSpatialAudio = false;

	// If the owning player is 1 this is the attenuation settings that will be used.
	// Can be left null in which case the default attenuation from the player controller is used.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	USoundAttenuation* PlayerSoundUnitAttenuationSettings;
	// If the owning player is 2 this is the attenuation settings that will be used.
	// Can be left null in which case the default attenuation from the player controller is used.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	USoundAttenuation* EnemySoundUnitAttenuationSettings;

private:
	/** True while waiting out the “spam” cooldown interval. */
	bool bIsOnSpatialCooldown = false;
	
	void HandleNewSpatialAudio(UAudioComponent* SpatialAudio);

	/** Reference to the player’s controller for routing the play call. */
	TWeakObjectPtr<ACPPController> M_PlayerController;

	/** Handle for the spam‐cooldown timer. */
	FTimerHandle TimerHandle_SpatialCooldown;

	/** Handle for the randomized stagger timer before actual play. */
	FTimerHandle TimerHandle_SpatialStagger;


	/** Cache the ACPPController pointer once at BeginPlay. */
	void BeginPlay_SetupControllerReference();

	/** Ensure the cached controller pointer is still valid. */
	bool EnsureControllerIsValid() const;

	/** Called when the cooldown period expires, allowing next play. */
	void ResetSpatialCooldown();

	// Populated with reference to the last created audio component for spatial audio if that could be played.
	UPROPERTY()
	TWeakObjectPtr<UAudioComponent> M_LastSpacialAudio;

	/**
	 * @brief After the stagger delay, actually invoke the controller’s spatial play.
	 * @param VoiceLineType  The event to play.
	 * @param Location       Where to play it.
	 * @param bIgnorePlayerCooldown
	 * @return The audio component if the voice line could be played.
	 */
	UFUNCTION()
	UAudioComponent* ExecuteSpatialVoiceLine(ERTSVoiceLine VoiceLineType, const FVector& Location, bool bIgnorePlayerCooldown);

	void Debug_SpatialNotAllowed(const ERTSVoiceLine& VoiceLine, const FVector& Location) const;
	void Debug_SpatialAllowed(const ERTSVoiceLine& VoiceLine, const FVector& Location) const;
	void Debug_ForceSpatialAudio(const ERTSVoiceLine& VoiceLine, const FVector& Location) const;

	// Spatial attenuation override.
	void BeginPlay_SetupAttenuationOverride();


	void BeginPlay_ReportErrorIfnoAttenuation() const;
	
	// Set depending on the owning player.
	UPROPERTY()
	USoundAttenuation* OverrideAttenuationSettings = nullptr;

};
