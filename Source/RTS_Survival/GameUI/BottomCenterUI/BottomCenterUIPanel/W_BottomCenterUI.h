// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "W_BottomCenterUI.generated.h"

struct FSelectedUnitsWidgetState;
class UW_SelectionPanel;
class UCanvasPanelSlot;
enum class EBxpOptionSection : uint8;
class IBuildingExpansionOwner;
struct FBxpOptionData;
class UBorder;
class UMainGameUI;
enum class ENomadicSubtype : uint8;
enum class EActionUINomadicButton : uint8;
class UW_OptionBuildingExpansion;
class UW_ItemBuildingExpansion;
class UW_BuildingUI_OptionPanel;
class ACPPController;
class UW_BuildingUI_ItemPanel;
class UWidgetSwitcher;
/**
 *	 COSMETIC ONLY DOES NOT TRACK STATE FOR BUILDING UI
 *
 *	 helper of the main game UI with providing the building expansion items and options.
 */
UCLASS()
class RTS_SURVIVAL_API UW_BottomCenterUI : public UUserWidget
{
	GENERATED_BODY()

	friend class UW_BuildingUI_ItemPanel;
	friend class UW_BuildingUI_OptionPanel;

public:
	void RebuildSelectionUI(const TArray<FSelectedUnitsWidgetState>& AllWidgetStates, int32 ActivePageIndex);

	void InitBottomCenterUI(UMainGameUI* MainGameUI, ACPPController* PlayerController);

	/**
     * @brief Populate the option slots per section and collapse sections that have no options.
     * @param Options All options (already unlocked) of the selected owner.
     * @param BxpOwner The owner (used for socket/unique checks).
     */
	void SetupBxpOptionsPerSection(const TArray<FBxpOptionData>& Options,
	                               const IBuildingExpansionOwner* BxpOwner);

	/**
	 * @brief Shift the options panel vertically based on height delta vs base.
	 * @param DeltaYFromBase Negative => collapsed (panel smaller) -> move down; positive -> move up; zero -> restore base.
	 */
	void AdjustOptionPanelYOffsetByDelta(const float DeltaYFromBase);


	/**
	 * @post The CancelBuilding button is shown instead of the construct building button.
	 */
	void ShowCancelBuilding() const;

	/**
	 * @post The construct building button is shown.
	 */
	void ShowConstructBuilding() const;

	/**
	 * @post The convert to vehicle button is shown.
	 */
	void ShowConvertToVehicle() const;

	/**
	 * @post The CancelBuilding button is shown instead of the construct building button.
	 */
	void ShowCancelVehicleConversion() const;

	void UpdateBuildingUIForNewUnit(const bool bShowBuildingUI,
	                                const EActionUINomadicButton NomadicButtonState,
	                                const ENomadicSubtype NomadicSubtype);


	UFUNCTION(BlueprintCallable, NotBlueprintable)
	TArray<UW_ItemBuildingExpansion*> GetBxpItemsInItemPanel();

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	TArray<UW_OptionBuildingExpansion*> GetBxpOptionsInPanel();

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void InitArraysOnChildPanels();

	void SetBuildingUIVisibility(const bool bVisible);
	// Only to be called by main game UI; if the option UI is hidden we also need to reset the active bxp index in it.
	void SetBuildingOptionUIVisibility(const bool bVisible) const;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UW_SelectionPanel* SelectionPanel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UW_BuildingUI_ItemPanel* BuildingUI_ItemPanel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UW_BuildingUI_OptionPanel* BuildingUI_OptionPanel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UBorder* BuildingUIBorder;

	// Set this in Blueprint to the pixel height of exactly one option row.
	UPROPERTY(BlueprintReadWrite, Category="Layout")
	float M_OptionRowHeight = 80.f;

private:
	bool EnsureIsValidChildPanels() const;
	bool EnsureIsValidSelectionPanel() const;
	bool EnsureIsValidBorderPanel() const;

	// ----------------------------------------------
	// ------------------ BXP Options Helper--------
	// ----------------------------------------------


	/** Groups incoming options by their UI section. */
	void SetupBxpOptionsPerSection_GroupBySection(
		const TArray<FBxpOptionData>& Options,
		TMap<EBxpOptionSection, TArray<FBxpOptionData>>& OutBySection) const;


	// ----------------------------------------------
	// ------------------ Building UI specifics------
	// ----------------------------------------------

	// Called by BuidlingUI_ItemPanel when the cancel building button is clicked.
	void CancelBuilding();
	void ConstructBuilding();
	void ConvertToVehicle();
	void CancelVehicleConversion();

	UPROPERTY()
	TObjectPtr<UMainGameUI> M_MainGameUI = nullptr;
	bool EnsureMainGameUIIsValid() const;

	AActor* GetPrimarySelectedActor() const;
	/** @return Whether the actor that is primary selected in the main UI is valid. */
	bool HasMainUIValidSelectedActor() const;

	UPROPERTY()
	TWeakObjectPtr<ACPPController> M_PlayerController;
	bool EnsureIsValidPlayerController();

	/** Cached base Y offset for the options panel when ALL slots are visible (no collapses). */
	UPROPERTY()
	float M_BaseOptionPanelYOffset = 0.f;

	/** Whether we've cached the base Y offset already. */
	UPROPERTY()
	bool bM_HasCachedBaseOptionPanelYOffset = false;

	/** Gets the options panel's canvas slot. Logs + returns nullptr if invalid. */
	UCanvasPanelSlot* GetOptionPanelCanvasSlot() const;

	/** Cache the base Y offset once at initialization when the panel is at full height. */
	void CacheOptionPanelBaseYOffset();
};
