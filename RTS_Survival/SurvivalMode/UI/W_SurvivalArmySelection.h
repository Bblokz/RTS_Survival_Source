// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/SurvivalMode/Army/SurvivalArmyTypes.h"
#include "SurvivalArmyUiTypes.h"
#include "W_SurvivalArmySlot.h"

#include "W_SurvivalArmySelection.generated.h"

class ASurvivalModeDirector;
class UBorder;
class UButton;
class UDataTable;
class UDragDropOperation;
class UHorizontalBox;
class UProgressBar;
class UScrollBox;
class USurvivalArmyDragOperation;
class USurvivalUiStyleAsset;
class UTextBlock;
class UTexture2D;
class UUniformGridPanel;
class UUnitVariantDataAsset;
class UW_SurvivalArmyTab;
class UW_SurvivalRosterEntry;
struct FSurvivalUiStyle;

/**
 * @brief Start army selection: drag faction units (as their selected variants) into the army slots within the
 * supply budget; slot 0 holds the locked HQ. Confirm calls ASurvivalModeDirector::ConfirmArmy.
 * The Blueprint child (WBP_SurvivalArmySelection, Docs/ui_specs/W_SurvivalArmySelection.json) holds the static
 * layout; roster entries, tabs and slots are created here. The slot grid resizes its cells to the slot count and
 * the area it gets (8, 16 and 40 slots all fill it; past ~100 it scrolls).
 * @note InitArmySelection: called by the director right after the widget is added to the viewport.
 * @note M_StyleAsset: set to DA_SurvivalUiStyle in the Blueprint class defaults.
 *
 * Bound widgets (names in the Blueprint must match):
 *   required: M_RosterScroll (ScrollBox), M_SlotScroll (ScrollBox), M_SlotGrid (UniformGridPanel inside
 *             M_SlotScroll), M_SupplyBar (ProgressBar), M_SupplyText (TextBlock), M_ConfirmButton (Button)
 *   optional: M_ScreenShade, M_HeaderPlate, M_RosterPanel, M_ArmyPanel (Border); M_TabRow (HorizontalBox);
 *             M_TitleText, M_FactionText, M_RosterTitleText, M_ArmyTitleText, M_SlotCountText, M_SupplyLabelText,
 *             M_FeedbackText, M_HintText, M_ConfirmText, M_ClearText (TextBlock); M_ClearButton (Button)
 */
UCLASS()
class RTS_SURVIVAL_API UW_SurvivalArmySelection : public UUserWidget
{
	GENERATED_BODY()

public:
	/** @brief Called by the director right after the widget is added to the viewport. */
	virtual void InitArmySelection(ASurvivalModeDirector* Director);

	// ------------------------------------------------------------------ Called by the roster entries, tabs and slots
	void HandleTabClicked(const ESurvivalRosterCategory Category);

	/** @brief Double-click on a roster entry: put it in the first free slot, or explain why not. */
	void HandleRosterEntryActivated(const int32 RosterIndex);

	/** @brief Right-click on a slot or its unit dragged outside every slot. The HQ slot ignores it. */
	void HandleSlotRemoveRequested(const int32 SlotIndex);

	/** @brief A drag was dropped on a slot: adds, replaces or swaps when allowed, otherwise shows why not. */
	void HandleDropOnSlot(const USurvivalArmyDragOperation* Operation, const int32 SlotIndex);

	/** @return Whether a drop of this drag on the slot would be accepted, for the hover tint. */
	ESurvivalSlotDropPreview GetDropPreview(const USurvivalArmyDragOperation* Operation, const int32 SlotIndex) const;

	UDragDropOperation* CreateDragOperationFromRoster(const int32 RosterIndex);
	UDragDropOperation* CreateDragOperationFromSlot(const int32 SlotIndex);

	// ------------------------------------------------------------------ Queries and actions
	UFUNCTION(BlueprintPure, Category = "Survival|Army Selection")
	int32 GetUsedSupply() const;

	UFUNCTION(BlueprintPure, Category = "Survival|Army Selection")
	int32 GetSupplyBudget() const { return M_SupplyBudget; }

	UFUNCTION(BlueprintPure, Category = "Survival|Army Selection")
	int32 GetSlotCount() const { return M_SlotRosterIndices.Num(); }

	/** @return The army as the director expects it: one pick per filled slot, HQ in slot 0. */
	TArray<FSurvivalArmyPick> BuildPicks() const;

	/** @brief Empties every slot except the HQ slot. */
	UFUNCTION(BlueprintCallable, Category = "Survival|Army Selection")
	void ClearArmy();

	/** @brief Sends the picks to the director; shows its reason when it refuses. */
	UFUNCTION(BlueprintCallable, Category = "Survival|Army Selection")
	void ConfirmArmy();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;
	// Enter (or gamepad Start) confirms, Delete/Backspace clears.
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Style")
	TObjectPtr<USurvivalUiStyleAsset> M_StyleAsset = nullptr;

