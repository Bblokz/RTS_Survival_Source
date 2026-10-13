// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Subsystems/WorldSubsystem.h"
#include "TrainingLayoutTypes.h"
#include "TrainingLayoutSubsystem.generated.h"

class UTrainingLayoutDataAsset;

/**
 * @brief Trainer components ask this for their sub sections and the training menu asks it for the quick
 * navigation buttons. It applies the player's sub faction and unit cards to the training layout data asset.
 */
UCLASS()
class RTS_SURVIVAL_API UTrainingLayoutSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UTrainingLayoutSubsystem* Get(const UObject* WorldContextObject);

	/**
	 * @brief Builds what a building of the player can train right now; slots of units the player has not
	 * unlocked stay in place as empty slots so that the other units keep their position.
	 * @param Building The trainer building to build the sub sections for.
	 * @param OutSubSections The sub sections in tab order, with their flat option indices filled in.
	 * @return False when the layout has nothing for this building; the caller then keeps its own options.
	 */
	bool BuildSubSectionsForBuilding(const ENomadicSubtype Building, TArray<FTrainingSubSectionState>& OutSubSections);

	const TArray<FTrainingQuickNavEntry>& GetQuickNavEntries();

	/**
	 * @param QuickNavEntry The quick navigation entry to find the icon of.
	 * @param OutIconBrush The brush to show on the quick navigation button.
	 * @return False when neither the entry nor the training options table has an icon for the building.
	 */
	bool GetQuickNavIconBrush(const FTrainingQuickNavEntry& QuickNavEntry, FSlateBrush& OutIconBrush);

private:
	UPROPERTY()
	TObjectPtr<const UTrainingLayoutDataAsset> M_TrainingLayout;

	// Null when the layout could not be loaded; an error is reported once.
	const UTrainingLayoutDataAsset* GetTrainingLayout();

	// The training options of the training cards in the player profile. Only loaded when the layout does
	// not unlock everything, as reading the profile goes to disk.
	TSet<FTrainingOption> M_OptionsUnlockedByCards;
	bool bM_HasLoadedCardUnlocks = false;

	void LoadCardUnlocksIfNeeded();
	bool GetIsOptionUnlocked(const FTrainingOption& Option, const bool bBuildingRequiresUnitCards);

	bool GetIconBrushFromTrainingOptionTable(const FTrainingOption& Option, FSlateBrush& OutIconBrush);
};
