#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_HoldToSkip.generated.h"

class UProgressBar;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHoldCompleted);

USTRUCT()
struct FHoldToSkipRuntimeState
{
	GENERATED_BODY()

	float M_CurrentHoldTime = 0.0f;
	float M_CurrentHoldDelayTime = 0.0f;
	float M_DisplayProgress = 0.0f;
	float M_IdleBlinkElapsedTime = 0.0f;
	bool bM_IsHolding = false;
	bool bM_HasCompletedInCurrentHold = false;
};

/**
 * @brief Reusable hold-to-confirm widget driven by external input calls.
 * Use StartHold and StopHold from controllers or other systems to trigger a generic confirmation flow.
 */
UCLASS()
class RTS_SURVIVAL_API UW_HoldToSkip : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintCallable, Category="Hold To Confirm")
	void StartHold();

	UFUNCTION(BlueprintCallable, Category="Hold To Confirm")
	void StopHold();

	UFUNCTION(BlueprintCallable, Category="Hold To Confirm")
	void ResetProgress();

	/**
	 * @brief Keeps progress behavior stable when callers override the default hold duration.
	 * @param InDuration Target duration in seconds. Values <= 0 are clamped to a safe minimum.
	 */
	UFUNCTION(BlueprintCallable, Category="Hold To Confirm")
	void SetHoldDuration(float InDuration);

	UPROPERTY(BlueprintAssignable, Category="Hold To Confirm")
	FOnHoldCompleted OnHoldCompleted;

private:
	bool GetIsValidProgressBar() const;
	void UpdateHoldProgress(const float InDeltaTime);
	void UpdateProgressBarVisual(const float InDeltaTime, const float InTargetProgress);
	void HandleHoldCompleted();
	float GetNormalizedProgress() const;
	void UpdateIdlePromptBlink(const float InDeltaTime);
	void SetPromptOpacity(const float InOpacity) const;

private:
	UPROPERTY(meta=(BindWidget), meta=(AllowPrivateAccess="true"), BlueprintReadOnly, Category="Hold To Confirm")
	TObjectPtr<UProgressBar> M_ProgressBar = nullptr;

	UPROPERTY(meta=(BindWidgetOptional), meta=(AllowPrivateAccess="true"), BlueprintReadOnly, Category="Hold To Confirm")
	TObjectPtr<UTextBlock> M_HoldPromptText = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Hold To Confirm", meta=(AllowPrivateAccess="true", ClampMin="0.01"))
	float M_HoldDuration = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category="Hold To Confirm", meta=(AllowPrivateAccess="true", ClampMin="0.0"))
	float M_HoldStartDelay = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category="Hold To Confirm", meta=(AllowPrivateAccess="true", ClampMin="0.1"))
	float M_ProgressInterpSpeed = 12.0f;

	UPROPERTY(EditDefaultsOnly, Category="Hold To Confirm", meta=(AllowPrivateAccess="true"))
	FText M_DefaultPromptText = FText::FromString(TEXT("Hold to confirm"));

	UPROPERTY(EditDefaultsOnly, Category="Hold To Confirm", meta=(AllowPrivateAccess="true", ClampMin="0.1"))
	float M_IdleBlinkFrequency = 0.6f;

	UPROPERTY(EditDefaultsOnly, Category="Hold To Confirm", meta=(AllowPrivateAccess="true", ClampMin="0.0", ClampMax="1.0"))
	float M_IdleBlinkMinOpacity = 0.45f;

	UPROPERTY(EditDefaultsOnly, Category="Hold To Confirm", meta=(AllowPrivateAccess="true", ClampMin="0.0", ClampMax="1.0"))
	float M_IdleBlinkMaxOpacity = 1.0f;

	// Runtime state grouped to keep hold progression and idle feedback values synchronized.
	UPROPERTY()
	FHoldToSkipRuntimeState M_HoldRuntimeState;
};
