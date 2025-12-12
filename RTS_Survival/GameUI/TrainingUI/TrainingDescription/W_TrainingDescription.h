// Copyright Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingUIWidget/TrainingItemState/TrainingWidgetState.h"

#include "W_TrainingDescription.generated.h"

class UPlayerResourceManager;
class UW_CostDisplay;
class UTrainingMenuManager;
class UMainGameUI;


/**
 * Displays text and cost of the training option hovered.
 * Make sure to hide other bottom panel widgets in the main menu, do not use SetTrainingDescriptionVisiblity but
 * use UMainGameUI::SetBottomUIPanel to regulate which widget will be displayed.
 * @note this widget is at the same place as the BXPDescription widget and ActionUI.
 */

UCLASS()
class UW_TrainingDescription : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Sets the visibility of the training description widget.
	 * @param bVisible Determines if the widget is visible.
	 * @param TrainingWidgetState The training option to display.
	 */
	void SetTrainingDescriptionVisibility(const bool bVisible,
	                                      const FTrainingWidgetState& TrainingWidgetState = FTrainingWidgetState());

	void SetTrainingMenuManager(UTrainingMenuManager* TrainingMenuManager);
	void SetResourceManager(TWeakObjectPtr<UPlayerResourceManager> PlayerResourceManager);

protected:
	/** @brief Sets up the description widget of the training option.
	 * @note Do not call, only called by OnUpdateForWidgetState*/
	UFUNCTION(BlueprintImplementableEvent, NotBlueprintable, Category="DisplayTrainingDescription")
	void OnShowTrainingDescription(const FTrainingWidgetState& TrainingOption);

	UFUNCTION(BlueprintCallable, Category="Time")
	FText GetTrainingTimeDisplay(const FTrainingOption TrainingOption, const float TimeToTrain) const;


	UPROPERTY(meta=(BindWidget, AllowPrivateAccess = "true"))
	UW_CostDisplay* CostDisplay;
	
	private:
	TObjectPtr<UTrainingMenuManager> M_TrainingMenuManager;

	bool GetIsValidTrainingMenuManager() const;
	bool GetIsValidCostDisplay()const;

	void UpdateCostDisplayForTrainingOption(const FTrainingOption& TrainingOption) const;

	/**
	 * @brief Updates the description to match the state. Also updates the cost.
	 * @param TrainingWidgetState Defines the UI state of the training option.
	 * @post The CostWidget is updated to match the cost of the training option.
	 * @post The description text is updated in blueprints.
	 */
	void OnUpdateForWidgetState(const FTrainingWidgetState& TrainingWidgetState);

	

	TWeakObjectPtr<UPlayerResourceManager> M_PlayerResourceManager;
	bool GetIsValidPlayerResourceManager() const;

};
