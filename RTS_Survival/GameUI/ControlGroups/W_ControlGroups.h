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
	void UpdateMostFrequentUnitInGroup(const int32 GroupIndex, const FTrainingOption TrainingOption);


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
