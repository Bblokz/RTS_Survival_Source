// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/GameUI/MiniMap/MapObjectFilters/RTSMapObjectFilterTypes.h"
#include "W_MapObjectFilterToggle.generated.h"

class UButton;
class UHorizontalBox;
class UImage;
class UMaterialInstanceDynamic;
class UTextBlock;
class UTexture2D;
class URTSMapObjectFilterSubsystem;

/**
 * @brief One row of the tactical map's filter panel: a colour swatch, a label and three glass buttons (off,
 * tactical map only, minimap and tactical map); the selected one is lit. UW_FullMapView creates one per filter;
 * the mode lives in URTSMapObjectFilterSubsystem, so it survives closing the map. Builds its own widget tree.
 * Button faces come from map_filter_buttons.py; M_UI_GlassPane is drawn over each for the glass and scratches.
 */
UCLASS()
class RTS_SURVIVAL_API UW_MapObjectFilterToggle : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Binds the row to a filter and shows the filter's current mode.
	 * @param Filter The filter this row switches.
	 * @param Label Text shown next to the swatch.
	 * @param SwatchColor Colour of the filter's dots, so the row doubles as a legend.
	 */
	void SetupFilterToggle(const ERTSMapObjectFilter Filter, const FText& Label, const FLinearColor& SwatchColor);

	/** Re-reads the mode from the subsystem (the map opens again, or another source changed it). */
	void RefreshSelectedMode();

protected:
	virtual void NativeOnInitialized() override;

private:
	ERTSMapObjectFilter M_Filter = ERTSMapObjectFilter::Radixite;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_LabelText;

	UPROPERTY()
	TObjectPtr<UImage> M_Swatch;

	// Indexed by ERTSMapObjectFilterMode.
	UPROPERTY()
	TArray<TObjectPtr<UButton>> M_OptionButtons;

	// Button faces per mode: unlit and lit (the selected option).
	UPROPERTY()
	TArray<TObjectPtr<UTexture2D>> M_DimTextures;

	UPROPERTY()
	TArray<TObjectPtr<UTexture2D>> M_LitTextures;

	// One glass pane per button, each with its own scratch seed so the three panes do not look copied.
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> M_GlassPaneMaterials;

	void BuildWidgetTree();
	UHorizontalBox* BuildHeaderRow();
	UHorizontalBox* BuildOptionRow();
	void LoadOptionTextures();
	void AddOptionButton(UHorizontalBox& OptionRow, const ERTSMapObjectFilterMode Mode, const FText& OptionTooltip);
	void BindOptionClick(UButton& OptionButton, const ERTSMapObjectFilterMode Mode);
	UImage* CreateGlassPane(const int32 ModeIndex);

	void ApplyOptionStyles(const ERTSMapObjectFilterMode SelectedMode);

	UFUNCTION()
	void OnOffClicked();

	UFUNCTION()
	void OnTacticalMapOnlyClicked();

	UFUNCTION()
	void OnMiniMapAndTacticalMapClicked();

	void SelectMode(const ERTSMapObjectFilterMode NewMode);

	URTSMapObjectFilterSubsystem* GetMapObjectFilterSubsystem() const;
};
