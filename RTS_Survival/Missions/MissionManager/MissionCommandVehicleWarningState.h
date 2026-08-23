#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GameUI/Portrait/RTSPortraitTypes.h"
#include "MissionCommandVehicleWarningState.generated.h"

class ATankMaster;
class UAudioComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class USoundAttenuation;
class USoundBase;
class USoundConcurrency;

/**
 * @brief Lets mission designers pair a portrait with a shuffled cycle of warning voice lines.
 * An empty voice-line array disables portrait playback without affecting the other warning effects.
 */
USTRUCT(BlueprintType)
struct FMissionCommandVehicleWarningPortraitSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Command Vehicle Warning")
	ERTSPortraitTypes PortraitType = ERTSPortraitTypes::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Command Vehicle Warning")
	TArray<TObjectPtr<USoundBase>> VoiceLineOptions;
};

/**
 * @brief Owns the single persistent command-vehicle warning session used by the mission manager.
 * Runtime timing, shuffled portrait order, tracked assets, and temporary effects stay together.
 */
USTRUCT()
struct FMissionCommandVehicleWarningState
{
	GENERATED_BODY()

	/**
	 * @brief Replaces the current session atomically so repeated starts cannot create parallel trackers.
	 * @param CommandVehicle Player command vehicle whose health drives the warning.
	 * @param HealthPercentageTrigger Health fraction below which a warning may trigger.
	 * @param TimeBetweenTriggers Minimum number of seconds between warnings while health remains low.
	 * @param PortraitSettings Portrait type and voice-line options retained beyond the calling mission lifetime.
	 * @param OneShotVFX Optional attached Niagara one-shot.
	 * @param CommanderOffset Relative attachment offset shared by the sound and Niagara effect.
	 * @param VFXScale Relative scale applied to the Niagara effect.
	 * @param OneShotSound Optional spatial sound attached to the command vehicle.
	 * @param SoundAttenuation Optional attenuation settings for OneShotSound.
	 * @param SoundConcurrency Optional concurrency settings for OneShotSound.
	 */
	void Init(
		ATankMaster* CommandVehicle,
		float HealthPercentageTrigger,
		float TimeBetweenTriggers,
		const FMissionCommandVehicleWarningPortraitSettings& PortraitSettings,
		UNiagaraSystem* OneShotVFX,
		const FVector& CommanderOffset,
		const FVector& VFXScale,
		USoundBase* OneShotSound,
		USoundAttenuation* SoundAttenuation,
		USoundConcurrency* SoundConcurrency);

	void Reset();
	void CleanupTransientEffects();
	/**
	 * @brief Accepts only an armed below-threshold transition after the cooldown has fully elapsed.
	 * @param CurrentHealthPercentage Current command-vehicle health expressed from zero to one.
	 * @param CurrentTimeSeconds Current world time used to reject transitions during cooldown.
	 * @return True only when this update starts a new warning trigger.
	 */
	bool TryBeginTrigger(float CurrentHealthPercentage, double CurrentTimeSeconds);
	USoundBase* GetNextPortraitVoiceLine();

	bool GetIsActive() const { return bM_IsActive; }
	ATankMaster* GetCommandVehicle() const { return M_CommandVehicle.Get(); }
	ERTSPortraitTypes GetPortraitType() const { return M_PortraitType; }
	UNiagaraSystem* GetOneShotVFX() const { return M_OneShotVFX.Get(); }
	USoundBase* GetOneShotSound() const { return M_OneShotSound.Get(); }
	USoundAttenuation* GetSoundAttenuation() const { return M_SoundAttenuation.Get(); }
	USoundConcurrency* GetSoundConcurrency() const { return M_SoundConcurrency.Get(); }
	const FVector& GetCommanderOffset() const { return M_CommanderOffset; }
	const FVector& GetVFXScale() const { return M_VFXScale; }
	void SetWarningAudioComponent(UAudioComponent* AudioComponent);
	void SetWarningNiagaraComponent(UNiagaraComponent* NiagaraComponent);

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<ATankMaster> M_CommandVehicle;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundBase>> M_ShuffledPortraitVoiceLines;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> M_OneShotVFX = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> M_OneShotSound = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> M_SoundAttenuation = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USoundConcurrency> M_SoundConcurrency = nullptr;

	UPROPERTY(Transient)
	TWeakObjectPtr<UAudioComponent> M_WarningAudioComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<UNiagaraComponent> M_WarningNiagaraComponent;

	UPROPERTY(Transient)
	FVector M_CommanderOffset = FVector::ZeroVector;

	UPROPERTY(Transient)
	FVector M_VFXScale = FVector::OneVector;

	UPROPERTY(Transient)
	ERTSPortraitTypes M_PortraitType = ERTSPortraitTypes::None;

	UPROPERTY(Transient)
	float M_HealthPercentageTrigger = 0.0f;

	UPROPERTY(Transient)
	float M_TimeBetweenTriggers = 0.0f;

	double M_NextAllowedTriggerTimeSeconds = 0.0;
	int32 M_NextPortraitVoiceLineIndex = 0;

	UPROPERTY(Transient)
	bool bM_IsActive = false;

	// A completed cooldown only re-arms after health is observed back at or above the threshold.
	UPROPERTY(Transient)
	bool bM_IsArmedForLowHealthTrigger = false;
};
