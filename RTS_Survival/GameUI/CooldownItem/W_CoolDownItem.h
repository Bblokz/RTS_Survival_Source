// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "TimerManager.h"
#include "W_CoolDownItem.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMaterialParameterCollection;
class UTexture2D;
class UWorld;

/**
 * Widget button that owns a dynamic UI material and drives a cooldown radial wipe.
 */
UCLASS()
class RTS_SURVIVAL_API UW_CoolDownItem : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Cooldown")
	bool GetIsOnCoolDown() const;

	/**
	 * @brief Caches the image MID and synchronizes cooldown state with the shader time source.
	 * @param CooldownMaterial UI-domain material using the cooldown shader. Pass nullptr to use the image's current brush material.
	 * @param IconTexture Optional icon texture assigned to the material's IconTexture parameter. Pass nullptr to keep the material default.
	 * @param CurrentTimeParameterCollection MPC asset containing the same time scalar used by the material graph.
	 * @param CurrentTimeParameterName Scalar parameter name inside the MPC, usually CurrentTimeSeconds.
	 * @param CooldownSeconds Total cooldown duration in seconds.
	 * @param CooldownClockColor Overlay color and alpha used by the material's CooldownClockColor parameter.
	 * @param ClockEdgeSoftness01 Softness of the clock edge in normalized angle units.
	 * @param ClockStartOffset01 Clock start offset in full turns; 0 starts at 12 o'clock.
	 * @param AspectRatio Width divided by height for non-square icons.
	 * @param bStartOnCooldown Whether this widget should immediately start covered.
	 * @param WeakWorldContext Weak context used for timer management and MPC lookup.
	 * @return None.
	 */
	void Init(
		UMaterialInterface* const CooldownMaterial,
		UTexture2D* const IconTexture,
		UMaterialParameterCollection* const CurrentTimeParameterCollection,
		const FName& CurrentTimeParameterName,
		const float CooldownSeconds,
		const FLinearColor& CooldownClockColor,
		const float ClockEdgeSoftness01,
		const float ClockStartOffset01,
		const float AspectRatio,
		const bool bStartOnCooldown,
		const TWeakObjectPtr<UObject>& WeakWorldContext);

	UFUNCTION(BlueprintPure, Category = "Cooldown")
	float GetCooldownRemaining() const;

	UFUNCTION(BlueprintCallable, Category = "Cooldown")
	void InstantlyResetCooldown();

	/**
	 * @brief Changes cooldown duration without restarting an active cooldown unless requested.
	 * @param NewCooldown New total cooldown duration in seconds.
	 * @param bResetCooldownState If true, stores the new duration and immediately finishes the cooldown.
	 * @return None.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cooldown")
	void UpgradeCooldown(const float NewCooldown, const bool bResetCooldownState);

protected:
	virtual void NativeDestruct() override;

private:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> M_Button;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> M_Image;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> M_DynamicMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialParameterCollection> M_CurrentTimeParameterCollection;

	UPROPERTY(Transient)
	TWeakObjectPtr<UObject> M_WeakWorldContext;

	UPROPERTY(Transient)
	TWeakObjectPtr<UWorld> M_WeakTimerWorld;

	FTimerHandle M_CooldownTimerHandle;

	FName M_CurrentTimeParameterName = TEXT("CurrentTimeSeconds");

	float M_CooldownDurationSeconds = 0.0f;
	float M_CooldownStartTimeSeconds = 0.0f;
	FLinearColor M_CooldownClockColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.65f);
	float M_ClockEdgeSoftness01 = 0.003f;
	float M_ClockStartOffset01 = 0.0f;
	float M_AspectRatio = 1.0f;

	bool bM_IsOnCooldown = false;

	bool CacheDynamicMaterial(UMaterialInterface* const CooldownMaterial);
	bool CacheTimerWorldFromContext();

	void ApplyMaterialSetupParameters(UTexture2D* const IconTexture) const;
	void ApplyCooldownMaterialActiveState() const;
	void ApplyCooldownMaterialCompletedState() const;
	void ApplyCooldownMaterialDisabledState() const;

	void BeginCooldownInternal();
	void HandleCooldownFinished();
	void ScheduleCooldownTimer();
	void ClearCooldownTimer();

	bool TryGetCurrentGameTimeSeconds(float& OutCurrentGameTimeSeconds, const bool bReportError = true) const;
	UWorld* GetTimerWorld(const bool bReportError = true) const;
	UWorld* GetTimerWorldFromContext(const bool bReportError = true) const;

	bool GetIsValidButton() const;
	bool GetIsValidImage() const;
	bool GetIsValidDynamicMaterial() const;
	bool GetIsValidCurrentTimeParameterCollection(const bool bReportError = true) const;
	bool GetIsValidCurrentTimeParameterName(const bool bReportError = true) const;
	bool GetIsValidWorldContext(const bool bReportError = true) const;
	bool GetIsValidTimerWorld(const bool bReportError = true) const;

	void ReportInvalidState(const FString& ErrorMessage) const;
};
