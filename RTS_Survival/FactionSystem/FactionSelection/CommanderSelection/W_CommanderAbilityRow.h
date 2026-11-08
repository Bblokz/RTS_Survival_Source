// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CommanderSelectionData.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantSelectionStyle.h"

#include "W_CommanderAbilityRow.generated.h"

class UBorder;
class UButton;
class UDataTable;
class UHorizontalBox;
class UImage;
class UMaterialInstanceDynamic;
class UOverlay;
class URichTextBlock;
class UTextBlock;
class UW_CommanderSelectionMenu;

/**
 * @brief One slot of the shown commander's kit on the commander selection screen: icon, title, tier badge and
 * description on a pane of glass. The whole row is one button: hovering lights the glass (the shared M_UI_GlassPane
 * hover and sweep), clicking opens the ability preview in the desert. Created by UW_CommanderSelectionMenu, which
 * calls SetupRow right after creating it.
 */
UCLASS()
class RTS_SURVIVAL_API UW_CommanderAbilityRow : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Fills the row; an ability without a template shows a question mark and a grey tier badge, and has no
	 * preview.
	 * @param Menu Receives the click.
	 * @param SlotIndex Index of the slot in the shown commander's kit.
	 * @param Style Brushes, fonts and colours of the selection screens.
	 * @param RichTextStyle The <Style> tags the ability descriptions use; may be null.
	 * @param AbilitySlot The slot to show.
	 */
	void SetupRow(UW_CommanderSelectionMenu* Menu, const int32 SlotIndex, const FUnitVariantSelectionStyle& Style,
	              UDataTable* RichTextStyle, const FCommanderSelectionAbilitySlot& AbilitySlot);

	/** The button of the row; the PIE drivers broadcast its click. */
	UButton* GetRowButton() const { return M_RowButton; }

	/** The Hover value last sent to the glass shader (0 idle, 1 fully lit); the test checks the glass lights up. */
	float GetGlassHoverAmount() const { return FMath::Max(M_SentHover, 0.f); }

	/** The glass instance the row's dynamic material was made from (MI_FactionButton_Glass); null without glass. */
	UMaterialInterface* GetGlassSourceMaterial() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY()
	TWeakObjectPtr<UW_CommanderSelectionMenu> M_Menu;

	int32 M_SlotIndex = INDEX_NONE;
	bool bM_CanPreview = false;

	UPROPERTY()
	TObjectPtr<UButton> M_RowButton;

	UPROPERTY()
	TObjectPtr<UImage> M_GlassPane;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> M_GlassMaterial;

	UPROPERTY()
	TObjectPtr<UBorder> M_IconWell;

	UPROPERTY()
	TObjectPtr<UImage> M_Icon;

	// Shown in the well instead of the icon while the ability has no template.
	UPROPERTY()
	TObjectPtr<UTextBlock> M_MissingIconText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_TitleText;

	UPROPERTY()
	TObjectPtr<UBorder> M_TierBadge;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_TierText;

	UPROPERTY()
	TObjectPtr<URichTextBlock> M_DescriptionText;

	// "PREVIEW" under the badge; lights up with the glass while the row is hovered.
	UPROPERTY()
	TObjectPtr<UTextBlock> M_PreviewHintText;

	// Hover, sweep and press values the glass shader gets (see UW_ControlGroupImage for the same animation).
	float M_HoverRamp = 0.f;
	float M_SweepProgress = 1.f;
	float M_PressRamp = 0.f;
	bool bM_WasHovered = false;
	float M_SentHover = -1.f;
	float M_SentSweep = -1.f;
	float M_SentPressed = -1.f;

	void BuildWidgetTree();
	void BuildGlassPane(UOverlay* RowOverlay);
	void BuildIconWell(UHorizontalBox* RowBox);
	void BuildTexts(UHorizontalBox* RowBox);
	UWidget* BuildTierBadge();
	void ApplyIcon(const FCommanderSelectionAbilitySlot& AbilitySlot);
	void ApplyTierBadge(const FUnitVariantSelectionStyle& Style, const FCommanderSelectionAbilitySlot& AbilitySlot);
	void ApplyGlassWear(const int32 SlotIndex);
	void TickGlass(const float DeltaSeconds);

	UFUNCTION()
	void OnRowClicked();

	bool GetIsValidDescriptionText() const;
};
