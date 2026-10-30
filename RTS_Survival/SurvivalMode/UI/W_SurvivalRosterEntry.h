// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "W_SurvivalRosterEntry.generated.h"

class UBorder;
class UImage;
class UOverlay;
class UTextBlock;
class USurvivalUiStyleAsset;
class UW_SurvivalArmySelection;
struct FSurvivalRosterItem;
struct FSurvivalUiStyle;

/**
 * @brief One unit in the army selection roster: icon, name, variant, supply cost and how many copies are in the army.
 * Drag it onto an army slot to add it, or double-click to put it in the first free slot. Created by
 * UW_SurvivalArmySelection, which owns the data; builds its own widget tree. Not a button on purpose: a button
 * captures mouse-down and drag detection would never fire.
 */
UCLASS()
class RTS_SURVIVAL_API UW_SurvivalRosterEntry : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Call once right after creating the entry.
	 * @param Owner Receives drags and double-clicks.
	 * @param RosterIndex Index of the item in the owner's roster.
	 * @param Item What to show.
	 * @param StyleAsset Brushes, fonts and colours; kept alive by the owner.
	 */
	void SetupEntry(UW_SurvivalArmySelection* Owner, const int32 RosterIndex, const FSurvivalRosterItem& Item,
	                const USurvivalUiStyleAsset* StyleAsset);

	/**
	 * @brief Refreshes the count line and dims the entry when it cannot be added now.
	 * @param CopiesInArmy How many slots hold this unit.
	 * @param bCanAdd False when over budget or at max copies; the entry stays draggable so a drop explains why.
	 */
	void SetArmyState(const int32 CopiesInArmy, const bool bCanAdd);

	int32 GetRosterIndex() const { return M_RosterIndex; }

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry,
	                                              const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent,
	                                  UDragDropOperation*& OutOperation) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

private:
	UPROPERTY()
	TWeakObjectPtr<UW_SurvivalArmySelection> M_Owner;

	UPROPERTY()
	TObjectPtr<const USurvivalUiStyleAsset> M_StyleAsset;

	UPROPERTY()
	TObjectPtr<UBorder> M_Background;

	UPROPERTY()
	TObjectPtr<UBorder> M_IconWell;

	UPROPERTY()
	TObjectPtr<UImage> M_Icon;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_NameText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_VariantText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_CountText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_SupplyText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_SupplyLabelText;

	int32 M_RosterIndex = INDEX_NONE;
	int32 M_MaxCopies = 0;
	bool bM_IsHovered = false;

	void BuildWidgetTree();
	void BuildIcon(UOverlay* EntryOverlay);
	void BuildTexts(UOverlay* EntryOverlay);
	void BuildSupply(UOverlay* EntryOverlay);
	void ApplyBackgroundLook();
	void ApplyTextStyles();

	const FSurvivalUiStyle& GetStyle() const;
	bool GetIsValidOwner() const;
	bool GetIsValidBackground() const;
};
