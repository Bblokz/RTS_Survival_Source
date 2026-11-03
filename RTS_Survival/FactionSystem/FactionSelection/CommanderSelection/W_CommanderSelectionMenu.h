// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CommanderSelectionData.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantSelectionStyle.h"

#include "W_CommanderSelectionMenu.generated.h"

class AFactionPlayerController;
class UBorder;
class UButton;
class UCanvasPanel;
class UCanvasPanelSlot;
class UDataTable;
class UGlobalAbilitySetDataAsset;
class UImage;
class UOverlay;
class UScrollBox;
class UTextBlock;
class UTexture2D;
class UVerticalBox;
class UW_CommanderAbilityRow;
class UW_CommanderSelectionEntry;
struct FStreamableHandle;

/**
 * @brief Shown by AFactionPlayerController after the faction (and unit variant) choice: lists the commanders with
 * their portrait and six-ability kit and hands the chosen commander back on Confirm. Builds its own widget tree; a
 * Blueprint child only fills M_Style with the skin brushes of the variant selection screen.
 */
UCLASS()
class RTS_SURVIVAL_API UW_CommanderSelectionMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Fills the screen and selects the current commander, or the first one when none was chosen yet.
	 * @param FactionPlayerController Receives Confirm and Back.
	 * @param CurrentCommander The game instance's commander; NoCommander selects the first in the list.
	 */
	void InitCommanderSelection(AFactionPlayerController* FactionPlayerController,
	                            const ERTSCommander CurrentCommander);

	void HandleCommanderEntryClicked(const int32 EntryIndex);

	/** Shows a commander without confirming it; false when the commander is not in the list. */
	UFUNCTION(BlueprintCallable, Category = "Commander Selection")
	bool SelectCommander(const ERTSCommander Commander);

	UFUNCTION(BlueprintPure, Category = "Commander Selection")
	ERTSCommander GetSelectedCommander() const;

	/** Same as the Confirm button: hands the selected commander to the faction player controller. */
	UFUNCTION(BlueprintCallable, Category = "Commander Selection")
	void ConfirmSelection();

	UFUNCTION(BlueprintPure, Category = "Commander Selection")
	int32 GetCommanderCount() const { return M_Commanders.Num(); }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	// Up/Down pick the commander, Enter or Space confirms, Escape goes back; the gamepad d-pad, A, Start and B too.
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// The variant selection screen's skin; copy the class defaults of its Blueprint child.
	UPROPERTY(EditDefaultsOnly, Category = "Commander Selection")
	FUnitVariantSelectionStyle M_Style;

private:
	UPROPERTY()
	TWeakObjectPtr<AFactionPlayerController> M_FactionPlayerController;

	// Keeps the ability templates, and so the icons the ability rows show, loaded.
	UPROPERTY()
	TObjectPtr<UGlobalAbilitySetDataAsset> M_AbilityDataAsset;

	UPROPERTY()
	TObjectPtr<UDataTable> M_RichTextStyle;

	UPROPERTY()
	TArray<FCommanderSelectionEntryData> M_Commanders;

	// Index matches M_Commanders; null until loaded or when the commander has no portrait.
	UPROPERTY()
	TArray<TObjectPtr<UTexture2D>> M_LoadedPortraits;

	TSharedPtr<FStreamableHandle> M_PortraitLoadHandle;

	int32 M_SelectedIndex = INDEX_NONE;

	UPROPERTY()
	TArray<TObjectPtr<UW_CommanderSelectionEntry>> M_Entries;

	UPROPERTY()
	TObjectPtr<UImage> M_EdgeShadeLeft;

	UPROPERTY()
	TObjectPtr<UImage> M_EdgeShadeRight;

	UPROPERTY()
	TObjectPtr<UBorder> M_HeaderPlate;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_TitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_SubtitleText;

	UPROPERTY()
	TObjectPtr<UBorder> M_CommanderPanel;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_CommanderPanelTitle;

	UPROPERTY()
	TObjectPtr<UScrollBox> M_CommanderList;

	UPROPERTY()
	TObjectPtr<UBorder> M_KitPanel;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_KitPanelTitle;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_KitCountText;

	UPROPERTY()
	TObjectPtr<UVerticalBox> M_AbilityColumn;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_FootnoteText;

	UPROPERTY()
	TObjectPtr<UBorder> M_PortraitFrame;

	UPROPERTY()
	TObjectPtr<UImage> M_PortraitImage;

	// Shown in the frame while the commander has no (loaded) portrait.
	UPROPERTY()
	TObjectPtr<UTextBlock> M_PortraitPendingText;

	UPROPERTY()
	TObjectPtr<UBorder> M_DescriptionPlate;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_CommanderNameText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_CommanderDescriptionText;

	UPROPERTY()
	TObjectPtr<UButton> M_BackButton;

	UPROPERTY()
	TObjectPtr<UButton> M_ConfirmButton;

	void BuildWidgetTree();
	void BuildEdgeShades(UCanvasPanel* RootCanvas);
	void BuildHeader(UCanvasPanel* RootCanvas);
	void BuildCommanderPanel(UCanvasPanel* RootCanvas);
	void BuildKitPanel(UCanvasPanel* RootCanvas);
	void BuildPortrait(UCanvasPanel* RootCanvas);
	void BuildDescription(UCanvasPanel* RootCanvas);
	void BuildButtons(UCanvasPanel* RootCanvas);

	/**
	 * @brief A side column: fixed size panel plate with a title and optional count in its header strip.
	 * @param RootCanvas The screen canvas the column goes on.
	 * @param PanelSize Drawn size the panel instance was made for.
	 * @param OutPanelSlot Receives the canvas slot, so the caller anchors the column.
	 * @param OutTitleText Receives the title text block (left of the strip).
	 * @param OutCountText Receives the count text block (right of the strip).
	 * @param OutPanelPlate Receives the plate, so ApplyStyle can set its brush.
	 * @return The overlay to put the body in, under the header strip.
	 */
	UOverlay* BuildSidePanel(UCanvasPanel* RootCanvas, const FVector2D& PanelSize, UCanvasPanelSlot*& OutPanelSlot,
	                         TObjectPtr<UTextBlock>& OutTitleText, TObjectPtr<UTextBlock>& OutCountText,
	                         TObjectPtr<UBorder>& OutPanelPlate);

	void ApplyStyle();
	void LoadAbilityData();
	void PopulateEntries();
	void LoadPortraits();
	void HandlePortraitsLoaded();

	void SelectEntry(const int32 EntryIndex);
	void RefreshPortrait();
	void RebuildAbilityRows();

	/** @return True when the key was one of the screen's navigation keys. */
	bool HandleNavigationKey(const FKey& Key);
	void SelectAdjacentEntry(const int32 Step);

	UFUNCTION()
	void OnConfirmClicked();

	UFUNCTION()
	void OnBackClicked();

	bool GetIsValidFactionPlayerController() const;
	bool GetIsValidAbilityColumn() const;
	bool GetIsValidPortraitImage() const;
};
