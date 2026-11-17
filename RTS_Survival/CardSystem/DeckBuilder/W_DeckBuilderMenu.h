// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DeckBuilderState.h"
#include "DeckBuilderStyle.h"
#include "DeckCardDragOperation.h"

#include "W_DeckBuilderMenu.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UDeckCardCatalog;
class UImage;
class UScrollBox;
class USizeBox;
class UTextBlock;
class UTexture2D;
class UUniformGridPanel;
class UUnitVariantDataAsset;
class UWrapBox;
class UW_DeckCard;
class UW_DeckSectionEntry;
class UW_WorldMenu;

// The order the collection lists its cards in.
enum class EDeckCollectionSort : uint8
{
	AsOwned,
	ByRarity,
	ByType
};

/** What the dock shows: the card or variant the player pointed at last. */
struct FDeckDockSubject
{
	enum class EKind : uint8
	{
		Nothing,
		Card,
		Variant
	};

	EKind Kind = EKind::Nothing;

	ERTSCard Card = ERTSCard::Card_INVALID;

	FUnitVariantSelection Variant;
};

/**
 * @brief The card menu of the world campaign map, shown by UW_WorldMenu in place of the old Blueprint card menu:
 * sections on a rail, the selected section's slots as a grid, a dock for the card pointed at and a collection that
 * shows what fits the section. Unit cards carry their variant as a narrow card clipped behind them.
 * Builds its own widget tree and works without a Blueprint child; a child may override M_Style.
 */
UCLASS()
class RTS_SURVIVAL_API UW_DeckBuilderMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitDeckBuilder(UW_WorldMenu* WorldMenu);

	/** Shows the campaign's card data; called every time the menu is opened, so it never shows stale cards. */
	void SetupFromCardData(const FPlayerCardSaveData& CardData);

	void HandleSectionClicked(const EDeckSection Section);
	void HandleCardHovered(const ERTSCard Card);
	void HandleVariantHovered(const FUnitVariantSelection& Variant);

	/** A slotted card goes back to the collection; a collection card goes into a free slot of its section. */
	void HandleCardRightClicked(const UW_DeckCard& Card);

	/** A clipped variant comes off its unit; a loose one clips onto it. */
	void HandleVariantRightClicked(const FUnitVariantSelection& Variant, const bool bIsClipped);

	/** @return Whether dropping the dragged card on Target would change the deck. */
	bool GetIsDropAllowed(const UDeckCardDragOperation& Operation, const UW_DeckCard& Target) const;

	/** @return False when the drop changed nothing. */
	bool HandleDropOnCard(const UDeckCardDragOperation& Operation, const UW_DeckCard& Target);

	/** A dock row was clicked: Base detaches, a variant attaches. */
	void HandleDockVariantClicked(const FUnitVariantSelection& Variant);

	const FDeckBuilderLook& GetLook() const { return M_Look; }

	// Test hooks for the offscreen capture (UDeckBuilderCaptureSubsystem): they go through the same handlers a
	// right-click or a drop calls. In every configuration, because the capture subsystem is a UCLASS and so is too.
	const FDeckBuilderState& Debug_GetState() const { return M_State; }
	EDeckSection Debug_GetSelectedSection() const { return M_SelectedSection; }
	bool Debug_ReturnFirstSlottedCard();
	bool Debug_SlotFirstFittingCard();
	bool Debug_AttachFirstLooseVariant();
	void Debug_ShowWholeCollection(const bool bShowsWholeCollection);
	// What Save & Go Back would hand to the campaign save, without saving anything.
	FPlayerCardSaveData Debug_BuildCardData() const { return BuildCardDataToSave(); }
	FString Debug_DescribeState() const;
	// Presses Save & Go Back: writes the campaign save and returns to the map.
	void Debug_SaveAndGoBack() { OnSaveClicked(); }

protected:
	virtual void NativeOnInitialized() override;
	// The menu covers the map; a click on it never reaches the map.
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	// A card dropped beside every card: a slotted card goes back to the collection, a clipped variant comes off.
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
	                          UDragDropOperation* InOperation) override;

	UPROPERTY(EditDefaultsOnly, Category = "Deck Builder")
	FDeckBuilderStyle M_Style;

	// Nothing grants variants in the campaign yet. While this is on, the player owns every implemented variant of
	// every unit that has a card, so the menu can be used and tested. Never applies in Shipping builds, and what it
	// grants is not written to the save.
	UPROPERTY(EditDefaultsOnly, Category = "Deck Builder")
	bool bM_OwnEveryVariantUntilRewardsExist = true;

