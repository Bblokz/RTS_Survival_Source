// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DeckBuilderTypes.h"

#include "W_DeckSectionEntry.generated.h"

class UButton;
class UCanvasPanel;
class UImage;
class UTextBlock;
class UW_DeckBuilderMenu;
struct FDeckBuilderLook;

/**
 * @brief One row of the deck builder's rail: a section with its picture, name and how many of its slots are filled.
 * Created by the deck builder menu, which it tells when it is clicked. Builds its own widget tree.
 */
UCLASS()
class RTS_SURVIVAL_API UW_DeckSectionEntry : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetupEntry(UW_DeckBuilderMenu* Menu, const FDeckBuilderLook& Look, const EDeckSection Section);

	/**
	 * @brief Updates the count, the fill bar and the selected look.
	 * @param Look The menu's art.
	 * @param bIsAvailable False while the player's profile has no slots for this section.
	 * @param FilledSlots Slots holding a card.
	 * @param SlotCount All slots of the section.
	 * @param bIsSelected Whether this is the section the grid shows.
	 */
	void RefreshEntry(const FDeckBuilderLook& Look, const bool bIsAvailable, const int32 FilledSlots,
	                  const int32 SlotCount, const bool bIsSelected);

	EDeckSection GetSection() const { return M_Section; }

protected:
	virtual void NativeOnInitialized() override;

private:
	void BuildWidgetTree();

	UFUNCTION()
	void OnEntryClicked();

	bool GetIsValidMenu() const;

	UPROPERTY()
	TWeakObjectPtr<UW_DeckBuilderMenu> M_Menu;

	EDeckSection M_Section = EDeckSection::Technologies;

	UPROPERTY()
	TObjectPtr<UButton> M_Button;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> M_Canvas;

	UPROPERTY()
	TObjectPtr<UImage> M_IconWell;

	UPROPERTY()
	TObjectPtr<UImage> M_Icon;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_NameText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_CountText;

	UPROPERTY()
	TObjectPtr<UImage> M_FillBarBack;

	UPROPERTY()
	TObjectPtr<UImage> M_FillBar;
};
