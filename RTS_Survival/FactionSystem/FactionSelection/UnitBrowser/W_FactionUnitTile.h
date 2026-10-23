// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FactionUnitBrowserStyle.h"

#include "W_FactionUnitTile.generated.h"

class UBorder;
class UButton;
class UImage;
class UMaterialInstanceDynamic;
class UOverlay;
class UTextBlock;
class UW_FactionUnitBrowser;

/**
 * @brief One unit in the faction menu's unit browser: its training picture in a well with the name under it; created by
 * UW_FactionUnitBrowser, builds its own widget tree and tells the browser when it is clicked.
 */
UCLASS()
class RTS_SURVIVAL_API UW_FactionUnitTile : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Fills the tile; call once right after creating it.
	 * @param Browser Receives the click.
	 * @param TileIndex Index of the unit in the browser's shown units.
	 * @param Style Brushes, fonts and colours shared by the browser.
	 * @param UnitIcon The unit's training picture; a brush without a resource leaves the tile empty.
	 * @param UnitName Plain text name of the unit.
	 * @param WearSeed Picks this tile's scuffs, chips, dent and crack; the same unit always gets the same seed.
	 */
	void SetupTile(UW_FactionUnitBrowser* Browser, const int32 TileIndex, const FFactionUnitBrowserStyle& Style,
	               const FSlateBrush& UnitIcon, const FText& UnitName, const float WearSeed);

	void SetIsSelected(const bool bIsSelected);

	/** Clicks the tile the way the player does; used by the faction menu capture test. */
	void SimulateClick();

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY()
	TWeakObjectPtr<UW_FactionUnitBrowser> M_Browser;

	UPROPERTY()
	TObjectPtr<UButton> M_Button;

	UPROPERTY()
	TObjectPtr<UBorder> M_IconWell;

	UPROPERTY()
	TObjectPtr<UImage> M_Icon;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_NameText;

	UPROPERTY()
	TObjectPtr<UImage> M_WearImage;

	// This tile's copy of the wear material; the image brush points at it, this keeps it alive with the tile.
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> M_WearMaterial;

	// Copied from the browser so selection can swap the button style, the icon well and the text colour.
	UPROPERTY()
	FFactionUnitBrowserStyle M_Style;

	int32 M_TileIndex = INDEX_NONE;
	bool bM_IsSelected = false;

	void BuildWidgetTree();
	void BuildIcon(UOverlay* TileOverlay);
	void BuildName(UOverlay* TileOverlay);
	void BuildWear(UOverlay* TileOverlay);
	void ApplyWear(const FSlateBrush& WearBrush, const float WearSeed);
	void ApplySelectionLook();

	UFUNCTION()
	void OnTileClicked();

	bool GetIsValidBrowser() const;
	bool GetIsValidButton() const;
};
