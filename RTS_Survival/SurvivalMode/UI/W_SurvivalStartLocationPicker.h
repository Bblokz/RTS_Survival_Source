// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "W_SurvivalStartLocationPicker.generated.h"

class ASurvivalModeDirector;
class ASurvivalStartLocation;
class UBorder;
class UButton;
class USurvivalUiStyleAsset;
class UTextBlock;

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

	UPROPERTY(meta = (BindWidget))
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

private:
	TWeakObjectPtr<ASurvivalModeDirector> M_Director;

	// The director's start locations that were valid when the picker opened, in the director's order.
	TArray<TWeakObjectPtr<ASurvivalStartLocation>> M_Locations;

	int32 M_SelectedIndex = INDEX_NONE;
	bool bM_IsConfirmed = false;

	void ApplyStyleToBoundWidgets();
	void BindButtons();
	void RefreshTexts();
	FText GetLocationName(const int32 LocationIndex) const;

	UFUNCTION()
	void OnPreviousClicked();

	UFUNCTION()
	void OnNextClicked();

	UFUNCTION()
	void OnConfirmClicked();

	bool GetIsValidDirector() const;
};
