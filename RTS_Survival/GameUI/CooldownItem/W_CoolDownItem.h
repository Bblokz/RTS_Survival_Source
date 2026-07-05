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

	/**
	 * @brief Caches the image MID and optionally starts the cooldown immediately.
	 * @param WeakWorldContext Context used to resolve the world for timers and world-time reads.
	 * @param IconTexture Icon texture assigned to the material's IconTexture parameter.
	 * @param CooldownSeconds Default cooldown duration used by this item.
	 * @param bStartOnCooldown Whether this widget should immediately start cooling down.
	 */
	void Init(
		const TWeakObjectPtr<UObject>& WeakWorldContext,
		UTexture2D* const IconTexture,
		const float CooldownSeconds,
		const bool bStartOnCooldown);

	float GetCooldownRemaining() const;

	/**
	 * @brief Starts a cooldown without restarting an already active cooldown.
	 * @param CooldownTime Duration used for this cooldown start.
	 */
	void StartCooldown(const float CooldownTime);

	void InstantlyResetCooldown();

	/**
	 * @brief Changes the stored cooldown duration while preserving active elapsed time when requested.
	 * @param NewCooldown New total cooldown duration in seconds.
	 * @param bResetCooldownState If true, stores the new duration and immediately clears cooldown state.
	 */
	void UpgradeCooldown(const float NewCooldown, const bool bResetCooldownState);

protected:
	virtual void NativeDestruct() override;
	virtual void BeginDestroy() override;

private:
	static const FName S_IconTextureParameterName;
	static const FName S_CooldownStartTimeParameterName;
	static const FName S_CooldownDurationParameterName;

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

	bool bM_WasInitialized = false;
	bool bM_IsOnCooldown = false;

	bool CacheDynamicMaterialFromImage();
	bool CacheTimerWorldFromContext();

	void ApplyIconTextureParameter(UTexture2D* const IconTexture) const;
	void ApplyCooldownMaterialActiveState() const;
	void ApplyCooldownMaterialCompletedState() const;

	void CompleteCooldownInternal();
	void HandleCooldownStateUpdateTimerElapsed();
	void EnsureCooldownStateUpdateTimerIsScheduled();
	void ScheduleNextCooldownStateUpdateTimer();
	void ClearCooldownTimer();

	bool UpdateCooldownStateFromCurrentTime(const bool bReportError = true);
	bool TryGetCurrentWorldTimeSeconds(float& OutCurrentWorldTimeSeconds, const bool bReportError = true) const;

	float CalculateCooldownRemainingSeconds(const float CurrentWorldTimeSeconds) const;
	float GetNextCooldownStateUpdateDelaySeconds() const;
	bool GetIsCooldownTimerActive() const;

	UWorld* GetTimerWorld(const bool bReportError = true) const;
	UWorld* GetTimerWorldFromContext(const bool bReportError = true) const;

	bool GetWasInitialized(const bool bReportError = true) const;
	bool GetIsValidButton() const;
	bool GetIsValidImage() const;
	bool GetIsValidDynamicMaterial() const;
	bool GetIsValidWorldContext(const bool bReportError = true) const;
	bool GetIsValidTimerWorld(const bool bReportError = true) const;

	void ReportInvalidState(const FString& ErrorMessage) const;
};
