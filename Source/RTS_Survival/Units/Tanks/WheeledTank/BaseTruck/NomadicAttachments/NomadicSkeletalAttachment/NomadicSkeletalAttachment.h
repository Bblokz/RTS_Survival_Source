// NomadicSkeletalAttachment.h
#pragma once

#include "AttachmentAnimationState.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicAttachments/NomadicAttachment.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundConcurrency.h"
#include "Components/AudioComponent.h"

#include "NomadicSkeletalAttachment.generated.h"

/* ======================================================================== */
/* Skeletal variant: Montages + AimOffset Yaw + Centralized Sound Control   */
/* ======================================================================== */

/**
 * @brief Minimal AnimInstance the AnimBP must derive from. Supplies AO yaw.
 * @note StartSkeletalNomadicOn: call in Blueprint to start the behavior on a skeletal mesh.
 * @note StopSkeletalNomadic: call in Blueprint to stop/reset the behavior.
 *
 * Your AnimGraph should feed its Aim Offset "Yaw" from NomadicYawDeg.
 * Put the AO BEFORE the full-body montage slot.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API UNomadicAttachmentAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
public:
	// Current desired yaw (degrees) that drives the Aim Offset Yaw parameter.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nomadic")
	float NomadicYawDeg = 0.f;
};

/** One entry for a candidate montage, optionally with a sound to play alongside it. */
USTRUCT(BlueprintType)
struct FNomadicMontageEntry
{
	GENERATED_BODY()

	// The montage to play when this entry is chosen.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nomadic Attachment|Skeletal")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	// Optional sound to play when the montage begins (fades from previous).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nomadic Attachment|Skeletal|Audio")
	TObjectPtr<USoundBase> MontageSound = nullptr;
};

/** Top-level tunables for the skeletal nomadic behavior. */
USTRUCT(BlueprintType)
struct FNomadicMontageSettings
{
	GENERATED_BODY()

	// Minimum delay (seconds) between actions (AO or montage).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nomadic Attachment|Skeletal")
	float MinDelayBetweenMontages = 0.5f;

	// Maximum delay (seconds) between actions (AO or montage).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nomadic Attachment|Skeletal")
	float MaxDelayBetweenMontages = 1.5f;

	// Candidate montage entries (each may carry its own optional sound).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nomadic Attachment|Skeletal")
	TArray<FNomadicMontageEntry> MontageEntries;

	// Chance [0..1] to play a montage; otherwise do custom idle rotation via AO yaw.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nomadic Attachment|Skeletal", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MontagePlayChance01 = 0.5f;

	// Name of a reference bone whose current yaw we cache as our "TPose" baseline.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nomadic Attachment|Skeletal")
	FName RotationBoneName = NAME_None;

	// Minimum absolute degrees for a custom idle rotation (applied to AO yaw).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nomadic Attachment|Skeletal", meta=(ClampMin="0.0"))
	float MinCustomRotationDeg = 10.f;

	// Maximum absolute degrees for a custom idle rotation (applied to AO yaw).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nomadic Attachment|Skeletal", meta=(ClampMin="0.0"))
	float MaxCustomRotationDeg = 45.f;

	// Whether to apply negative rotations (left) when doing custom idle rotations.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nomadic Attachment|Skeletal")
	bool bAllowNegativeCustomRotations = true;

	// Rotation speed (deg/sec) for custom yaw and returning to the baseline.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nomadic Attachment|Skeletal", meta=(ClampMin="0.0"))
	float CustomRotationSpeedDegPerSec = 90.f;

	// AO "start" sound that is intentionally NON-looping; it should be restarted at each new AO animation.
	// (Renamed from AOLoopingSound for clarity.)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nomadic Attachment|Skeletal|Audio")
	TObjectPtr<USoundBase> AONonLoopingSound = nullptr;

	// Idle sound to fade to when becoming idle. If not set, we fade-out current and stay silent.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nomadic Attachment|Skeletal|Audio")
	TObjectPtr<USoundBase> IdleSound = nullptr;

