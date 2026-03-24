#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_MissionTimer.generated.h"

class URichTextBlock;

USTRUCT(BlueprintType)
struct FTimerLifeTime
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission Timer")
	FText M_TimerText = FText::GetEmpty();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission Timer", meta = (ClampMin = "0.0"))
	float M_Time = 0.0f;
};

USTRUCT(BlueprintType)
struct FMissionTimerLifetimeSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission Timer")
	TArray<FTimerLifeTime> M_TimerLifetimes;
};

/**
 * @brief Mission timer widget that owns its full countdown lifetime and can optionally chain into follow-up entries.
 * @note InitMissionTimer: call right after adding to viewport so this widget can start its internal one-second ticker.
 */
UCLASS()
class RTS_SURVIVAL_API UW_MissionTimer : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Starts or restarts the timer with the provided display text and countdown value.
	 * @param TimerText Text shown as the timer purpose label.
	 * @param TimerInSeconds Total countdown duration in seconds.
	 * @param LifetimeSettings Follow-up entries consumed in order after each timer expiration.
	 */
	UFUNCTION(BlueprintCallable, Category = "Mission Timer")
	void InitMissionTimer(const FText& TimerText, float TimerInSeconds, const FMissionTimerLifetimeSettings& LifetimeSettings);

	virtual void NativeDestruct() override;

protected:
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	URichTextBlock* M_TimerText;

	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	URichTextBlock* M_ClockText;

private:
	FTimerHandle M_CountdownTimerHandle;

	FMissionTimerLifetimeSettings M_MissionTimerLifetimeSettings;

	int32 M_NextLifetimeEntryIndex = 0;

	float M_RemainingSeconds = 0.0f;

	void OnMissionTimerTick();
	void HandleTimerExpired();
	void SetTimerText(const FText& TimerText) const;
	void UpdateClockTextFromRemainingSeconds() const;
	FString GetFormattedClockText() const;
	void StartCountdownTimer();
	void DestroyWidgetAndStopTimer();

	bool GetIsValidTimerText() const;
	bool GetIsValidClockText() const;
};
