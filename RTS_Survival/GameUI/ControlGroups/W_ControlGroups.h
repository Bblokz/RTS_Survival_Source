// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_ControlGroups.generated.h"

class UW_ControlGroupImage;
struct FTrainingOption;
/**
 * @brief Maintains a collection of control group widgets and updates their displayed unit and hotkey state.
 */
UCLASS()
class RTS_SURVIVAL_API UW_ControlGroups : public UUserWidget
{
	GENERATED_BODY()


public:
	/**
	 * @brief Shows the group's most frequent unit and size on its slot; an empty group clears the slot.
	 * @param GroupIndex Control group index (0 = key 1).
	 * @param TrainingOption Most frequent unit of the group, None when the group is empty.
	 * @param UnitCount Number of units in the group.
	 */
	void UpdateMostFrequentUnitInGroup(const int32 GroupIndex, const FTrainingOption TrainingOption, const int32 UnitCount);

	// The group's hotkey just selected it: its slot flashes.
	void OnControlGroupRecalled(const int32 GroupIndex);


protected:
	/**
	 * @brief Registers the control group widget list and initializes their hotkey bindings.
	 * @param ControlGroupWidgets Control group widget list in slot order.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Init")
	void InitControlGroups(TArray<UW_ControlGroupImage*> ControlGroupWidgets);


private:
	UPROPERTY()
	TArray<TObjectPtr<UW_ControlGroupImage>> M_ControlGroupWidgets;
};
