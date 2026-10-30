// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SurvivalArmyUiTypes.h"

#include "W_SurvivalArmyTab.generated.h"

class UButton;
class UTextBlock;
class USurvivalUiStyleAsset;
class UW_SurvivalArmySelection;

/**
 * @brief One roster filter tab of the army selection (All / Infantry / Vehicles / Trucks / Aircraft). Created by
 * UW_SurvivalArmySelection for every category that has units; builds its own widget tree.
 */
UCLASS()
class RTS_SURVIVAL_API UW_SurvivalArmyTab : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetupTab(UW_SurvivalArmySelection* Owner, const ESurvivalRosterCategory Category,
	              const USurvivalUiStyleAsset* StyleAsset);

	void SetIsSelected(const bool bIsSelected);

	ESurvivalRosterCategory GetCategory() const { return M_Category; }

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY()
	TWeakObjectPtr<UW_SurvivalArmySelection> M_Owner;

	UPROPERTY()
	TObjectPtr<const USurvivalUiStyleAsset> M_StyleAsset;

	UPROPERTY()
	TObjectPtr<UButton> M_Button;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_LabelText;

	ESurvivalRosterCategory M_Category = ESurvivalRosterCategory::All;
	bool bM_IsSelected = false;

	void BuildWidgetTree();
	void ApplySelectionLook();

	UFUNCTION()
	void OnTabClicked();

	bool GetIsValidOwner() const;
	bool GetIsValidButton() const;
};
