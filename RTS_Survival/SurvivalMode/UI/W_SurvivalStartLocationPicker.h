// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "W_SurvivalStartLocationPicker.generated.h"

class ASurvivalModeDirector;
class ASurvivalStartLocation;
class UBorder;
class UButton;
class UHorizontalBox;
class UMaterialInterface;
class USurvivalUiStyleAsset;
class UTextBlock;
class UWidget;

/**
 * @brief Lets the player cycle the survival start locations (camera follows via
 * ASurvivalModeDirector::PreviewStartLocation) and confirm one (ConfirmStartLocation). Prev/Next buttons, Left/Right
 * (A/D, gamepad d-pad) cycle, Enter (gamepad A) confirms. The Blueprint child holds the layout
 * (Docs/ui_specs/W_SurvivalStartLocationPicker.json).
 * @note InitStartLocationPicker: called by the director right after the widget is added to the viewport.
 * @note M_StyleAsset: set to DA_SurvivalUiStyle in the Blueprint class defaults.
 *
 * Bound widgets (names in the Blueprint must match):
 *   required: M_LocationNameText (TextBlock), M_PreviousButton, M_NextButton, M_ConfirmButton (Button)
 *   optional: M_PanelBorder (Border); M_TitleText, M_LocationIndexText, M_HintText, M_ConfirmText (TextBlock)
 *   optional: M_ResourcesText, M_SpecialText (TextBlock): close radixite/metal nodes and the location's special;
 *   created under M_LocationIndexText's panel when the Blueprint does not have them. M_ResourcesText is the label
 *   of a C++ built row with the radixite/metal glass icons and counts (moved into that row when bound).
 */
UCLASS()
class RTS_SURVIVAL_API UW_SurvivalStartLocationPicker : public UUserWidget
{
	GENERATED_BODY()

public:
	/** @brief Called by the director right after the widget is added to the viewport. */
	virtual void InitStartLocationPicker(ASurvivalModeDirector* Director);

	UFUNCTION(BlueprintCallable, Category = "Survival|Start Location")
	void SelectNextLocation();

	UFUNCTION(BlueprintCallable, Category = "Survival|Start Location")
	void SelectPreviousLocation();

	/** @brief Shows and previews the location at this index (wraps around). */
	UFUNCTION(BlueprintCallable, Category = "Survival|Start Location")
	void SelectLocationByIndex(const int32 LocationIndex);

	/**
	 * @brief For a world-click handler: selects the clicked start location if it is one of the director's.
	 * @return False when the actor is not a known start location.
	 */
	UFUNCTION(BlueprintCallable, Category = "Survival|Start Location")
	bool SelectLocationActor(ASurvivalStartLocation* StartLocation);

	UFUNCTION(BlueprintCallable, Category = "Survival|Start Location")
	void ConfirmSelectedLocation();

	UFUNCTION(BlueprintPure, Category = "Survival|Start Location")
	int32 GetSelectedLocationIndex() const { return M_SelectedIndex; }

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Style")
	TObjectPtr<USurvivalUiStyleAsset> M_StyleAsset = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Survival|Start Location", meta = (BindWidget))
	TObjectPtr<UTextBlock> M_LocationNameText = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_PreviousButton = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_NextButton = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_ConfirmButton = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> M_PanelBorder = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> M_TitleText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> M_LocationIndexText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> M_HintText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> M_ConfirmText = nullptr;

	// Label of the close radixite and metal nodes row ("Near the drop-off:  [icon] 6  [icon] 2"); holds the whole
	// line as words ("Near the drop-off: 6 radixite, 2 metal") when a resource icon material fails to load.
	UPROPERTY(BlueprintReadOnly, Category = "Survival|Start Location", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> M_ResourcesText = nullptr;

	// The location's special: label and one line ("Hangar: 3 capturable Panzer II Luchs").
	UPROPERTY(BlueprintReadOnly, Category = "Survival|Start Location", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> M_SpecialText = nullptr;

private:
	TWeakObjectPtr<ASurvivalModeDirector> M_Director;

	// The director's start locations that were valid when the picker opened, in the director's order.
	TArray<TWeakObjectPtr<ASurvivalStartLocation>> M_Locations;

	int32 M_SelectedIndex = INDEX_NONE;
	bool bM_IsConfirmed = false;

	// Same glass dome icons as the top resource bar; null when the material failed to load.
	UPROPERTY()
	TObjectPtr<UMaterialInterface> M_RadixiteIconMaterial = nullptr;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> M_MetalIconMaterial = nullptr;

	// One centred line: M_ResourcesText label, radixite icon + count, metal icon + count.
	UPROPERTY()
	TObjectPtr<UHorizontalBox> M_ResourcesRow = nullptr;

	// Only created with the icon row; null means the counts are written as words into M_ResourcesText.
	UPROPERTY()
	TObjectPtr<UTextBlock> M_RadixiteCountText = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_MetalCountText = nullptr;

	void ApplyStyleToBoundWidgets();
	void BindButtons();
	void RefreshTexts();
	void RefreshSummaryTexts();
	/** @brief Adds the summary widgets under the location index text when the Blueprint has none. */
	void EnsureSummaryTexts();
	UTextBlock* CreateSummaryText(int32 InsertIndexOffset);

	/**
	 * @brief Puts a summary widget in the panel of the location index text (or name text) so it follows the name.
	 * @param Widget The new summary widget.
	 * @param InsertIndexOffset Children after the anchor text to insert at.
	 * @return False when there is no panel to insert into.
	 */
	bool InsertSummaryWidget(UWidget* Widget, int32 InsertIndexOffset);

	void LoadResourceIconMaterials();

	/** @brief Builds the resources row with the icons; without both icon materials M_ResourcesText stays plain. */
	void CreateResourcesRow();

	/** @brief Takes M_ResourcesText's place in its panel with the row, or inserts the row after the anchor text. */
	bool PlaceResourcesRow(UHorizontalBox* Row);

	/**
	 * @brief Adds one resource icon and its count text to the resources row.
	 * @param IconMaterial Glass icon material of the resource.
	 * @param LeadingGap Space before the icon, separating it from the label or the previous count.
	 * @return The count text that RefreshSummaryTexts fills; null without a row.
	 */
	UTextBlock* AddResourceIconAndCount(UMaterialInterface* IconMaterial, float LeadingGap);

	bool GetHasResourceIconRow() const;
	FText GetLocationName(const int32 LocationIndex) const;

	UFUNCTION()
	void OnPreviousClicked();

	UFUNCTION()
	void OnNextClicked();

	UFUNCTION()
	void OnConfirmClicked();

	bool GetIsValidDirector() const;
};
