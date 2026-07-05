// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "TimerManager.h"
#include "W_CoolDownItem.generated.h"

class UMaterialInstanceDynamic;
class UTexture2D;
class UWorld;

/**
 * Widget button that owns an image dynamic material and tracks a cooldown clock.
 */
UCLASS()
class RTS_SURVIVAL_API UW_CoolDownItem : public UUserWidget
{
	GENERATED_BODY()

public:
	bool GetIsOnCoolDown() const;
	bool GetWasInitialized(const bool bReportError = true) const;
	UButton* GetButton() const;

/**
 * @brief Caches the image MID and initializes the cooldown clock state.
 * @param WeakWorldContext Context used to resolve the world for timers and world-time reads.
 * @param IconTexture Icon texture assigned to the material's IconTexture parameter.
 * @param CooldownSeconds Total cooldown duration for this item. Used by StartCooldown and restored cooldown state.
 * @param bStartOnCooldown If true, starts a fresh cooldown using CooldownSeconds. Ignored when SecondsLeft > 0.
 * @param bStartPaused If true, freezes the cooldown clock immediately after init. Works for fresh and restored cooldowns.
 * @param SecondsLeft Optional restored cooldown time remaining. If greater than 0, the item starts on cooldown with this remaining time clamped to CooldownSeconds.
 */
void Init(
	const TWeakObjectPtr<UObject>& WeakWorldContext,
	UTexture2D* const IconTexture,
	const float CooldownSeconds,
	const bool bStartOnCooldown,
	const bool bStartPaused = false,
	const float SecondsLeft = 0.0f);
	
	float GetCooldownRemaining() const;

	/**
	 * @brief Starts a cooldown without restarting an already active cooldown.
	 * @param CooldownTime Duration used for this cooldown start.
	 */
	void StartCooldown(const float CooldownTime);

	/**
	 * @brief Restores cooldown visuals from saved gameplay state without losing elapsed time.
	 * @param CooldownDurationSeconds Total duration represented by the material clock.
	 * @param CooldownRemainingSeconds Remaining duration to display from this moment.
	 */
	void SetCooldownState(const float CooldownDurationSeconds, const float CooldownRemainingSeconds);

	void InstantlyResetCooldown();

	/**
	 * @brief Pauses or unpauses the material clock and the cooldown state timer.
	 * @param bPause True freezes the clock. False resumes it from the same visual progress.
	 */
	void PauseClock(const bool bPause);

	/**
	 * @brief Changes the stored cooldown duration while preserving active elapsed time when requested.
	 * @param NewCooldown New total cooldown duration in seconds.
	 * @param bResetCooldownState If true, stores the new duration and immediately clears cooldown state.
	 */
	void UpgradeCooldown(const float NewCooldown, const bool bResetCooldownState);

	virtual void BeginDestroy() override;

protected:
	virtual void NativeDestruct() override;

private:
	static const FName S_IconTextureParameterName;
	static const FName S_CooldownStartTimeParameterName;
	static const FName S_CooldownDurationParameterName;
	static const FName S_PauseAlphaParameterName;
	static const FName S_PauseTimeParameterName;

	static constexpr float S_MinimumCooldownDurationSeconds = 0.0001f;
	static constexpr float S_MinimumTimerDelaySeconds = 0.001f;
	static constexpr float S_CooldownStateUpdateIntervalSeconds = 0.05f;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> M_Button;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> M_Image;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> M_DynamicMaterial;

	UPROPERTY(Transient)
	TWeakObjectPtr<UObject> M_WeakWorldContext;

	UPROPERTY(Transient)
	TWeakObjectPtr<UWorld> M_WeakTimerWorld;

	FTimerHandle M_CooldownTimerHandle;

	float M_CooldownDurationSeconds = 0.0f;
	float M_CooldownStartTimeSeconds = 0.0f;
	float M_CooldownRemainingSeconds = 0.0f;
	float M_CooldownPausedTimeSeconds = 0.0f;

	bool bM_WasInitialized = false;
	bool bM_IsOnCooldown = false;
	bool bM_IsClockPaused = false;

	bool CacheDynamicMaterialFromImage();
	bool CacheTimerWorldFromContext();

	void ApplyIconTextureParameter(UTexture2D* const IconTexture) const;
	void ApplyCooldownMaterialActiveState() const;
	void ApplyCooldownMaterialCompletedState() const;
	void ApplyPauseMaterialState() const;

	void CompleteCooldownInternal();

	void StartClockPauseInternal();
	void StopClockPauseInternal();

	void HandleCooldownStateUpdateTimerElapsed();
	void EnsureCooldownStateUpdateTimerIsScheduled();
	void RefreshCooldownTimerForCurrentPauseState();
	void ScheduleNextCooldownStateUpdateTimer();
	void ClearCooldownTimer();

	bool UpdateCooldownStateFromCurrentTime(const bool bReportError = true);
	bool TryGetCurrentWorldTimeSeconds(float& OutCurrentWorldTimeSeconds, const bool bReportError = true) const;
	bool TryGetEffectiveCooldownTimeSeconds(float& OutEffectiveCooldownTimeSeconds, const bool bReportError = true) const;

	float CalculateCooldownRemainingSeconds(const float CurrentWorldTimeSeconds) const;
	float CalculatePauseDurationSeconds(const float CurrentWorldTimeSeconds) const;
	float GetNextCooldownStateUpdateDelaySeconds() const;
	bool GetIsCooldownTimerActive() const;

	UWorld* GetTimerWorld(const bool bReportError = true) const;
	UWorld* GetTimerWorldFromContext(const bool bReportError = true) const;

	bool GetIsValidButton() const;
	bool GetIsValidImage() const;
	bool GetIsValidDynamicMaterial() const;
	bool GetIsValidWorldContext(const bool bReportError = true) const;
	bool GetIsValidTimerWorld(const bool bReportError = true) const;

	void ReportInvalidState(const FString& ErrorMessage) const;
};
