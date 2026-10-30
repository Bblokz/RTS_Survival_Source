// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "W_SurvivalArmySlot.generated.h"

class UBorder;
class UImage;
class UMaterialInstanceDynamic;
class UOverlay;
class USizeBox;
class UTextBlock;
class USurvivalUiStyleAsset;
class UW_SurvivalArmySelection;
struct FSurvivalRosterItem;
struct FSurvivalUiStyle;

/** How a slot draws the drag that hovers it. */
UENUM()
enum class ESurvivalSlotDropPreview : uint8
{
	None,
	Accept,
	Refuse
};

/**
 * @brief One army slot of the survival army selection: drop target for roster entries and other slots, drag source
 * for its own unit (drop outside any slot removes it), right-click removes it. Created and laid out by
 * UW_SurvivalArmySelection, which owns all army state; the slot only draws and forwards input. Builds its own
 * widget tree at the native 120x164 tile size and scales it into the grid cell it is given. Also used, hit-test
 * invisible, as the drag visual.
 */
UCLASS()
class RTS_SURVIVAL_API UW_SurvivalArmySlot : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Call once right after creating the slot.
	 * @param Owner Receives drops, drags and remove requests; null for a drag visual.
	 * @param SlotIndex Index in the army; 0 is the locked HQ slot.
	 * @param StyleAsset Brushes, fonts and colours; kept alive by the owner.
	 */
	void SetupSlot(UW_SurvivalArmySelection* Owner, const int32 SlotIndex, const USurvivalUiStyleAsset* StyleAsset);

	/**
	 * @brief Shows a unit or the empty look.
	 * @param Item The unit in the slot; null for an empty slot.
	 * @param bIsLocked True for the HQ slot: no drag, no remove, locked look.
	 */
	void SetSlotContent(const FSurvivalRosterItem* Item, const bool bIsLocked);

	/** @brief Grid cell width in UI units; the height follows the native aspect. Small cells hide the name. */
	void SetCellWidth(const float CellWidth);

	void SetDropPreview(const ESurvivalSlotDropPreview Preview);

	/** @brief For the drag visual: never takes the mouse, so the drop reaches the slot under the cursor. */
	void SetIsDragVisual();

	int32 GetSlotIndex() const { return M_SlotIndex; }

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent,
	                                  UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
	                          UDragDropOperation* InOperation) override;
	virtual void NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
	                               UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

private:
	UPROPERTY()
	TWeakObjectPtr<UW_SurvivalArmySelection> M_Owner;

	UPROPERTY()
	TObjectPtr<const USurvivalUiStyleAsset> M_StyleAsset;

	UPROPERTY()
	TObjectPtr<USizeBox> M_CellBox;

	UPROPERTY()
	TObjectPtr<UBorder> M_Background;

	UPROPERTY()
	TObjectPtr<UBorder> M_IconWell;

	UPROPERTY()
	TObjectPtr<UImage> M_Icon;

	UPROPERTY()
	TObjectPtr<UImage> M_WearImage;

	// This slot's copy of the wear material; the wear brush points at it.
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> M_WearMaterial;

	UPROPERTY()
	TObjectPtr<UBorder> M_SupplyBadge;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_SupplyText;

	UPROPERTY()
	TObjectPtr<UBorder> M_VariantBadge;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_VariantText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_NameText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_EmptyText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_HqText;

	UPROPERTY()
	TObjectPtr<UBorder> M_DropHighlight;

	int32 M_SlotIndex = INDEX_NONE;
	bool bM_IsFilled = false;
	bool bM_IsLocked = false;
	bool bM_IsHovered = false;
	// Small cells collapse the name; remembered so SetSlotContent keeps it collapsed.
	bool bM_HideName = false;

	void BuildWidgetTree();
	void BuildIconWell(UOverlay* SlotOverlay);
	void BuildBadges(UOverlay* SlotOverlay);
	void BuildTexts(UOverlay* SlotOverlay);
	void ApplyBackgroundLook();
	void ApplyTextStyles();
	void ApplyWear(const float WearSeed);
	void ApplyNameVisibility();

	const FSurvivalUiStyle& GetStyle() const;
	bool GetCanDragOut() const { return bM_IsFilled and not bM_IsLocked; }

	bool GetIsValidOwner() const;
	bool GetIsValidBackground() const;
};
