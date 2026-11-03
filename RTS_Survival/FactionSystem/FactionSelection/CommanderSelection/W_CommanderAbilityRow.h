// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CommanderSelectionData.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantSelectionStyle.h"

#include "W_CommanderAbilityRow.generated.h"

class UBorder;
class UDataTable;
class UHorizontalBox;
class UImage;
class URichTextBlock;
class UTextBlock;

/**
 * @brief One slot of the shown commander's kit on the commander selection screen: icon, title, tier badge and
 * description. Created by UW_CommanderSelectionMenu, which calls SetupRow right after creating it.
 */
UCLASS()
class RTS_SURVIVAL_API UW_CommanderAbilityRow : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Fills the row; an ability without a template shows a question mark and a grey tier badge.
	 * @param Style Brushes, fonts and colours of the selection screens.
	 * @param RichTextStyle The <Style> tags the ability descriptions use; may be null.
	 * @param AbilitySlot The slot to show.
	 */
	void SetupRow(const FUnitVariantSelectionStyle& Style, UDataTable* RichTextStyle,
	              const FCommanderSelectionAbilitySlot& AbilitySlot);

protected:
	virtual void NativeOnInitialized() override;

private:
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

	void BuildWidgetTree();
	void BuildIconWell(UHorizontalBox* RowBox);
	void BuildTexts(UHorizontalBox* RowBox);
	UWidget* BuildTierBadge();
	void ApplyIcon(const FCommanderSelectionAbilitySlot& AbilitySlot);
	void ApplyTierBadge(const FUnitVariantSelectionStyle& Style, const FCommanderSelectionAbilitySlot& AbilitySlot);

	bool GetIsValidDescriptionText() const;
};
