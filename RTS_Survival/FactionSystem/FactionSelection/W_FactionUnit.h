// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "W_FactionUnit.generated.h"

class UButton;
class UW_FactionSelectionMenu;

/**
 * @brief Widget entry used in the faction selection list to represent a previewable unit.
 */
UCLASS()
class RTS_SURVIVAL_API UW_FactionUnit : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetTrainingOption(const FTrainingOption& TrainingOption);
	void SetFactionSelectionMenu(UW_FactionSelectionMenu* FactionSelectionMenu);
	void SimulateUnitButtonClick();

protected:
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintImplementableEvent)
	void BP_OnTrainingOptionChanged(const FTrainingOption& TrainingOption);

	UPROPERTY(meta = (BindWidget))
	UButton* M_UnitButton = nullptr;

private:
	FTrainingOption M_TrainingOption;

	UPROPERTY()
	TWeakObjectPtr<UW_FactionSelectionMenu> M_FactionSelectionMenu;

	UFUNCTION()
	void HandleUnitButtonClicked();

	bool GetIsValidUnitButton() const;
	bool GetIsValidFactionSelectionMenu() const;
};
