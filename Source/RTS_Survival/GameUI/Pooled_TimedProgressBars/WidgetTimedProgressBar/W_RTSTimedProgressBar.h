#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/GameUI/Pooled_TimedProgressBars/RTSProgressBarType.h"
#include "W_RTSTimedProgressBar.generated.h"

class URichTextBlock;
class UProgressBar;
class UTextBlock;

/**
 * @brief Minimal widget with a progress bar and optional percentage text.
 * It is controlled by the pool manager (no self ticking).
 */
UCLASS()
class RTS_SURVIVAL_API UW_RTSTimedProgressBar : public UUserWidget
{
	GENERATED_BODY()

public:
	UW_RTSTimedProgressBar(const FObjectInitializer& ObjectInitializer);

	/** Called by the manager when (re)activating this widget. */
	UFUNCTION(BlueprintCallable, Category="Timed Progress Bar")
	void ActivateProgressBar(const float InitialRatio,
	                         const bool bInUsePercentageText,
	                         const ERTSProgressBarType InType, const bool bInUseText, const FString& InText);

	/** Called by the manager each tick to update the fill ratio (0..1). */
	UFUNCTION(BlueprintCallable, Category="Timed Progress Bar")
	void UpdateProgress(const float Ratio, const bool bInUsePercentageText);

	/** Mark as dormant and hide; used by the pool when the bar completes or is force-dormant. */
	UFUNCTION(BlueprintCallable, Category="Timed Progress Bar")
	void SetDormant(const bool bInDormant);

	/** True when not in use / pooled. False while actively showing progress. */
	UPROPERTY(BlueprintReadOnly, Category="Timed Progress Bar")
	bool bM_IsDormant = true;

protected:
	// Artists can style the widget in BP based on the BarType.
	UFUNCTION(BlueprintImplementableEvent)
	void OnProgressBarTypeApplied(ERTSProgressBarType BarType);

private:
	// Bound automatically to the UMG widget by name.
	UPROPERTY(meta = (BindWidget), meta=(AllowPrivateAccess="true"), BlueprintReadOnly)
	UProgressBar* M_ProgressBar = nullptr;

	// Bound automatically to the UMG widget by name.
	UPROPERTY(meta = (BindWidget), meta=(AllowPrivateAccess="true"), BlueprintReadOnly)
	UTextBlock* M_ProgressText = nullptr;
	
	UPROPERTY(meta = (BindWidget), meta=(AllowPrivateAccess="true"), BlueprintReadOnly)
	URichTextBlock* M_RichProgressText = nullptr;

	void ApplyPercentText(const float Ratio, const bool bInUsePercentageText) const;
	void ApplyDescriptionText(const bool bInUseText, const FString& InText) const;
};
