// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/FactionSystem/Factions/Factions.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Units/Variants/UnitVariantTypes.h"
#include "UnitVariantAbilityCards.h"
#include "UnitVariantSelectionStyle.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"
#include "W_UnitVariantSelectionMenu.generated.h"

class AFactionPlayerController;
class UBorder;
class UButton;
class UCanvasPanel;
class UCanvasPanelSlot;
class UDataTable;
class UImage;
class UOverlay;
class URichTextBlock;
class UScrollBox;
class UVerticalBox;
class UWidget;
class UTextBlock;
class UTexture2D;
class UUnitVariantDataAsset;
class UW_UnitVariantCard;
class UW_UnitVariantDetailPanel;
class UW_UnitVariantDiscardDialog;
class UUnitVariantProbes;
struct FUnitVariantProbeResult;
class UW_UnitVariantUnitEntry;
struct FStreamableHandle;

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

	/** The ability panel shows the hovered card's abilities, and the equipped variant's when no card is hovered. */
	void HandleVariantCardHovered(const EUnitVariant Variant, const bool bIsHovered);

	/** @param bDiscard True leaves the screen without saving; false closes the dialog and keeps the choices. */
	void HandleDiscardDialogChoice(const bool bDiscard);

	int32 GetUnitCount() const { return M_Units.Num(); }

#if !UE_BUILD_SHIPPING || RTS_WITH_MAP_TESTS
	// Test hooks (offscreen captures): drive the keyboard navigation and read its effects without a real key press.
	bool Debug_PressNavigationKey(const FKey& Key) { return HandleNavigationKey(Key); }
	int32 Debug_GetUnconfirmedChangeCount() const { return GetUnconfirmedChangeCount(); }
	bool Debug_GetIsDiscardDialogOpen() const;
	EUnitVariant Debug_GetChosenVariantOfSelectedUnit() const;
#endif
	void ScrollCardsToEnd() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	// Up/Down pick the unit, Left/Right move over the cards, Enter equips, Space confirms, Escape goes back; the
	// gamepad d-pad, A, Start and B do the same.
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

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
	TObjectPtr<UScrollBox> M_UnitList;

	UPROPERTY()
	TObjectPtr<UBorder> M_CardPanel;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_SelectedUnitText;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_VariantCountText;

	UPROPERTY()
	TObjectPtr<UScrollBox> M_CardScrollBox;

	UPROPERTY()
	TObjectPtr<UVerticalBox> M_CardColumn;

	UPROPERTY()
	TObjectPtr<UTextBlock> M_FootnoteText;

	// Left of the variant panel: the special abilities of the hovered or equipped variant.
	UPROPERTY()
	TObjectPtr<UW_UnitVariantDetailPanel> M_DetailPanel;

	UPROPERTY()
	TObjectPtr<UW_UnitVariantDiscardDialog> M_DiscardDialog;

	UPROPERTY()
	TObjectPtr<UButton> M_ResetButton;

	// The choices as saved when the screen opened (Base for units without one); Back without Confirm returns to them.
	TMap<FTrainingOption, EUnitVariant> M_ConfirmedSelections;

	// Spawns each variant once out of sight to read the weapons and health its Blueprint sets up at BeginPlay.
	UPROPERTY()
	TObjectPtr<UUnitVariantProbes> M_Probes;

	// Index into M_Cards of the card the arrow keys are on; INDEX_NONE until a Left/Right press.
	int32 M_KeyboardCardIndex = INDEX_NONE;

	// M_Style.AbilityCardTable, loaded once; it also holds the ability icons the cards and the panel show.
	UPROPERTY()
	TObjectPtr<const UDataTable> M_AbilityCardTable;

	UPROPERTY()
	TObjectPtr<UDataTable> M_AbilityRichTextStyle;

	// M_Style.WeaponCardTable, loaded once; it holds the weapon icon brushes the detail panel shows.
	UPROPERTY()
	TObjectPtr<const UDataTable> M_WeaponCardTable;

	// Keeps the selected unit's variant classes loaded while their ability components are read; replaced per unit.
	TSharedPtr<FStreamableHandle> M_AbilityClassLoadHandle;

	// The selected unit's abilities per variant; empty until its variant classes have loaded.
	UPROPERTY()
	TMap<EUnitVariant, FUnitVariantAbilityCardList> M_VariantAbilityCards;

	EUnitVariant M_HoveredVariant = EUnitVariant::Base;
	bool bM_HasHoveredVariant = false;

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
	void BuildDetailPanel(UCanvasPanel* RootCanvas);
	void BuildResetButton(UCanvasPanel* RootCanvas);
	void BuildDiscardDialog(UCanvasPanel* RootCanvas);

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

	void ApplyStyle();

	void PopulateUnitEntries();
	void SelectUnit(const int32 EntryIndex);
	void RebuildCards(const FTrainingOption& Unit);
	void RefreshCardSelection();
	void RefreshEntryChosenVariant(const int32 EntryIndex);
	void RequestPreviewOfSelectedUnit() const;

	void LoadAbilityTables();
	// Loads the variant classes of the unit async; their component templates say which abilities each variant has.
	void LoadVariantAbilities(const FTrainingOption& Unit);
	void ResolveVariantAbilities();
	void ProbeVariantsOfSelectedUnit();
	void HandleVariantProbesRead(const int32 ProbedUnitIndex);
	// Null until the variant's probe was read.
	const FUnitVariantProbeResult* FindVariantProbe(const FTrainingOption& Unit, const EUnitVariant Variant) const;
	void RefreshAbilityPanel();
	EUnitVariant GetAbilityPanelVariant() const;

	bool GetHasUnconfirmedChange(const FTrainingOption& Unit) const;
	int32 GetUnconfirmedChangeCount() const;

	/** @return True when the key was one of the screen's navigation keys. */
	bool HandleNavigationKey(const FKey& Key);
	bool HandleDiscardDialogKey(const FKey& Key);
	void SelectAdjacentUnit(const int32 Step);
	void FocusAdjacentCard(const int32 Step);
	void SetKeyboardFocusedCard(const int32 CardIndex);
	void EquipKeyboardFocusedCard();

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
	FSlateBrush MakeUnitIconBrush(const FTrainingOption& Unit);

	UFUNCTION()
	void OnConfirmClicked();

	UFUNCTION()
	void OnBackClicked();

	UFUNCTION()
	void OnResetClicked();

	bool GetIsValidFactionPlayerController() const;
	bool GetIsValidVariantDataAsset() const;
	bool GetIsValidUnitList() const;
	bool GetIsValidCardColumn() const;
	bool GetIsValidCardScrollBox() const;
	bool GetIsValidDetailPanel() const;
	bool GetIsValidDiscardDialog() const;
	bool GetIsValidAbilityCardTable() const;
};
