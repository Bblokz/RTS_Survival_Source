// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FactionUnitBrowserStyle.h"

#include "W_FactionUnitTab.generated.h"

class UButton;
class UTextBlock;
class UW_FactionUnitBrowser;

/**
 * @brief One filter tab (ALL, RECON, LIGHT, ...) of the faction menu's unit browser; created by UW_FactionUnitBrowser,
 * builds its own widget tree and tells the browser when it is clicked.
 */
UCLASS()
class RTS_SURVIVAL_API UW_FactionUnitTab : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Fills the tab; call once right after creating it.
	 * @param Browser Receives the click.
	 * @param TabIndex Index of the tab in the browser's tab row.
	 * @param Style Brushes, fonts and colours shared by the browser.
	 * @param Label Group name, shown in capitals.
	 */
	void SetupTab(UW_FactionUnitBrowser* Browser, const int32 TabIndex, const FFactionUnitBrowserStyle& Style,
	              const FText& Label);

	void SetIsSelected(const bool bIsSelected);

	/** Clicks the tab the way the player does; used by the faction menu capture test. */
	void SimulateClick();

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY()
	TWeakObjectPtr<UW_FactionUnitBrowser> M_Browser;

	UPROPERTY()
	TObjectPtr<UButton> M_Button;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_LabelText;

	// Copied from the browser so selection can swap the button style and the label colour.
	UPROPERTY()
	FFactionUnitBrowserStyle M_Style;

	int32 M_TabIndex = INDEX_NONE;
	bool bM_IsSelected = false;

	void BuildWidgetTree();
	void ApplySelectionLook();

	UFUNCTION()
	void OnTabClicked();

	bool GetIsValidBrowser() const;
	bool GetIsValidButton() const;
};
