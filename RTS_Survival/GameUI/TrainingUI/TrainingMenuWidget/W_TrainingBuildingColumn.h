// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_TrainingBuildingColumn.generated.h"

class UButton;
class UPanelWidget;
class UProgressBar;
class UTextBlock;
class UTrainingMenuManager;

// What one entry of the building column shows; made by the training menu manager on every refresh.
struct FTrainingBuildingEntryState
{
	// Whether this is the building whose training items are on screen.
	bool bIsShownBuilding = false;
	// False when the building trains nothing; its progress bar is then hidden.
	bool bIsTraining = false;
	// How far the unit in training is, from 0 to 1.
	float TrainingProgress = 0.0f;
};

/**
 * @brief One entry of the building column next to the training menu: a numbered button that selects one
 * of the player's buildings, with a bar that shows how far that building's current unit is.
 */
UCLASS()
class RTS_SURVIVAL_API UW_TrainingBuildingEntry : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitBuildingEntry(UTrainingMenuManager* TrainingMenuManager, const int32 EntryIndex);

	void UpdateBuildingEntry(const FTrainingBuildingEntryState& EntryState);

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	TObjectPtr<UButton> M_EntryButton;

	// Shows the number of the building, counted from one.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	TObjectPtr<UTextBlock> M_EntryText;

	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	TObjectPtr<UProgressBar> M_TrainingProgressBar;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building Entry")
	FLinearColor ShownBuildingButtonColor = FLinearColor(0.75f, 0.55f, 0.15f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Building Entry")
	FLinearColor OtherBuildingButtonColor = FLinearColor(0.12f, 0.12f, 0.12f, 0.85f);

private:
	UPROPERTY()
	TWeakObjectPtr<UTrainingMenuManager> M_TrainingMenuManager;
	bool GetIsValidTrainingMenuManager() const;

	int32 M_EntryIndex = INDEX_NONE;

	UFUNCTION()
	void OnEntryClicked();
};

/**
 * @brief Sits against the left edge of the training menu and grows downwards: when the player has several
 * expanded buildings of the type the menu shows, each gets an entry to jump to it and watch its training.
 * @note Place the UW_TrainingBuildingEntry widgets inside the bound panel; their count is the most
 * buildings of one type the column can show.
 */
UCLASS()
class RTS_SURVIVAL_API UW_TrainingBuildingColumn : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitBuildingColumn(UTrainingMenuManager* TrainingMenuManager);

	int32 GetMaxEntries() const { return M_BuildingEntries.Num(); }

	/**
	 * @param EntryStates One state per building, from top to bottom; entries without a state take no space.
	 */
	void UpdateBuildingEntries(const TArray<FTrainingBuildingEntryState>& EntryStates);

protected:
	virtual void NativeOnInitialized() override;

	// Holds the UW_TrainingBuildingEntry widgets, from top to bottom.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	TObjectPtr<UPanelWidget> M_EntryPanel;

private:
	UPROPERTY()
	TArray<UW_TrainingBuildingEntry*> M_BuildingEntries;
};
