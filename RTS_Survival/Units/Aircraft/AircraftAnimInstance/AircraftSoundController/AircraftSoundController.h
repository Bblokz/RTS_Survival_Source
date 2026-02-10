#pragma once
#include "CoreMinimal.h"
#include "AircraftSoundController.generated.h"

enum class EAircraftLandingState : uint8;
class AAircraftMaster;
class UAircraftAnimInstance;
class UAudioComponent;
class USoundBase;
class USoundConcurrency;
class USoundAttenuation;

/**
 * @brief Controls aircraft engine audio and (new) airborne fly-by one-shots.
 *        Creates persistent audio components (no spawn/destroy churn) and uses timers to update
 *        engine parameters and to schedule fly-by stingers while airborne.
 * @note InitEngineSound: call during aircraft/anim setup to create & configure components.
 */
USTRUCT(Blueprintable)
struct FAircraftSoundController
{
	GENERATED_BODY()

	// -------------------- Config  --------------------
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	USoundBase* FlightEngineSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	USoundBase* LandedEngineSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	USoundBase* VTOPrepSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	USoundBase* VTOSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	USoundBase* LandingSound = nullptr;

	// Random fly-by sounds to play when the aircraft is near the player.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<USoundBase*> RandomFlyBySounds = {};

	// Defines a min max range for the random float for how often we play a fly-by sound.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FVector2D MinMaxTimeBetweenFlyBySounds = FVector2D(10.f, 20.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	USoundConcurrency* EngineSoundConcurrency = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	USoundAttenuation* EngineSoundAttenuation = nullptr;

	// -------------------- Public API  --------------------
	void InitEngineSound(AAircraftMaster* OwningAircraft, UAircraftAnimInstance* OwningAnimInst);

	void PlayVto_Prep(UAircraftAnimInstance* OwningAircraftAnimator, float VtoPrepTime);
	void PlayVto_Start(UAircraftAnimInstance* OwningAircraftAnimator, float TotalVTOTime);
	void PlayVto_Landing(UAircraftAnimInstance* OwningAircraftAnimator, float VtoLandingTime);
	void Play_Airborne(UAircraftAnimInstance* OwningAircraftAnimator);
	void Play_Landed(UAircraftAnimInstance* OwningAircraftAnimator);

private:
	// ---------- Engine sound internals ----------
	FTimerHandle M_EngineSoundUpdate;

	UPROPERTY()
	UAudioComponent* M_EngineSoundSystem = nullptr;

	FName AudioSpeedParam = "Speed";
	bool bIsTimerActive = false;

	// Used at init to start with the correct sound.
	void PlaySoundForAircraftState(EAircraftLandingState LandingState, UAircraftAnimInstance* OwningAnimInst);
	bool EnsureIsValidEngineSystem() const;
	bool EnsureSoundIsValid(const USoundBase* SoundToCheck, const FString& SoundName) const;
	bool GetIsValidEngineSystem() const;
	bool EnsureValidAnimState(const UAircraftAnimInstance* OwningAircraftAnimator) const;
	void UpdateEngineSound(float Speed) const;
	void CancelTimerIfActive(UAircraftAnimInstance* OwningAircraftAnimator);

	/**
	 * @brief Compute how many seconds to skip (jump ahead) so that the remaining playtime equals TotalWindowSeconds.
	 * @param Sound Sound to analyze (must be valid).
	 * @param TotalWindowSeconds Target window duration to fit inside.
	 * @return Start offset in seconds (>= 0). If the sound is shorter than the window, returns 0 and reports.
	 */
	float ComputeJumpAheadTime(const USoundBase* Sound, float TotalWindowSeconds) const;

	// ==================== Fly-By one-shots  ====================

	/** @brief Track current and start indices for shuffle-like traversal. */
	struct FFlyByShuffleState
	{
		int32 StartIndex = INDEX_NONE;
		int32 CurrentIndex = INDEX_NONE;

		void Reset()
		{
			StartIndex = INDEX_NONE;
			CurrentIndex = INDEX_NONE;
		}
	};

	// Persistent fly-by audio component (same Attenuation/Concurrency as engine).
	UPROPERTY()
	UAudioComponent* M_FlyBySoundSystem = nullptr;


	// Owning anim instance used for safe lambda callbacks into this controller.
	TWeakObjectPtr<UAircraftAnimInstance> M_OwningAircraftAnimInstance;
	// World cache for timers (set in Init).
	TWeakObjectPtr<UWorld> M_World;

	// Timers for scheduling next play and for deactivating after clip end.
	FTimerHandle M_FlyByTimer;
	FTimerHandle M_FlyByDeactivateTimer;

	// State & flags
	FFlyByShuffleState M_FlyByShuffleState;
	bool bM_FlyByTimerActive = false;
	bool bM_FlyByDeactivateTimerActive = false;

	// ----- Fly-by setup & lifecycle -----

	/**
	 * @brief Create/configure the persistent fly-by component (attached to aircraft root).
	 * @param OwningAircraft The aircraft actor that will own the component.
	 */
	void FlyBySound_Init(AAircraftMaster* OwningAircraft);

	/**
	 * @brief Called when aircraft becomes airborne: start scheduling fly-by sounds.
	 * @param OwningAircraftAnimator Used to validate world context.
	 */
	void FlyBySound_OnAirborne(UAircraftAnimInstance* OwningAircraftAnimator);

	/**
	 * @brief Called when aircraft is no longer airborne: stop sound and clear timers.
	 * @param OwningAircraftAnimator Used to validate world context.
	 */
	void FlyBySound_OnNotAirborne(UAircraftAnimInstance* OwningAircraftAnimator);

	// ----- Fly-by helpers -----

	/** @brief Schedule the next fly-by attempt after a random delay in MinMaxTimeBetweenFlyBySounds. */
	void FlyBySound_ScheduleNext();

	/** @brief Play the next sound (advance indices + play + schedule post actions). */
	void FlyBySound_PlayNext();

	/** @brief Deactivate/stop the fly-by component after the current clip ends. */
	void FlyBySound_DeactivateAfterPlay();

	/** @brief Choose next index, maintaining StartIndex/CurrentIndex semantics with reshuffle on wrap. */
	int32 FlyBySound_GetNextIndex();

	/** @brief Random delay within MinMaxTimeBetweenFlyBySounds (clamped sane). */
	float FlyBySound_PickRandomDelay() const;

	/** @brief All preconditions to play (component + array + world). */
	bool FlyBySound_CanPlay() const;

	/** @brief Validate fly-by component; logs error if invalid. */
	bool EnsureIsValidFlyBySystem() const;

	/** @brief True if fly-by component exists. */
	bool GetIsValidFlyBySystem() const;

	/** @brief Clear the schedule timer if active. */
	void FlyBySound_ClearTimerIfActive();

	/** @brief Clear the deactivate timer if active. */
	void FlyBySound_ClearDeactivateTimerIfActive();
};
