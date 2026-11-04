// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitBrowser/FactionUnitBrowserStyle.h"

#include "W_FactionTeamColorSwatch.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UW_FactionTeamColorPicker;
struct FFactionTeamColorOption;

/**
 * @brief One colour of the faction menu's team colour picker: a framed colour chip, or the camo label for the default;
 * created by UW_FactionTeamColorPicker, builds its own widget tree and tells the picker when it is clicked.
 */
UCLASS()
class RTS_SURVIVAL_API UW_FactionTeamColorSwatch : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Fills the swatch; call once right after creating it.
	 * @param Picker Receives the click.
	 * @param SwatchIndex Index of the colour in the picker's options.
	 * @param Option The colour and its name; alpha 0 is the camo default.
	 * @param Style The unit browser's skin: the frame uses its tab styles and heading font.
	 */
	void SetupSwatch(UW_FactionTeamColorPicker* Picker, const int32 SwatchIndex, const FFactionTeamColorOption& Option,
	                 const FFactionUnitBrowserStyle& Style);

	void SetIsSelected(const bool bIsSelected);

	/** Clicks the swatch the way the player does; used by the faction menu capture test. */
	void SimulateClick();

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY()
	TWeakObjectPtr<UW_FactionTeamColorPicker> M_Picker;

	UPROPERTY()
	TObjectPtr<UButton> M_Button;

	UPROPERTY()
	TObjectPtr<UImage> M_Chip;

	// Only shown on the camo default, which has no colour to show.
	UPROPERTY()
	TObjectPtr<UTextBlock> M_DefaultLabel;

	UPROPERTY()
	FFactionUnitBrowserStyle M_Style;

	int32 M_SwatchIndex = INDEX_NONE;
	bool bM_IsSelected = false;

	void BuildWidgetTree();

	UFUNCTION()
	void OnSwatchClicked();

	bool GetIsValidPicker() const;
	bool GetIsValidButton() const;
};