	// Fade time (seconds) used when cross-fading between sounds on the internal audio component.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nomadic Attachment|Skeletal|Audio", meta=(ClampMin="0.0"))
	float SoundFadeSeconds = 0.15f;

	// Global audio settings that govern ALL sounds played by ANomadicAttachmentSkeletal.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nomadic Attachment|Skeletal|Audio")
	TObjectPtr<USoundAttenuation> SoundAttenuation = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nomadic Attachment|Skeletal|Audio")
	TObjectPtr<USoundConcurrency> SoundConcurrency = nullptr;

	// Initial animation mode (Montage+AO, AO-only, Montage-only).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nomadic Attachment|Skeletal")
	EAttachmentAnimationState InitialAnimationState = EAttachmentAnimationState::MontageAndAO;

	// When the crane is set to AO-only, this is added (in seconds) to the base delay window.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Nomadic Attachment|Skeletal")
	float AOOnly_ExtraDelayBetweenAnimations = 0.f;
};

UENUM()
enum class ENomadicSkeletalState : uint8
{
	Inactive,
	Waiting,
	PlayingMontage,
	RotatingToTarget,   // AO yaw -> target
	ReturningToTPose,   // AO yaw -> base
	HoldingFixedAO      // AO-only average-hold mode → keep orientation
};

/**
 * @brief Actor that drives a skeletal mesh with random AO yaw glances and/or montages, with unified audio control.
 * @note StartSkeletalNomadicOn: call in blueprint with mesh, anim instance and settings to start.
 * @note StopSkeletalNomadic: call in blueprint to stop and reset.
 */
UCLASS()
class RTS_SURVIVAL_API ANomadicAttachmentSkeletal : public ANomadicAttachment
{
	GENERATED_BODY()

public:
	ANomadicAttachmentSkeletal();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

public:
	/**
	 * @brief Start the skeletal flow on a mesh using the provided AnimBP and settings.
	 * @param SkeletalMesh The mesh component to drive.
	 * @param AnimInstance AnimBP instance (must derive from UNomadicAttachmentAnimInstance).
	 * @param Settings Tunables for montage/AO behavior and audio.
	 */
	UFUNCTION(BlueprintCallable, Category="Nomadic Attachment|Skeletal")
	void StartSkeletalNomadicOn(USkeletalMeshComponent* SkeletalMesh, UNomadicAttachmentAnimInstance* AnimInstance, const FNomadicMontageSettings& Settings);

	/** @brief Stop the skeletal flow, clear delegates, reset AO to baseline, stop audio (fade). */
	UFUNCTION(BlueprintCallable, Category="Nomadic Attachment|Skeletal")
	void StopSkeletalNomadic();

	/** @brief Set allowed animation mode (Montage+AO, AO-only, Montage-only). */
	UFUNCTION(BlueprintCallable, Category="Nomadic Attachment|Skeletal")
	void SetAttachmentAnimationState(EAttachmentAnimationState NewState);

	/** Enable/disable AO-only average-hold behavior at runtime. */
	UFUNCTION(BlueprintCallable, Category="Nomadic Attachment|Skeletal")
	void SetOnAOOnlyMoveToAverageCustomRotation(bool bEnable);

	/** Force-stop any playing/scheduled montage and go AOOnly cleanly (used at end-of-item). */
	UFUNCTION(BlueprintCallable, Category="Nomadic Attachment|Skeletal")
	void ForceAOOnly_StopAnyMontageImmediately();

	/** Enable Montage+AO cleanly for next item; clear stale pending + AO-only holds. */
	UFUNCTION(BlueprintCallable, Category="Nomadic Attachment|Skeletal")
	void EnableMontageAndAO_FlushIfHoldingAOOnly();

private:
	// ===== Validation / helpers (rule 0.5: centralize validity + logging) =====
	bool GetIsValidSkeletalMesh() const;
	bool GetIsValidNomadicAnimInstance() const;
	bool GetIsValidSoundComponent() const;

	bool TryCacheBaseAimYaw();
	float GetCurrentAOYawDeg() const;
	void  SetAOYawDeg(float NewYawDeg) const;

