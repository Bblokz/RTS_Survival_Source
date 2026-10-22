// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/FactionSystem/Factions/Factions.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Units/Variants/UnitVariantTypes.h"
#include "UnitVariantSelectionStyle.h"

#include "W_UnitVariantSelectionMenu.generated.h"

class AFactionPlayerController;
class UBorder;
class UButton;
class UCanvasPanel;
class UCanvasPanelSlot;
class UDataTable;
class UImage;
class UOverlay;
class UScrollBox;
class UVerticalBox;
class UWidget;
class UTextBlock;
class UTexture2D;
class UUnitVariantDataAsset;
class UW_UnitVariantCard;
class UW_UnitVariantUnitEntry;

/**
 * @brief Shown by AFactionPlayerController after the faction is chosen when the game instance allows variant
 * selection: lists the faction's units that have variants, previews the chosen variant in the hangar and hands the
 * choices back on Confirm. Builds its own widget tree; a Blueprint child only fills M_Style with the skin brushes.
 */
UCLASS()
class RTS_SURVIVAL_API UW_UnitVariantSelectionMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * @brief Fills the screen and selects the first unit (which also previews it).
	 * @param FactionPlayerController Receives preview requests, Confirm and Back.
	 * @param Faction Shown in the header.
	 * @param UnitsWithVariants The faction's units with more than one implemented variant, in list order.
	 * @param VariantDataAsset Names, descriptions and icons of the variants; kept alive by this menu.
	 * @param CurrentSelections The saved choice per unit; a missing unit is Base.
	 */
	void InitVariantSelection(AFactionPlayerController* FactionPlayerController, const ERTSFaction Faction,
	                          const TArray<FTrainingOption>& UnitsWithVariants,
	                          const UUnitVariantDataAsset* VariantDataAsset,
	                          const TMap<FTrainingOption, EUnitVariant>& CurrentSelections);

	void HandleUnitEntryClicked(const int32 EntryIndex);
	void HandleVariantCardClicked(const EUnitVariant Variant);

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(EditDefaultsOnly, Category = "Unit Variant Selection")
	FUnitVariantSelectionStyle M_Style;

private:
	UPROPERTY()
	TWeakObjectPtr<AFactionPlayerController> M_FactionPlayerController;

	// Keeps the variant icons and training brushes the cards and entries show loaded.
	UPROPERTY()
	TObjectPtr<const UUnitVariantDataAsset> M_VariantDataAsset;

	UPROPERTY()
	TObjectPtr<const UDataTable> M_TrainingOptionsTable;

	// Selection screen icons loaded from the variant definitions' soft Icon textures.
	UPROPERTY()
	TArray<TObjectPtr<UTexture2D>> M_LoadedIcons;

	TArray<FTrainingOption> M_Units;
	// Every unit of M_Units has an entry, Base included, so Confirm writes the full choice for this faction.
	TMap<FTrainingOption, EUnitVariant> M_Selections;
	int32 M_SelectedUnitIndex = INDEX_NONE;

	UPROPERTY()
	TArray<TObjectPtr<UW_UnitVariantUnitEntry>> M_UnitEntries;

	UPROPERTY()
	TArray<TObjectPtr<UW_UnitVariantCard>> M_Cards;

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
	TObjectPtr<UBorder> M_UnitPanel;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_UnitCountText;

	UPROPERTY()
	TObjectPtr<UScrollBox> M_UnitList;

	UPROPERTY()
	TObjectPtr<UBorder> M_CardPanel;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_SelectedUnitText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_VariantCountText;

	UPROPERTY()
	TObjectPtr<UVerticalBox> M_CardColumn;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_FootnoteText;

	UPROPERTY()
	TObjectPtr<UButton> M_ConfirmButton;

	UPROPERTY()
	TObjectPtr<UButton> M_BackButton;

	void BuildWidgetTree();
	void BuildEdgeShades(UCanvasPanel* RootCanvas);
	void BuildHeader(UCanvasPanel* RootCanvas);
	void BuildUnitPanel(UCanvasPanel* RootCanvas);
	void BuildCardPanel(UCanvasPanel* RootCanvas);
	void BuildButtons(UCanvasPanel* RootCanvas);

	/**
	 * @brief A side column: fixed size panel plate with a title and count in its header strip.
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

	/**
	 * @param Label The button text.
	 * @param ButtonStyle The Confirm or Back style from M_Style.
	 * @param ButtonSize Drawn size its instances were made for.
	 * @param OutButton Receives the button, to bind its click.
	 * @return The size box to place on the canvas.
	 */
	UWidget* BuildTextButton(const FText& Label, const FButtonStyle& ButtonStyle, const FVector2D& ButtonSize,
	                         TObjectPtr<UButton>& OutButton);
	void ApplyStyle();

	void PopulateUnitEntries();
	void SelectUnit(const int32 EntryIndex);
	void RebuildCards(const FTrainingOption& Unit);
	void RefreshCardSelection();
	void RefreshEntryChosenVariant(const int32 EntryIndex);
	void RequestPreviewOfSelectedUnit() const;

	EUnitVariant GetChosenVariant(const FTrainingOption& Unit) const;
	static FText GetFactionDisplayName(const ERTSFaction Faction);
	FText GetUnitName(const FTrainingOption& Unit) const;
	FText GetVariantName(const FTrainingOption& Unit, const EUnitVariant Variant) const;
	FText GetVariantDescription(const FTrainingOption& Unit, const EUnitVariant Variant) const;

	/**
	 * @brief The picture a card shows: the selection icon from the variant data asset, else the variant's training
	 * brush, else the unit's DT_TrainingOptions brush. Every source stays loaded by this menu's UPROPERTYs.
	 * @param Unit The unit of the card.
	 * @param Variant The variant of the card.
	 * @return A brush with no resource when nothing has a picture.
	 */
	FSlateBrush MakeVariantIconBrush(const FTrainingOption& Unit, const EUnitVariant Variant);

	UFUNCTION()
	void OnConfirmClicked();

	UFUNCTION()
	void OnBackClicked();

	bool GetIsValidFactionPlayerController() const;
	bool GetIsValidVariantDataAsset() const;
	bool GetIsValidUnitList() const;
	bool GetIsValidCardColumn() const;
};