private:
	void BuildWidgetTree();
	UBorder* BuildPanel(const FVector2D& Position, const FVector2D& Size, const FMargin& PanelPadding);
	void BuildRail();
	void BuildSectionPanel();
	void BuildDock();
	void BuildCollection();

	void LoadCatalogIfNeeded();
	TArray<FUnitVariantSelection> GatherOwnedVariants(const FPlayerCardSaveData& CardData) const;

	void RefreshAll();
	void RefreshRail();
	void RefreshSectionPanel();
	void RefreshSectionPanel_Grid(const FDeckSectionState& Section);
	void RefreshCollection();
	void RefreshCollection_Cards(const TArray<ERTSCard>& Cards);
	void RefreshCollection_Variants(const TArray<FUnitVariantSelection>& Variants);
	void RefreshDock();
	void RefreshDock_Card(const FDeckCardDefinition& Definition);
	void RefreshDock_Variant(const FUnitVariantSelection& Variant);
	void RefreshDock_VariantRows(const FTrainingOption& Unit);
	void SetDockTexts(const FText& Title, const FText& Subtitle, const FText& Description);

	FDeckVariantCardView MakeVariantView(const FUnitVariantSelection& Variant);
	FDeckVariantCardView MakeBaseView(const FTrainingOption& Unit);
	FDeckCardVariantsView MakeCardVariantsView(const FDeckCardDefinition& Definition);
	FSlateBrush LoadVariantPicture(const FUnitVariantSelection& Variant);

	void SortCollectionCards(TArray<ERTSCard>& InOutCards) const;

	/** @return The deck as the campaign save stores it, normalised the way the old card menu's saves were. */
	FPlayerCardSaveData BuildCardDataToSave() const;
	bool GetOwnsEveryVariant() const;

	UFUNCTION()
	void OnSaveClicked();

	UFUNCTION()
	void OnFitsTabClicked();

	UFUNCTION()
	void OnEverythingTabClicked();

	UFUNCTION()
	void OnSortByRarityClicked();

	UFUNCTION()
	void OnSortByTypeClicked();

	bool GetIsValidWorldMenu() const;
	bool GetIsValidCatalog() const;
	bool GetIsValidVariantDataAsset() const;

	UPROPERTY()
	TWeakObjectPtr<UW_WorldMenu> M_WorldMenu;

	UPROPERTY()
	FDeckBuilderLook M_Look;

	UPROPERTY()
	TObjectPtr<UDeckCardCatalog> M_Catalog;

	UPROPERTY()
	TObjectPtr<const UUnitVariantDataAsset> M_VariantDataAsset;

	// Variant pictures loaded from the data asset's soft references; kept so the cards showing them stay drawn.
	UPROPERTY()
	TMap<FString, TObjectPtr<UTexture2D>> M_LoadedVariantPictures;

	FDeckBuilderState M_State;

	EDeckSection M_SelectedSection = EDeckSection::Barracks;

	FDeckDockSubject M_DockSubject;

	// False: the collection lists only what fits the selected section. True: everything the player owns.
	bool bM_ShowsWholeCollection = false;

	EDeckCollectionSort M_CollectionSort = EDeckCollectionSort::AsOwned;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> M_ScreenCanvas;

	UPROPERTY()
	TArray<TObjectPtr<UW_DeckSectionEntry>> M_SectionEntries;

	UPROPERTY()
	TObjectPtr<UButton> M_SaveButton;

	UPROPERTY()
	TObjectPtr<UImage> M_SectionIcon;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_SectionNameText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_SectionSubtitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_SectionCountText;

	UPROPERTY()
	TObjectPtr<UUniformGridPanel> M_SlotGrid;

	// Shown instead of the grid for a section the player's profile has no slots for.
	UPROPERTY()
	TObjectPtr<UTextBlock> M_SectionUnavailableText;

	UPROPERTY()
	TObjectPtr<USizeBox> M_DockPreviewHost;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_DockTitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_DockSubtitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_DockDescriptionText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_DockVariantsLabel;

	UPROPERTY()
	TObjectPtr<UWrapBox> M_DockVariantRows;

	UPROPERTY()
	TObjectPtr<UButton> M_FitsTabButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_FitsTabLabel;

	UPROPERTY()
	TObjectPtr<UButton> M_EverythingTabButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_EverythingTabLabel;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_CollectionCardsHeader;

	UPROPERTY()
	TObjectPtr<UWrapBox> M_CollectionCards;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_CollectionVariantsHeader;

	UPROPERTY()
	TObjectPtr<UWrapBox> M_CollectionVariants;

	UPROPERTY()
	TObjectPtr<UButton> M_SortByRarityButton;

	UPROPERTY()
	TObjectPtr<UButton> M_SortByTypeButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_CollectionHintText;
};