	// ===== Flow =====
	void DecideNextAction();
	bool TryPickRandomMontage(UAnimMontage*& OutPickedMontage, USoundBase*& OutPickedSound) const;
	bool TryPlayMontage(UAnimMontage* MontageToPlay);

	void StartRandomAORotation();
	void StartReturnToTPose();
	void StepYawTowardTarget(float DeltaSeconds, float SpeedDegPerSec);
	void BeginWaitRandomDelay();
	void ClearMontageBinding();

	// ===== AO-only average-hold helpers =====
	bool IsAOOnlyAverageHoldEnabled() const;
	float ComputeAOOnlyAverageTargetYaw() const;
	/** Ensures we are rotating to the AO-only average target or holding it. Returns true if a new rotation started. */
	bool EnsureAOOnlyAverageHoldTarget(bool bPlaySoundIfStarted);

	// ===== Audio (single audio component, centralized control) =====
	void SetupOrRefreshAudioComponentIfNeeded();
	bool HasAnyConfiguredSound() const;
	void Audio_FadeToSound(USoundBase* NewSound) const;
	void Audio_FadeOutOnly() const;

	// Non-looping AO start: ALWAYS restart when a new AO rotation begins.
	void Audio_PlayAONonLoopingStart() const;

	void Audio_PlayIdle() const;
	void Audio_PlayMontageSound(USoundBase* MontageSound) const;

	// Montage-ended callback
	UFUNCTION()
	void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);

private:
	// Skeletal mesh we operate on.
	UPROPERTY()
	TWeakObjectPtr<USkeletalMeshComponent> M_SkeletalMesh = nullptr;

	// Anim instance (your AnimBP) we drive the AO yaw on and read audio settings from.
	UPROPERTY()
	TWeakObjectPtr<UNomadicAttachmentAnimInstance> M_AnimInstance = nullptr;

	// Our single audio component that plays AO starts, montage sounds, and idle. Created on-demand.
	UPROPERTY(Transient)
	TWeakObjectPtr<UAudioComponent> M_SoundComponent = nullptr;

	// Settings for this flow.
	FNomadicMontageSettings M_MontageSettings;

	// State machine
	ENomadicSkeletalState M_SkeletalState = ENomadicSkeletalState::Inactive;

	// Countdown for waits.
	float M_TimeUntilNextAction = 0.f;

	// Cached "TPose" baseline yaw (deg).
	float M_BaseAimYawDeg = 0.f;

	// Current target yaw for AO (deg).
	float M_TargetAimYawDeg = 0.f;

	// After finishing a custom rotation, we first wait, then return to base.
	bool bM_PendingReturnToBaseAfterWait = false;

	// If true, we're returning to baseline specifically to play a chosen montage next.
	bool bM_ReturningForMontage = false;

	// The montage we already decided to play right after returning to baseline.
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> M_PendingMontage = nullptr;

	// If a montage entry had a sound, keep it until we actually start playing that montage.
	UPROPERTY(Transient)
	TObjectPtr<USoundBase> M_PendingMontageSound = nullptr;

	// Montage delegate bookkeeping.
	bool bM_MontageBound = false;

	// Whether audio system (component + settings) is prepared; we only attempt audio if true.
	bool bM_SoundReady = false;

	// Defines what type of animations are allowed to be played.
	EAttachmentAnimationState M_AnimationState = EAttachmentAnimationState::MontageAndAO;

	// AO-only mode enhancement toggle. When true and in AO-only mode:
	// rotate to the average of Min/MaxCustomRotationDeg and keep that orientation,
	// until we are no longer in AO-only mode. Does not affect Montage+AO mode.
	bool bM_OnAOOnlyMoveToAverageCustomRotation = false;

	// ---- Delay vs rotation magnitude (kept from previous fix) ----
	float GetMinDelay_StateAdjusted() const;   // seconds
	float GetMaxDelay_StateAdjusted() const;   // seconds

	// Rotation magnitudes in degrees for AO glances (never seconds).
	float GetMinAORotationMagnitudeDeg() const;
	float GetMaxAORotationMagnitudeDeg() const;

	void Debug(float DeltaTime);
};
