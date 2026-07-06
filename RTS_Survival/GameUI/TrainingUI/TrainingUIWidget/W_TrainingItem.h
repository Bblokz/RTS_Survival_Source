// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingUIWidget/TrainingItemState/TrainingWidgetState.h"
#include "W_TrainingItem.generated.h"

class USlateBrushAsset;
class UTexture2D;
class UW_CoolDownItem;
class RTS_SURVIVAL_API UTrainingMenuManager;


/**
 * @brief Displays a training queue item and keeps its cooldown clock synced with queue state.
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
	void SetTrainingButtonRenderOpacity(const float NewRenderOpacity);

	/**
	 * @brief Called when the trainer finished an item and the count needs to be adjusted.
	 * @param NewCount The count to display on this widget.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "TrainingItem")
	void OnUpdateCountItem(const int NewCount);

	/**
	 * @brief Starts the cooldown clock from the active training queue state.
	 * @param TimeRemaining Seconds left for the active queue item.
	 * @param TotalTrainingTime Total training duration used by the cooldown material.
	 * @param bIsPaused Whether the clock should initialize paused.
	 */
	void StartClock(
		const int32 TimeRemaining,
		const int32 TotalTrainingTime,
		const bool bIsPaused = false);

	/**
	 * @brief Stops the cooldown clock and restores the item icon to ready state.
	 */
	void StopClock();

	/**
	 * @brief Sets the cooldown clock to a paused state.
	 * @param bPause If true, pauses the clock; if false, resumes it from the same visual progress.
	*/
	void SetClockPaused(const bool bPause);

	FTrainingWidgetState GetTrainingItemState() const { return M_TrainingItemState; }

protected:
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "TrainingItem")
	void OnClickedTrainingItem(const bool bIsLeftClick);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "TrainingItem")
	void OnHoverTrainingItem(const bool bIsHovering);
	
	// Called after OnUpdateTrainingItem in the blueprint has loaded the slate brush asset from the
	// data table.
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "TrainingItem")
	void OnObtainedSlateBrushFromTrainingTable(USlateBrushAsset* SlateBrushAsset);
	

	/**
	 * @brief Updates the widget with the provided state.
	 * @param TrainingItemState The state to update the widget with.
	 * @post Propagates state to blueprint.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "TrainingItem")
	void OnUpdateTrainingItem(const FTrainingWidgetState& TrainingItemState);

	// The size box that is the parent of all elements in this widget.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	USizeBox* M_TrainingItemSizeBox;

	// The button of this widget that is underneath the size box in the hierarchy.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	UButton* M_TrainingItemButton;
	
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	UW_CoolDownItem* M_CoolDownItem;


private:
	UPROPERTY()
	UTrainingMenuManager* M_TrainingUIManager;

	// The index of the training item in the training item scrollbar.
	UPROPERTY()
	int32 M_Index;

	UPROPERTY()
	FTrainingWidgetState M_TrainingItemState;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> M_CooldownIconTexture;

	int32 M_ClockSecondsLeft = 0;
	int32 M_ClockTotalTrainingTime = 0;
	bool bM_HasActiveClockState = false;
	bool bM_IsClockPaused = false;

	void ResetCachedCooldownState();
	void ResetCooldownItemForMissingBrush();
	void ApplyCooldownItemFromCachedState();
	UTexture2D* GetIconTextureFromSlateBrushAsset(USlateBrushAsset* SlateBrushAsset) const;
	bool GetIsValidCoolDownItem() const;
	bool GetIsValidTrainingUIManager() const;
};
