// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingUIWidget/TrainingItemState/TrainingWidgetState.h"
#include "W_TrainingItem.generated.h"

class RTS_SURVIVAL_API UTrainingMenuManager;


/**
 * @brief Displays a training queue item and handles its clock-driven opacity updates.
 */
UCLASS()
class RTS_SURVIVAL_API UW_TrainingItem : public UUserWidget
{
	GENERATED_BODY()

public:
	UW_TrainingItem(const FObjectInitializer& ObjectInitializer);

	void InitW_TrainingItem(UTrainingMenuManager* TrainingUIManager, int32 IndexInScrollBar);

	/**
	 * @brief Propagates the training item state to the blueprint widget.
	 */
	void UpdateTrainingItem(const FTrainingWidgetState& TrainingItemState);
	void SetTrainingButtonRenderOpacity(const float RenderOpacity);

	/**
	 * @brief Called when the trainer finished an item and the count needs to be adjusted.
	 * @param NewCount The count to display on this widget.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "TrainingItem")
	void OnUpdateCountItem(const int NewCount);

	/**
	 * @brief Starts a clock animation on the image of the button.
	 * @param TimeRemaining The time remaining for the animation.
	 * @param TotalTrainingTime The total time for the animation; used to calculate start opacity.
	 * @param bIsPaused Whether the clock should start paused in this case we set the clock to the proper opacity.
	 */
	void StartClock(
		const int32 TimeRemaining,
		const int32 TotalTrainingTime,
		const bool bIsPaused = false);

	/**
	 * @brief Stops the clock animation on the image of the button.
	 */
	void StopClock();

	/**
	 * @brief Sets the clock to a paused state.
	 * @param bPause If true, pauses the clock; if false, resumes the clock from where it was paused.
	*/
	void SetClockPaused(const bool bPause);

	FTrainingWidgetState GetTrainingItemState() const { return M_TrainingItemState; }

protected:
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "TrainingItem")
	void OnClickedTrainingItem(const bool bIsLeftClick);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "TrainingItem")
	void OnHoverTrainingItem(const bool bIsHovering);

	/**
	 * @brief Updates the widget with the provided state.
	 * @param TrainingItemState The state to update the widget with.
	 * @post Propagates state to blueprint.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "TrainingItem")
	void OnUpdateTrainingItem(const FTrainingWidgetState& TrainingItemState);

	// The style for the button.
	UPROPERTY(EditAnywhere, Category = "Style")
	USlateWidgetStyleAsset* ButtonStyleAsset;

	/**
	 * @brief Updates the look of the button used by this item using a reference to the slate set in blueprint.
	 * called at init of this widget.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void UpdateButtonWithGlobalSlateStyle();

	// The size box that is the parent of all elements in this widget.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	USizeBox* M_TrainingItemSizeBox;

	// The button of this widget that is underneath the size box in the hierarchy.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	UButton* M_TrainingItemButton;


private:
	UPROPERTY()
	UTrainingMenuManager* M_TrainingUIManager;

	// The index of the training item in the training item scrollbar.
	UPROPERTY()
	int32 M_Index;

	UPROPERTY()
	FTrainingWidgetState M_TrainingItemState;

	FTimerHandle M_ClockTimerHandle;
	float M_AnimationStartTime;
	float M_AnimationEndTime;

	float M_InitialOpacity;
	bool bM_IsClockPaused;
	const float M_LowestPossibleTrainingOpacity = 0.25f;

	void UpdateClockOpacity();

	/** 
	 * Returns the clock opacity at a given world time, 
	 * using a power curve so the last 40% of time is most visible. 
	 */
	float ComputeClockOpacity(float WorldTime) const;

	bool bM_IsCLockPaused = false;
	float M_PauseTime = 0.0f;

	void ResumeClock();
	bool GetIsValidTrainingUIManager() const;
};