	// Designer only: how many placeholder slots the widget designer (and analyze_widget_layout) shows.
	UPROPERTY(EditAnywhere, Category = "Design Preview", meta = (ClampMin = "2", ClampMax = "256"))
	int32 M_DesignPreviewSlotCount = 12;

	// Designer only: placeholder roster entries shown in the widget designer.
	UPROPERTY(EditAnywhere, Category = "Design Preview", meta = (ClampMin = "0", ClampMax = "40"))
	int32 M_DesignPreviewRosterCount = 8;

	// ------------------------------------------------------------------ Bound widgets
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> M_RosterScroll = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> M_SlotScroll = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UUniformGridPanel> M_SlotGrid = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> M_SupplyBar = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> M_SupplyText = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_ConfirmButton = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> M_ScreenShade = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> M_HeaderPlate = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> M_RosterPanel = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> M_ArmyPanel = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> M_TabRow = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> M_TitleText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> M_FactionText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> M_RosterTitleText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> M_ArmyTitleText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> M_SlotCountText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> M_SupplyLabelText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> M_FeedbackText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> M_HintText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> M_ConfirmText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> M_ClearText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> M_ClearButton = nullptr;

private:
	TWeakObjectPtr<ASurvivalModeDirector> M_Director;

	// Every unit the player can bring, HQ excluded; roster entry i shows item i.
	UPROPERTY()
	TArray<FSurvivalRosterItem> M_Roster;

	// The HQ shown in slot 0; never in M_Roster.
	UPROPERTY()
	FSurvivalRosterItem M_HQItem;

	// One element per slot: index into M_Roster, INDEX_NONE when empty. Element 0 is the HQ slot and stays
	// INDEX_NONE; it always holds M_HQItem.
	TArray<int32> M_SlotRosterIndices;

	int32 M_SupplyBudget = 0;
	ESurvivalRosterCategory M_SelectedCategory = ESurvivalRosterCategory::All;
	bool bM_IsConfirmPending = false;

	UPROPERTY()
	TArray<TObjectPtr<UW_SurvivalRosterEntry>> M_RosterEntries;

	UPROPERTY()
	TArray<TObjectPtr<UW_SurvivalArmyTab>> M_Tabs;

	UPROPERTY()
	TArray<TObjectPtr<UW_SurvivalArmySlot>> M_Slots;

	// Keeps the table brushes, variant brushes and variant icon textures the roster and slots show loaded.
	UPROPERTY()
	TObjectPtr<const UDataTable> M_TrainingOptionsTable;

	UPROPERTY()
	TObjectPtr<const UUnitVariantDataAsset> M_VariantDataAsset;

	UPROPERTY()
	TArray<TObjectPtr<UTexture2D>> M_LoadedIcons;

	// Slot grid area of the last layout pass; the grid is re-laid out when the scroll box size changes.
	FVector2D M_LastSlotAreaSize = FVector2D::ZeroVector;
	float M_CellWidth = 0.f;

	FTimerHandle M_FeedbackTimerHandle;

	// ---- Setup
	void ApplyStyleToBoundWidgets();
	void ApplyStaticTexts();
	void BindButtons();
	void BuildRoster(const ASurvivalModeDirector& Director);
	FSurvivalRosterItem MakeRosterItem(const FSurvivalArmyEntry& Entry);
	FSlateBrush MakeIconBrush(const FTrainingOption& Unit, const EUnitVariant Variant);
	FText MakeVariantName(const FTrainingOption& Unit, const EUnitVariant Variant) const;
	FText MakeTooltip(const FSurvivalArmyEntry& Entry, const FText& VariantName) const;
	FText MakeStatLine(const FTrainingOption& Unit) const;
	void RebuildTabs();
	void RebuildRosterEntries();
	void RebuildSlots(const int32 SlotCount);
	void BuildDesignPreview();

	// ---- Slot grid
	void LayoutSlotGrid();
	FVector2D GetSlotAreaSize() const;

	// ---- Army rules
	bool GetCanPlaceRosterItem(const int32 RosterIndex, const int32 SlotIndex, FText& OutReason) const;
	bool GetCanMoveSlot(const int32 SourceSlotIndex, const int32 TargetSlotIndex, FText& OutReason) const;
	int32 GetCopiesInArmy(const int32 RosterIndex) const;
	int32 GetSupplyOfSlot(const int32 SlotIndex) const;
	int32 FindFirstEmptySlot() const;

	// ---- Views
	void RefreshArmyViews();
	void RefreshSlots();
	void RefreshSupply();
	void RefreshRosterStates();
	void ApplyRosterFilter();
	void ShowFeedback(const FText& Message, const bool bIsWarning);
	void ClearFeedback();

	UFUNCTION()
	void OnConfirmClicked();

	UFUNCTION()
	void OnClearClicked();

	const FSurvivalUiStyle& GetStyle() const;
	bool GetIsValidDirector() const;
	bool GetIsValidSlotGrid() const;
	bool GetIsValidRosterScroll() const;
};
