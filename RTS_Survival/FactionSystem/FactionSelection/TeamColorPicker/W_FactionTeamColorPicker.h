// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FactionTeamColorPickerLayout.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitBrowser/FactionUnitBrowserStyle.h"

#include "W_FactionTeamColorPicker.generated.h"

class UHorizontalBox;
class UOverlay;
class UTextBlock;
class UW_FactionSelectionMenu;
class UW_FactionTeamColorSwatch;

/**
 * @brief The faction menu's team colour panel: a row of colours for the player's vehicles, the camo default first.
 * Created by UW_FactionSelectionMenu under the faction buttons, skinned with the unit browser's style; a click tells the
 * menu, which stores the colour and repaints the hangar preview.
 */
UCLASS()
class RTS_SURVIVAL_API UW_FactionTeamColorPicker : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Fills the panel; call once right after creating it.
	 * @param Menu Receives the picked colour.
	 * @param Style The unit browser's skin, so both panels look alike.
	 * @param CurrentTeamColor Selected at start; a colour not in the palette selects the camo default.
	 */
	void SetupPicker(UW_FactionSelectionMenu* Menu, const FFactionUnitBrowserStyle& Style,
	                 const FLinearColor& CurrentTeamColor);

	void HandleSwatchClicked(const int32 SwatchIndex);

	/** Test access for the faction menu capture. */
	UW_FactionTeamColorSwatch* GetSwatch(const int32 SwatchIndex) const;
	int32 GetSwatchCount() const { return M_Swatches.Num(); }

private:
	UPROPERTY()
	TWeakObjectPtr<UW_FactionSelectionMenu> M_Menu;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_ColorNameText;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> M_SwatchRow;

	UPROPERTY()
	TArray<TObjectPtr<UW_FactionTeamColorSwatch>> M_Swatches;

	TArray<FFactionTeamColorOption> M_Options;
	int32 M_SelectedIndex = 0;

	void BuildWidgetTree(const FFactionUnitBrowserStyle& Style);
	void BuildHeader(UOverlay* PanelOverlay, const FFactionUnitBrowserStyle& Style);
	void BuildSwatches(const FFactionUnitBrowserStyle& Style);
	void SelectSwatch(const int32 SwatchIndex);
	int32 FindOptionIndex(const FLinearColor& TeamColor) const;

	bool GetIsValidMenu() const;
};
