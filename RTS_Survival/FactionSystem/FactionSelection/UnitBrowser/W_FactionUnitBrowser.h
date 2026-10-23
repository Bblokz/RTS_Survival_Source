// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FactionUnitBrowserStyle.h"

#include "W_FactionUnitBrowser.generated.h"

class UDataTable;
class UHorizontalBox;
class UScrollBox;
class UTextBlock;
class UOverlay;
class UW_FactionSelectionMenu;
class UW_FactionUnitTab;
class UW_FactionUnitTile;

/**
 * @brief The unit browser at the bottom of the faction selection menu: filter tabs over a scrolling row of unit tiles.
 * UW_FactionSelectionMenu creates it on its canvas and hands it the faction's unit groups; clicking a tile previews the
 * unit in the hangar through the menu. Builds its own widget tree; the Blueprint child only fills M_Style.
 * @note M_Style: fill in the class defaults of BP_W_FactionUnitBrowser (UISkins/_generator/ui_faction_unit_browser.py).
 */
UCLASS()
class RTS_SURVIVAL_API UW_FactionUnitBrowser : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetupBrowser(UW_FactionSelectionMenu* Menu);

	/**
	 * @brief Shows a faction's units: one tab per group that has units, the first tab selected and its first unit
	 * previewed, as picking a faction always did.
	 * @param Groups In tab order; the first is expected to be ALL.
	 */
	void SetUnitGroups(const TArray<FFactionUnitBrowserGroup>& Groups);

	void HandleTabClicked(const int32 TabIndex);
	void HandleTileClicked(const int32 TileIndex);

	int32 GetTabCount() const { return M_Tabs.Num(); }
	FText GetTabLabel(const int32 TabIndex) const;
	const TArray<FTrainingOption>& GetShownUnits() const { return M_ShownUnits; }

	/** Test access: the tab and tile widgets, to click them the way the player does. */
	UW_FactionUnitTab* GetTab(const int32 TabIndex) const;
	UW_FactionUnitTile* GetTile(const int32 TileIndex) const;

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(EditDefaultsOnly, Category = "Style")
	FFactionUnitBrowserStyle M_Style;

private:
	UPROPERTY()
	TWeakObjectPtr<UW_FactionSelectionMenu> M_Menu;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_TitleText;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> M_TabRow;

	UPROPERTY()
	TObjectPtr<UScrollBox> M_TileScroll;

	UPROPERTY()
	TArray<TObjectPtr<UW_FactionUnitTab>> M_Tabs;

	UPROPERTY()
	TArray<TObjectPtr<UW_FactionUnitTile>> M_Tiles;

	// Only the groups that have units, so tab indices index this array.
	UPROPERTY()
	TArray<FFactionUnitBrowserGroup> M_Groups;

	UPROPERTY()
	TArray<FTrainingOption> M_ShownUnits;

	// DT_TrainingOptions; held so the row brushes the tiles show stay loaded.
	UPROPERTY()
	TObjectPtr<const UDataTable> M_TrainingOptionsTable;

	int32 M_SelectedTabIndex = INDEX_NONE;
	int32 M_SelectedTileIndex = INDEX_NONE;

	void BuildWidgetTree();
	void BuildHeader(UOverlay* PanelOverlay);
	void BuildTabRow(UOverlay* PanelOverlay);
	void BuildTileScroll(UOverlay* PanelOverlay);
	void RebuildTabs();
	void RebuildTiles();
	void AddTile(const int32 TileIndex, const FTrainingOption& Unit);
	void SelectTile(const int32 TileIndex);

	FSlateBrush GetUnitIconBrush(const FTrainingOption& Unit) const;
	FText GetUnitName(const FTrainingOption& Unit) const;
	static float GetUnitWearSeed(const FTrainingOption& Unit);

	bool GetIsValidMenu() const;
	bool GetIsValidTabRow() const;
	bool GetIsValidTileScroll() const;
};
