// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "W_TrainingMenu.generated.h"

class UPanelWidget;
class UTrainingMenuManager;
class UW_TrainingItem;
class UW_TrainingQuickNavButton;
class UW_TrainingSubSectionTab;

// What one sub section tab shows; made by the training menu manager on every refresh.
struct FTrainingSubSectionTabState
{
	FText DisplayName;
	int32 QueuedCount = 0;
};

/**
 * @brief The training part of the main game UI: quick navigation buttons on top, sub section tabs below
 * them and a grid of training items at the bottom. The training menu manager drives all three.
 * @note Place the buttons, tabs and training items inside the three bound panels of the widget blueprint;
 * they are picked up in the order of the widget tree.
 */
UCLASS()
class RTS_SURVIVAL_API UW_TrainingMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitTrainingMenu(UTrainingMenuManager* TrainingMenuManager);

	const TArray<UW_TrainingItem*>& GetTrainingItems() const { return M_TrainingItems; }

	/**
	 * @brief Shows one tab per sub section and makes them share the full width equally, so that a
	 * building with one sub section gets a single tab over the whole width.
	 * @param TabStates The sub sections of the trainer in tab order.
	 * @param SelectedSubSectionIndex The sub section whose training items are on screen.
	 */
	void UpdateSubSectionTabs(const TArray<FTrainingSubSectionTabState>& TabStates,
	                          const int32 SelectedSubSectionIndex);

	/**
	 * @param AvailableBuildings The building types of which the player has an expanded, training building.
	 * @param ShownBuilding The building type the training menu shows; gets the highlighted button.
	 */
	void UpdateQuickNavButtons(const TSet<ENomadicSubtype>& AvailableBuildings, const ENomadicSubtype ShownBuilding);

protected:
	virtual void NativeOnInitialized() override;

	// Holds the UW_TrainingQuickNavButton widgets, from left to right.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	TObjectPtr<UPanelWidget> M_QuickNavPanel;

	// Holds the UW_TrainingSubSectionTab widgets, from left to right. Must be a horizontal box for the
	// tabs to share the width equally.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	TObjectPtr<UPanelWidget> M_SubSectionPanel;

	// Holds the UW_TrainingItem widgets, row by row.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	TObjectPtr<UPanelWidget> M_TrainingItemPanel;

private:
	UPROPERTY()
	TArray<UW_TrainingItem*> M_TrainingItems;

	UPROPERTY()
	TArray<UW_TrainingSubSectionTab*> M_SubSectionTabs;

	UPROPERTY()
	TArray<UW_TrainingQuickNavButton*> M_QuickNavButtons;

	void InitTrainingMenu_SetupQuickNavButtons(UTrainingMenuManager* TrainingMenuManager);
};
