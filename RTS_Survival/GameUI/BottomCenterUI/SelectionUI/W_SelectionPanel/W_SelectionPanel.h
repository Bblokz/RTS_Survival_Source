// Copyright (C) Bas Blokzijl - All rights.
// @brief Two-row selection panel with up to four page (enumerate) buttons.
//        Builds pages from a flat list of unit widget states; routes tile and page clicks upward.
// @note InitBeforeMainGameUIInit: call in Blueprint before main UI init to wire enumerate buttons.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_SelectionPanel.generated.h"

class UW_SelectedUnitsRow;
class UW_EnumerateUnitSelection;
struct FSelectedUnitsWidgetState;
class ACPPController;

/**
 * @brief Two-row selection panel that displays selected units and offers paging via enumerate buttons.
 *        RebuildSelectionUI consumes a flat array of states and lays them out across the two rows.
 * @note InitBeforeMainGameUIInit: call in blueprint to prime child arrays/buttons before main UI init.
 */
UCLASS()
class RTS_SURVIVAL_API UW_SelectionPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Rebuilds the selection UI from a flat list of widget states and activates a given page.
	 * @param AllWidgetStates Flat array of tile states (already ordered by controller).
	 * @param ActivePageIndex Page index to show (0-based; clamped internally).
	 */
	void RebuildSelectionUI(const TArray<FSelectedUnitsWidgetState>& AllWidgetStates, int32 ActivePageIndex);

	void InitMainGameUI(ACPPController* PlayerController);

	/**
	 * @brief Initializes references that must exist before the main game UI init continues.
	 *        Populates the 4 enumerate-button pointers from the two child rows.
	 */
	void InitBeforeMainGameUIInit();

	/** @brief Safe accessor; returns nullptr if Index out of range or not initialized. */
	UW_EnumerateUnitSelection* GetEnumerateButtonAt(int32 Index) const;

	/** @brief Returns true once all four enumerate buttons are discovered and non-null. */
	bool GetAreEnumerateButtonsInitialized() const { return bM_EnumerateButtonsInitialized; }

	/** @brief Routed by UW_SelectedUnit on click; modifies player selection depending on modifiers. */
	void HandleSelectedUnitClicked(const FSelectedUnitsWidgetState& ClickedState);

	/** @brief Routed by UW_EnumerateUnitSelection on click; updates currently drawn page only. */
	void HandleEnumerateClicked(int32 NewPageIndex);


protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UW_SelectedUnitsRow* SelectedUnitsRow1 = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UW_SelectedUnitsRow* SelectedUnitsRow2 = nullptr;

private:
	/** @brief Validates SelectedUnitsRow1 is bound and alive; logs on failure. */
	bool GetIsValidSelectedUnitsRow1() const;

	/** @brief Validates SelectedUnitsRow2 is bound and alive; logs on failure. */
	bool GetIsValidSelectedUnitsRow2() const;

	/** @brief Validates the given enumerate button pointer; logs on failure. */
	bool GetIsValidEnumerateButton(const UW_EnumerateUnitSelection* InPtr, const TCHAR* DebugName) const;

	/** @brief Validates both rows via their validators. */
	bool EnsureSelectedUnitsRowsAreValid() const;

	/** @brief Fills M_EnumerateButtons[] from the two rows using their getters. */
	void InitializeEnumerateButtonsFromRows();

	/**
	 * @brief Shows/hides up to 4 enumerate buttons and sets their labels and page indices.
	 * @param PageCount Total number of pages.
	 * @param ActivePage 0-based page index currently shown.
	 */
	void UpdateEnumerateButtonsVisibilityAndLabels(int32 PageCount, int32 ActivePage);

	/**
	 * @brief Draws a single page into row1 and row2; hides unused tiles.
	 * @param AllWidgetStates Flat array of states from controller.
	 * @param PageIndex Page to render (0-based).
	 */
	void DrawPage(const TArray<FSelectedUnitsWidgetState>& AllWidgetStates, int32 PageIndex);

	/** @brief Returns true if the player controller weak ptr is valid, logs otherwise. */
	bool GetIsValidPlayerController() const;

	/**
	 * Four enumerate buttons ordered as:
	 * [0] Row1.Button1, [1] Row1.Button2, [2] Row2.Button1, [3] Row2.Button2
	 * Marked UPROPERTY so GC tracks the references.
	 */
	UPROPERTY()
	UW_EnumerateUnitSelection* M_EnumerateButtons[4] = { nullptr, nullptr, nullptr, nullptr };

	/** True once all four pointers are non-null. */
	bool bM_EnumerateButtonsInitialized = false;

	/** Player controller to query modifiers and mutate selection. Not owned here. */
	UPROPERTY()
	TWeakObjectPtr<ACPPController> M_PlayerController;

	// Cached last data so enumerate buttons can re-render without recompute.
	UPROPERTY()
	TArray<FSelectedUnitsWidgetState> M_CachedFlatStates;

	/** Currently shown page (0-based). */
	int32 M_CurrentPage = 0;

	void Debug(const TArray<FSelectedUnitsWidgetState>& AllWidgetStates, int32 ActivePageIndex);
};
