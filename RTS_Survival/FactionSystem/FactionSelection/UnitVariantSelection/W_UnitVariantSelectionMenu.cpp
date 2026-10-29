// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_UnitVariantSelectionMenu.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/RichTextBlock.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/AssetManager.h"
#include "Engine/DataTable.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "RTS_Survival/FactionSystem/FactionSelection/FactionPlayerController.h"
#include "RTS_Survival/Units/Variants/UnitVariantDataAsset.h"
#include "RTS_Survival/Units/Variants/UnitVariantPresentationLibrary.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "Slate/SlateBrushAsset.h"
#include "UnitVariantSelectionWidgetHelpers.h"
#include "UnitVariantProbes.h"
#include "W_UnitVariantCard.h"
#include "W_UnitVariantDetailPanel.h"
#include "W_UnitVariantDiscardDialog.h"
#include "W_UnitVariantUnitEntry.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSUnitVariantSelection, Log, All);

// Screen layout of UISkins/_generator/variant_selection_spec.md, section 4 (1920x1080 reference, whole units).
namespace UnitVariantSelectionLayout
{
	constexpr float ScreenMargin = 40.f;
	constexpr float EdgeShadeWidth = 640.f;
	constexpr float HeaderTop = 24.f;
	const FVector2D HeaderSize(560.f, 80.f);
	// Top padding 4 instead of the spec's 10: with Roboto the faction line otherwise sits on the underline at y 73.
	const FMargin HeaderPadding(40.f, 4.f, 40.f, 12.f);
	const FMargin FactionNamePadding(0.f, 2.f, 0.f, 0.f);
	constexpr float PanelTop = 120.f;
	const FVector2D UnitPanelSize(392.f, 848.f);
	const FVector2D VariantPanelSize(480.f, 848.f);
	constexpr float PanelHeaderHeight = 52.f;
	const FMargin PanelTitlePadding(32.f, 0.f, 0.f, 0.f);
	const FMargin PanelCountPadding(0.f, 0.f, 24.f, 0.f);
	const FMargin UnitListPadding(20.f, 64.f, 20.f, 20.f);
	const FMargin CardColumnPadding(20.f, 68.f, 20.f, 20.f);
	constexpr float EntrySpacing = 6.f;
	constexpr float CardSpacing = 12.f;
	const FVector2D BackButtonSize(240.f, 48.f);
	const FVector2D ConfirmButtonSize(280.f, 48.f);
	constexpr int32 TitleFontSize = 28;
	constexpr int32 TitleLetterSpacing = 150;
	constexpr int32 FactionFontSize = 15;
	constexpr int32 FactionLetterSpacing = 100;
	constexpr int32 PanelTitleFontSize = 18;
	constexpr int32 PanelTitleLetterSpacing = 100;
	constexpr int32 PanelCountFontSize = 13;
	constexpr int32 FootnoteFontSize = 13;
	// Size of a selection icon texture on a card; the card's well draws it at 128x128.
	const FVector2D CardIconSize(128.f, 128.f);
	// Detail panel: left of the variant panel, top aligned with it.
	constexpr float DetailPanelGap = 16.f;
	// Reset button bottom centre between Back and Confirm.
	const FVector2D ResetButtonSize(280.f, 48.f);
}

void UW_UnitVariantSelectionMenu::NativeOnInitialized()
{
	// Keyboard focus stays on the screen itself (its buttons never take it), for the arrow key navigation.
	SetIsFocusable(true);
	Super::NativeOnInitialized();
	M_Probes = NewObject<UUnitVariantProbes>(this);
	BuildWidgetTree();
	ApplyStyle();
}

void UW_UnitVariantSelectionMenu::BuildWidgetTree()
{
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}
	UCanvasPanel* const RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = RootCanvas;
	BuildEdgeShades(RootCanvas);
	BuildHeader(RootCanvas);
	BuildUnitPanel(RootCanvas);
	BuildCardPanel(RootCanvas);
	BuildDetailPanel(RootCanvas);
	BuildButtons(RootCanvas);
	BuildResetButton(RootCanvas);
	BuildDiscardDialog(RootCanvas);
}

void UW_UnitVariantSelectionMenu::BuildEdgeShades(UCanvasPanel* RootCanvas)
{
	using namespace UnitVariantSelectionLayout;
	M_EdgeShadeLeft = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	M_EdgeShadeLeft->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* const LeftSlot = RootCanvas->AddChildToCanvas(M_EdgeShadeLeft);
	LeftSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 1.f));
	LeftSlot->SetOffsets(FMargin(0.f, 0.f, EdgeShadeWidth, 0.f));

	M_EdgeShadeRight = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	M_EdgeShadeRight->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* const RightSlot = RootCanvas->AddChildToCanvas(M_EdgeShadeRight);
	RightSlot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 1.f));
	RightSlot->SetOffsets(FMargin(-EdgeShadeWidth, 0.f, EdgeShadeWidth, 0.f));
}

void UW_UnitVariantSelectionMenu::BuildHeader(UCanvasPanel* RootCanvas)
{
	using namespace UnitVariantSelectionLayout;
	M_HeaderPlate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	M_HeaderPlate->SetPadding(HeaderPadding);
	M_HeaderPlate->SetHorizontalAlignment(HAlign_Center);
	M_HeaderPlate->SetVerticalAlignment(VAlign_Top);
	UCanvasPanelSlot* const HeaderSlot = RootCanvas->AddChildToCanvas(M_HeaderPlate);
	HeaderSlot->SetAnchors(FAnchors(0.5f, 0.f));
	HeaderSlot->SetAlignment(FVector2D(0.5f, 0.f));
	HeaderSlot->SetPosition(FVector2D(0.f, HeaderTop));
	HeaderSlot->SetSize(HeaderSize);

	UVerticalBox* const HeaderColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	M_HeaderPlate->SetContent(HeaderColumn);
	M_TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_TitleText->SetText(NSLOCTEXT("UnitVariantSelection", "Title", "UNIT VARIANTS"));
	HeaderColumn->AddChildToVerticalBox(M_TitleText)->SetHorizontalAlignment(HAlign_Center);
	M_SubtitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	UVerticalBoxSlot* const SubtitleSlot = HeaderColumn->AddChildToVerticalBox(M_SubtitleText);
	SubtitleSlot->SetHorizontalAlignment(HAlign_Center);
	SubtitleSlot->SetPadding(FactionNamePadding);
}

UOverlay* UW_UnitVariantSelectionMenu::BuildSidePanel(UCanvasPanel* RootCanvas, const FVector2D& PanelSize,
                                                      UCanvasPanelSlot*& OutPanelSlot,
                                                      TObjectPtr<UTextBlock>& OutTitleText,
                                                      TObjectPtr<UTextBlock>& OutCountText,
                                                      TObjectPtr<UBorder>& OutPanelPlate)
{
	using namespace UnitVariantSelectionLayout;
	USizeBox* const PanelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	PanelBox->SetWidthOverride(PanelSize.X);
	PanelBox->SetHeightOverride(PanelSize.Y);
	OutPanelSlot = RootCanvas->AddChildToCanvas(PanelBox);
	OutPanelSlot->SetAutoSize(true);

	OutPanelPlate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	OutPanelPlate->SetPadding(FMargin(0.f));
	PanelBox->SetContent(OutPanelPlate);
	UOverlay* const PanelOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	OutPanelPlate->SetContent(PanelOverlay);

	USizeBox* const HeaderStrip = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	HeaderStrip->SetHeightOverride(PanelHeaderHeight);
	UOverlaySlot* const StripSlot = PanelOverlay->AddChildToOverlay(HeaderStrip);
	StripSlot->SetHorizontalAlignment(HAlign_Fill);
	StripSlot->SetVerticalAlignment(VAlign_Top);
	UHorizontalBox* const HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	HeaderStrip->SetContent(HeaderRow);

	OutTitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	UHorizontalBoxSlot* const TitleSlot = HeaderRow->AddChildToHorizontalBox(OutTitleText);
	TitleSlot->SetVerticalAlignment(VAlign_Center);
	TitleSlot->SetPadding(PanelTitlePadding);
	TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	OutCountText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	UHorizontalBoxSlot* const CountSlot = HeaderRow->AddChildToHorizontalBox(OutCountText);
	CountSlot->SetVerticalAlignment(VAlign_Center);
	CountSlot->SetPadding(PanelCountPadding);
	return PanelOverlay;
}

void UW_UnitVariantSelectionMenu::BuildUnitPanel(UCanvasPanel* RootCanvas)
{
	using namespace UnitVariantSelectionLayout;
	UCanvasPanelSlot* PanelSlot = nullptr;
	TObjectPtr<UTextBlock> PanelTitle;
	TObjectPtr<UTextBlock> PanelCount;
	UOverlay* const PanelOverlay = BuildSidePanel(RootCanvas, UnitPanelSize, PanelSlot, PanelTitle, PanelCount, M_UnitPanel);
	PanelSlot->SetAnchors(FAnchors(0.f, 0.f));
	PanelSlot->SetPosition(FVector2D(ScreenMargin, PanelTop));
	// The unit list needs no count; its title sits centred in the header strip.
	PanelCount->SetVisibility(ESlateVisibility::Collapsed);
	if (UHorizontalBoxSlot* const TitleSlot = Cast<UHorizontalBoxSlot>(PanelTitle->Slot))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetPadding(FMargin(0.f));
	}
	PanelTitle->SetText(NSLOCTEXT("UnitVariantSelection", "UnitsTitle", "UNITS"));
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		PanelTitle, UnitVariantSelectionWidgets::MakeFont(M_Style.HeadingFont, PanelTitleFontSize,
		                                                  PanelTitleLetterSpacing), M_Style.TextColor);

	M_UnitList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	UOverlaySlot* const ListSlot = PanelOverlay->AddChildToOverlay(M_UnitList);
	ListSlot->SetHorizontalAlignment(HAlign_Fill);
	ListSlot->SetVerticalAlignment(VAlign_Fill);
	ListSlot->SetPadding(UnitListPadding);
}

void UW_UnitVariantSelectionMenu::BuildCardPanel(UCanvasPanel* RootCanvas)
{
	using namespace UnitVariantSelectionLayout;
	UCanvasPanelSlot* PanelSlot = nullptr;
	UOverlay* const PanelOverlay = BuildSidePanel(RootCanvas, VariantPanelSize, PanelSlot, M_SelectedUnitText,
	                                              M_VariantCountText, M_CardPanel);
	PanelSlot->SetAnchors(FAnchors(1.f, 0.f));
	PanelSlot->SetAlignment(FVector2D(1.f, 0.f));
	PanelSlot->SetPosition(FVector2D(-ScreenMargin, PanelTop));

	UVerticalBox* const Body = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UOverlaySlot* const BodySlot = PanelOverlay->AddChildToOverlay(Body);
	BodySlot->SetHorizontalAlignment(HAlign_Fill);
	BodySlot->SetVerticalAlignment(VAlign_Fill);
	BodySlot->SetPadding(CardColumnPadding);
	// Takes the height the footnote leaves; its scroll bar only shows once the cards need more than that.
	M_CardScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	Body->AddChildToVerticalBox(M_CardScrollBox)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	M_CardColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	M_CardScrollBox->AddChild(M_CardColumn);
	M_FootnoteText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_FootnoteText->SetText(NSLOCTEXT("UnitVariantSelection", "Footnote",
	                                  "The equipped variant is trained from every barracks and factory of this faction."));
	M_FootnoteText->SetAutoWrapText(true);
	M_FootnoteText->SetJustification(ETextJustify::Center);
	Body->AddChildToVerticalBox(M_FootnoteText)->SetHorizontalAlignment(HAlign_Fill);
}

void UW_UnitVariantSelectionMenu::BuildButtons(UCanvasPanel* RootCanvas)
{
	using namespace UnitVariantSelectionLayout;
	UWidget* const BackBox = UnitVariantSelectionWidgets::BuildTextButton(
		*WidgetTree, M_Style, NSLOCTEXT("UnitVariantSelection", "Back", "BACK"), M_Style.BackButton, BackButtonSize,
		M_BackButton);
	M_BackButton->OnClicked.AddDynamic(this, &UW_UnitVariantSelectionMenu::OnBackClicked);
	UCanvasPanelSlot* const BackSlot = RootCanvas->AddChildToCanvas(BackBox);
	BackSlot->SetAnchors(FAnchors(0.f, 1.f));
	BackSlot->SetAlignment(FVector2D(0.f, 1.f));
	BackSlot->SetPosition(FVector2D(ScreenMargin, -ScreenMargin));
	BackSlot->SetAutoSize(true);

	UWidget* const ConfirmBox = UnitVariantSelectionWidgets::BuildTextButton(
		*WidgetTree, M_Style, NSLOCTEXT("UnitVariantSelection", "Confirm", "CONFIRM"), M_Style.ConfirmButton,
		ConfirmButtonSize, M_ConfirmButton);
	M_ConfirmButton->OnClicked.AddDynamic(this, &UW_UnitVariantSelectionMenu::OnConfirmClicked);
	UCanvasPanelSlot* const ConfirmSlot = RootCanvas->AddChildToCanvas(ConfirmBox);
	ConfirmSlot->SetAnchors(FAnchors(1.f, 1.f));
	ConfirmSlot->SetAlignment(FVector2D(1.f, 1.f));
	ConfirmSlot->SetPosition(FVector2D(-ScreenMargin, -ScreenMargin));
	ConfirmSlot->SetAutoSize(true);
}

void UW_UnitVariantSelectionMenu::BuildDetailPanel(UCanvasPanel* RootCanvas)
{
	using namespace UnitVariantSelectionLayout;
	M_DetailPanel = WidgetTree->ConstructWidget<UW_UnitVariantDetailPanel>(UW_UnitVariantDetailPanel::StaticClass());
	UCanvasPanelSlot* const PanelSlot = RootCanvas->AddChildToCanvas(M_DetailPanel);
	PanelSlot->SetAnchors(FAnchors(1.f, 0.f));
	PanelSlot->SetAlignment(FVector2D(1.f, 0.f));
	PanelSlot->SetPosition(FVector2D(-(ScreenMargin + VariantPanelSize.X + DetailPanelGap), PanelTop));
	PanelSlot->SetAutoSize(true);
}

void UW_UnitVariantSelectionMenu::ApplyStyle()
{
	using namespace UnitVariantSelectionLayout;
	if (not IsValid(M_HeaderPlate) || not IsValid(M_UnitPanel) || not IsValid(M_CardPanel))
	{
		return;
	}
	M_EdgeShadeLeft->SetBrush(M_Style.EdgeShade);
	FSlateBrush MirroredEdgeShade = M_Style.EdgeShade;
	MirroredEdgeShade.Mirroring = ESlateBrushMirrorType::Horizontal;
	M_EdgeShadeRight->SetBrush(MirroredEdgeShade);
	M_HeaderPlate->SetBrush(M_Style.HeaderPlate);
	M_UnitPanel->SetBrush(M_Style.UnitListPanel);
	M_CardPanel->SetBrush(M_Style.VariantPanel);

	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		M_TitleText, UnitVariantSelectionWidgets::MakeFont(M_Style.HeadingFont, TitleFontSize, TitleLetterSpacing),
		M_Style.TextColor);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		M_SubtitleText, UnitVariantSelectionWidgets::MakeFont(M_Style.HeadingFont, FactionFontSize, FactionLetterSpacing),
		M_Style.AccentColor);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		M_SelectedUnitText, UnitVariantSelectionWidgets::MakeFont(M_Style.HeadingFont, PanelTitleFontSize,
		                                                          PanelTitleLetterSpacing), M_Style.TextColor);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		M_VariantCountText, UnitVariantSelectionWidgets::MakeFont(M_Style.BodyFont, PanelCountFontSize,
		                                                          PanelTitleLetterSpacing), M_Style.MutedTextColor);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		M_FootnoteText, UnitVariantSelectionWidgets::MakeFont(M_Style.BodyFont, FootnoteFontSize), M_Style.MutedTextColor);
}

void UW_UnitVariantSelectionMenu::InitVariantSelection(AFactionPlayerController* FactionPlayerController,
                                                       const ERTSFaction Faction,
                                                       const TArray<FTrainingOption>& UnitsWithVariants,
                                                       const UUnitVariantDataAsset* VariantDataAsset,
                                                       const TMap<FTrainingOption, EUnitVariant>& CurrentSelections)
{
	M_FactionPlayerController = FactionPlayerController;
	M_VariantDataAsset = VariantDataAsset;
	M_TrainingOptionsTable = UUnitVariantPresentationLibrary::LoadTrainingOptionsTable();
	M_Units = UnitsWithVariants;
	M_Selections.Reset();
	for (const FTrainingOption& Unit : M_Units)
	{
		const EUnitVariant* SavedVariant = CurrentSelections.Find(Unit);
		M_Selections.Add(Unit, SavedVariant ? *SavedVariant : EUnitVariant::Base);
	}
	M_ConfirmedSelections = M_Selections;
	if (GetIsValidDiscardDialog())
	{
		M_DiscardDialog->SetupDialog(this, M_Style);
	}
	if (IsValid(M_SubtitleText))
	{
		M_SubtitleText->SetText(GetFactionDisplayName(Faction).ToUpper());
	}
	LoadAbilityTables();
	PopulateUnitEntries();
	SelectUnit(0);
}

void UW_UnitVariantSelectionMenu::PopulateUnitEntries()
{
	using namespace UnitVariantSelectionLayout;
	if (not GetIsValidUnitList() || not GetIsValidVariantDataAsset())
	{
		return;
	}
	M_UnitList->ClearChildren();
	M_UnitEntries.Reset();
	for (int32 UnitIndex = 0; UnitIndex < M_Units.Num(); ++UnitIndex)
	{
		const FTrainingOption& Unit = M_Units[UnitIndex];
		UW_UnitVariantUnitEntry* const Entry = CreateWidget<UW_UnitVariantUnitEntry>(
			this, UW_UnitVariantUnitEntry::StaticClass());
		if (not IsValid(Entry))
		{
			continue;
		}
		Entry->SetupEntry(this, UnitIndex, M_Style, MakeUnitIconBrush(Unit), GetUnitName(Unit),
		                  M_VariantDataAsset->GetImplementedVariantCount(Unit));
		Entry->SetPadding(FMargin(0.f, 0.f, 0.f, EntrySpacing));
		M_UnitList->AddChild(Entry);
		M_UnitEntries.Add(Entry);
		RefreshEntryChosenVariant(UnitIndex);
	}
}

void UW_UnitVariantSelectionMenu::HandleUnitEntryClicked(const int32 EntryIndex)
{
	SelectUnit(EntryIndex);
}

void UW_UnitVariantSelectionMenu::HandleVariantCardClicked(const EUnitVariant Variant)
{
	if (not M_Units.IsValidIndex(M_SelectedUnitIndex))
	{
		return;
	}
	M_Selections.Add(M_Units[M_SelectedUnitIndex], Variant);
	RefreshCardSelection();
	RefreshAbilityPanel();
	RefreshEntryChosenVariant(M_SelectedUnitIndex);
	RequestPreviewOfSelectedUnit();
}

void UW_UnitVariantSelectionMenu::SelectUnit(const int32 EntryIndex)
{
	if (not M_Units.IsValidIndex(EntryIndex))
	{
		return;
	}
	M_SelectedUnitIndex = EntryIndex;
	for (int32 Index = 0; Index < M_UnitEntries.Num(); ++Index)
	{
		if (IsValid(M_UnitEntries[Index]))
		{
			M_UnitEntries[Index]->SetIsSelected(Index == EntryIndex);
		}
	}
	const FTrainingOption& Unit = M_Units[EntryIndex];
	if (IsValid(M_SelectedUnitText))
	{
		M_SelectedUnitText->SetText(GetUnitName(Unit).ToUpper());
	}
	if (IsValid(M_VariantCountText) && GetIsValidVariantDataAsset())
	{
		M_VariantCountText->SetText(FText::Format(NSLOCTEXT("UnitVariantSelection", "PanelVariantCount", "{0} VARIANTS"),
		                                          FText::AsNumber(M_VariantDataAsset->GetImplementedVariantCount(Unit))));
	}
	RebuildCards(Unit);
	LoadVariantAbilities(Unit);
	RequestPreviewOfSelectedUnit();
}

void UW_UnitVariantSelectionMenu::RebuildCards(const FTrainingOption& Unit)
{
	using namespace UnitVariantSelectionLayout;
	if (not GetIsValidCardColumn() || not GetIsValidVariantDataAsset())
	{
		return;
	}
	M_CardColumn->ClearChildren();
	M_Cards.Reset();
	bM_HasHoveredVariant = false;
	M_KeyboardCardIndex = INDEX_NONE;
	const int32 VariantCount = M_VariantDataAsset->GetImplementedVariantCount(Unit);
	for (int32 VariantIndex = 0; VariantIndex < VariantCount; ++VariantIndex)
	{
		const EUnitVariant Variant = static_cast<EUnitVariant>(VariantIndex);
		UW_UnitVariantCard* const Card = CreateWidget<UW_UnitVariantCard>(this, UW_UnitVariantCard::StaticClass());
		if (not IsValid(Card))
		{
			continue;
		}
		Card->SetupCard(this, Variant, M_Style, MakeVariantIconBrush(Unit, Variant), GetVariantName(Unit, Variant),
		                GetVariantDescription(Unit, Variant));
		M_CardColumn->AddChildToVerticalBox(Card)->SetPadding(FMargin(0.f, 0.f, 0.f, CardSpacing));
		M_Cards.Add(Card);
	}
	if (GetIsValidCardScrollBox())
	{
		M_CardScrollBox->ScrollToStart();
	}
	RefreshCardSelection();
}

void UW_UnitVariantSelectionMenu::ScrollCardsToEnd() const
{
	if (not GetIsValidCardScrollBox())
	{
		return;
	}
	M_CardScrollBox->ScrollToEnd();
}

void UW_UnitVariantSelectionMenu::BuildResetButton(UCanvasPanel* RootCanvas)
{
	using namespace UnitVariantSelectionLayout;
	UWidget* const ResetBox = UnitVariantSelectionWidgets::BuildTextButton(
		*WidgetTree, M_Style, NSLOCTEXT("UnitVariantSelection", "ResetAll", "RESET TO BASE"), M_Style.BackButton,
		ResetButtonSize, M_ResetButton);
	M_ResetButton->OnClicked.AddDynamic(this, &UW_UnitVariantSelectionMenu::OnResetClicked);
	UCanvasPanelSlot* const ResetSlot = RootCanvas->AddChildToCanvas(ResetBox);
	ResetSlot->SetAnchors(FAnchors(0.5f, 1.f));
	ResetSlot->SetAlignment(FVector2D(0.5f, 1.f));
	ResetSlot->SetPosition(FVector2D(0.f, -ScreenMargin));
	ResetSlot->SetAutoSize(true);
}

void UW_UnitVariantSelectionMenu::BuildDiscardDialog(UCanvasPanel* RootCanvas)
{
	M_DiscardDialog = WidgetTree->ConstructWidget<UW_UnitVariantDiscardDialog>(
		UW_UnitVariantDiscardDialog::StaticClass());
	// Last on the canvas, so it draws over everything and takes every click while open.
	UCanvasPanelSlot* const DialogSlot = RootCanvas->AddChildToCanvas(M_DiscardDialog);
	DialogSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	DialogSlot->SetOffsets(FMargin(0.f));
}

FReply UW_UnitVariantSelectionMenu::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (HandleNavigationKey(InKeyEvent.GetKey()))
	{
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

bool UW_UnitVariantSelectionMenu::HandleNavigationKey(const FKey& Key)
{
	if (GetIsValidDiscardDialog() && M_DiscardDialog->GetIsOpen())
	{
		return HandleDiscardDialogKey(Key);
	}
	const auto IsAnyOf = [&Key](const std::initializer_list<FKey> Keys)
	{
		for (const FKey& Candidate : Keys)
		{
			if (Candidate == Key)
			{
				return true;
			}
		}
		return false;
	};
	if (IsAnyOf({EKeys::Up, EKeys::W, EKeys::Gamepad_DPad_Up, EKeys::Gamepad_LeftStick_Up}))
	{
		SelectAdjacentUnit(-1);
		return true;
	}
	if (IsAnyOf({EKeys::Down, EKeys::S, EKeys::Gamepad_DPad_Down, EKeys::Gamepad_LeftStick_Down}))
	{
		SelectAdjacentUnit(1);
		return true;
	}
	if (IsAnyOf({EKeys::Left, EKeys::A, EKeys::Gamepad_DPad_Left, EKeys::Gamepad_LeftShoulder}))
	{
		FocusAdjacentCard(-1);
		return true;
	}
	if (IsAnyOf({EKeys::Right, EKeys::D, EKeys::Gamepad_DPad_Right, EKeys::Gamepad_RightShoulder}))
	{
		FocusAdjacentCard(1);
		return true;
	}
	if (IsAnyOf({EKeys::Enter, EKeys::Gamepad_FaceButton_Bottom}))
	{
		EquipKeyboardFocusedCard();
		return true;
	}
	if (IsAnyOf({EKeys::SpaceBar, EKeys::Gamepad_Special_Right}))
	{
		OnConfirmClicked();
		return true;
	}
	if (IsAnyOf({EKeys::Escape, EKeys::Gamepad_FaceButton_Right}))
	{
		OnBackClicked();
		return true;
	}
	return false;
}

bool UW_UnitVariantSelectionMenu::HandleDiscardDialogKey(const FKey& Key)
{
	if (Key == EKeys::Enter || Key == EKeys::Gamepad_FaceButton_Bottom)
	{
		HandleDiscardDialogChoice(true);
	}
	else if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
	{
		HandleDiscardDialogChoice(false);
	}
	// Every other key is swallowed so nothing changes behind the open dialog.
	return true;
}

void UW_UnitVariantSelectionMenu::SelectAdjacentUnit(const int32 Step)
{
	const int32 NewIndex = FMath::Clamp(M_SelectedUnitIndex + Step, 0, M_Units.Num() - 1);
	if (NewIndex == M_SelectedUnitIndex || not M_Units.IsValidIndex(NewIndex))
	{
		return;
	}
	SelectUnit(NewIndex);
	if (GetIsValidUnitList() && M_UnitEntries.IsValidIndex(NewIndex))
	{
		M_UnitList->ScrollWidgetIntoView(M_UnitEntries[NewIndex]);
	}
}

void UW_UnitVariantSelectionMenu::FocusAdjacentCard(const int32 Step)
{
	if (M_Cards.IsEmpty() || not M_Units.IsValidIndex(M_SelectedUnitIndex))
	{
		return;
	}
	int32 StartIndex = M_KeyboardCardIndex;
	if (not M_Cards.IsValidIndex(StartIndex))
	{
		// The first press lands on the equipped card's neighbour, as if the focus started on the equipped card.
		const EUnitVariant ChosenVariant = GetChosenVariant(M_Units[M_SelectedUnitIndex]);
		StartIndex = M_Cards.IndexOfByPredicate([ChosenVariant](const UW_UnitVariantCard* Card)
		{
			return IsValid(Card) && Card->GetVariant() == ChosenVariant;
		});
	}
	const int32 CardCount = M_Cards.Num();
	SetKeyboardFocusedCard(((FMath::Max(StartIndex, 0) + Step) % CardCount + CardCount) % CardCount);
}

void UW_UnitVariantSelectionMenu::SetKeyboardFocusedCard(const int32 CardIndex)
{
	if (M_Cards.IsValidIndex(M_KeyboardCardIndex) && IsValid(M_Cards[M_KeyboardCardIndex]))
	{
		M_Cards[M_KeyboardCardIndex]->SetIsKeyboardFocused(false);
		HandleVariantCardHovered(M_Cards[M_KeyboardCardIndex]->GetVariant(), false);
	}
	M_KeyboardCardIndex = CardIndex;
	if (not M_Cards.IsValidIndex(CardIndex) || not IsValid(M_Cards[CardIndex]))
	{
		return;
	}
	M_Cards[CardIndex]->SetIsKeyboardFocused(true);
	// The detail panel follows the keyboard like it follows the mouse.
	HandleVariantCardHovered(M_Cards[CardIndex]->GetVariant(), true);
	if (GetIsValidCardScrollBox())
	{
		M_CardScrollBox->ScrollWidgetIntoView(M_Cards[CardIndex]);
	}
}

void UW_UnitVariantSelectionMenu::EquipKeyboardFocusedCard()
{
	if (not M_Cards.IsValidIndex(M_KeyboardCardIndex) || not IsValid(M_Cards[M_KeyboardCardIndex]))
	{
		return;
	}
	HandleVariantCardClicked(M_Cards[M_KeyboardCardIndex]->GetVariant());
}

bool UW_UnitVariantSelectionMenu::GetHasUnconfirmedChange(const FTrainingOption& Unit) const
{
	// FindRef gives Base for a unit without a confirmed choice, matching how the screen treats it.
	return GetChosenVariant(Unit) != M_ConfirmedSelections.FindRef(Unit);
}

int32 UW_UnitVariantSelectionMenu::GetUnconfirmedChangeCount() const
{
	int32 ChangeCount = 0;
	for (const FTrainingOption& Unit : M_Units)
	{
		ChangeCount += GetHasUnconfirmedChange(Unit) ? 1 : 0;
	}
	return ChangeCount;
}

void UW_UnitVariantSelectionMenu::OnResetClicked()
{
	for (const FTrainingOption& Unit : M_Units)
	{
		M_Selections.Add(Unit, EUnitVariant::Base);
	}
	for (int32 UnitIndex = 0; UnitIndex < M_Units.Num(); ++UnitIndex)
	{
		RefreshEntryChosenVariant(UnitIndex);
	}
	RefreshCardSelection();
	RefreshAbilityPanel();
	RequestPreviewOfSelectedUnit();
}

void UW_UnitVariantSelectionMenu::HandleDiscardDialogChoice(const bool bDiscard)
{
	if (GetIsValidDiscardDialog())
	{
		M_DiscardDialog->HideDialog();
	}
	if (not bDiscard || not GetIsValidFactionPlayerController())
	{
		return;
	}
	M_FactionPlayerController->HandleUnitVariantSelectionBack();
}

void UW_UnitVariantSelectionMenu::HandleVariantCardHovered(const EUnitVariant Variant, const bool bIsHovered)
{
	if (bIsHovered)
	{
		M_HoveredVariant = Variant;
		bM_HasHoveredVariant = true;
	}
	else if (bM_HasHoveredVariant && M_HoveredVariant == Variant)
	{
		bM_HasHoveredVariant = false;
	}
	RefreshAbilityPanel();
}

void UW_UnitVariantSelectionMenu::LoadAbilityTables()
{
	M_AbilityCardTable = M_Style.AbilityCardTable.LoadSynchronous();
	M_AbilityRichTextStyle = M_Style.AbilityRichTextStyle.LoadSynchronous();
	M_WeaponCardTable = M_Style.WeaponCardTable.LoadSynchronous();
	(void)GetIsValidAbilityCardTable();
	if (not IsValid(M_AbilityRichTextStyle))
	{
		RTSFunctionLibrary::ReportError("The unit variant screen has no AbilityRichTextStyle in M_Style; ability "
			"descriptions show their style tags as plain text.\nAt function: UW_UnitVariantSelectionMenu::LoadAbilityTables");
	}
	if (GetIsValidDetailPanel())
	{
		M_DetailPanel->SetupPanel(M_Style, M_AbilityRichTextStyle, M_WeaponCardTable);
	}
}

void UW_UnitVariantSelectionMenu::LoadVariantAbilities(const FTrainingOption& Unit)
{
	M_VariantAbilityCards.Reset();
	if (M_AbilityClassLoadHandle.IsValid())
	{
		M_AbilityClassLoadHandle->CancelHandle();
		M_AbilityClassLoadHandle.Reset();
	}
	RefreshAbilityPanel();
	if (not GetIsValidFactionPlayerController() || not GetIsValidVariantDataAsset())
	{
		return;
	}
	TArray<FSoftObjectPath> ClassPaths;
	const int32 VariantCount = M_VariantDataAsset->GetImplementedVariantCount(Unit);
	for (int32 VariantIndex = 0; VariantIndex < VariantCount; ++VariantIndex)
	{
		const TSoftClassPtr<AActor> VariantClass = M_FactionPlayerController->GetUnitVariantClass(
			Unit, static_cast<EUnitVariant>(VariantIndex));
		if (not VariantClass.IsNull())
		{
			ClassPaths.AddUnique(VariantClass.ToSoftObjectPath());
		}
	}
	if (ClassPaths.IsEmpty())
	{
		ResolveVariantAbilities();
		return;
	}
	const int32 RequestedUnitIndex = M_SelectedUnitIndex;
	M_AbilityClassLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		ClassPaths, FStreamableDelegate::CreateWeakLambda(this, [this, RequestedUnitIndex]()
		{
			if (M_SelectedUnitIndex == RequestedUnitIndex)
			{
				ResolveVariantAbilities();
			}
		}));
}

void UW_UnitVariantSelectionMenu::ResolveVariantAbilities()
{
	if (not M_Units.IsValidIndex(M_SelectedUnitIndex) || not GetIsValidFactionPlayerController()
		|| not GetIsValidAbilityCardTable())
	{
		return;
	}
	const FTrainingOption& Unit = M_Units[M_SelectedUnitIndex];
	for (UW_UnitVariantCard* const Card : M_Cards)
	{
		if (not IsValid(Card))
		{
			continue;
		}
		const EUnitVariant Variant = Card->GetVariant();
		const UClass* const VariantClass = M_FactionPlayerController->GetUnitVariantClass(Unit, Variant).Get();
		const FUnitVariantAbilityCardList& AbilityCards = M_VariantAbilityCards.Add(
			Variant, UnitVariantAbilityCards::MakeAbilityCards(VariantClass, M_AbilityCardTable));
		Card->SetAbilityIcons(AbilityCards.Cards);
		UE_LOG(LogRTSUnitVariantSelection, Display, TEXT("RTS_VARIANT_ABILITIES unit=%s variant=%s class=%s abilities=%d"),
		       *GetUnitName(Unit).ToString(), *UEnum::GetValueAsString(Variant), *GetNameSafe(VariantClass),
		       AbilityCards.Cards.Num());
	}
	RefreshAbilityPanel();
	ProbeVariantsOfSelectedUnit();
}

void UW_UnitVariantSelectionMenu::NativeDestruct()
{
	if (IsValid(M_Probes))
	{
		M_Probes->CancelProbes();
	}
	Super::NativeDestruct();
}

void UW_UnitVariantSelectionMenu::ProbeVariantsOfSelectedUnit()
{
	UWorld* const World = GetWorld();
	if (not IsValid(M_Probes) || not IsValid(World) || not M_Units.IsValidIndex(M_SelectedUnitIndex)
		|| not GetIsValidFactionPlayerController())
	{
		return;
	}
	TArray<UClass*> VariantClasses;
	for (const UW_UnitVariantCard* const Card : M_Cards)
	{
		if (IsValid(Card))
		{
			VariantClasses.Add(M_FactionPlayerController->GetUnitVariantClass(
				M_Units[M_SelectedUnitIndex], Card->GetVariant()).Get());
		}
	}
	const int32 ProbedUnitIndex = M_SelectedUnitIndex;
	TWeakObjectPtr<UW_UnitVariantSelectionMenu> WeakThis(this);
	M_Probes->ProbeClasses(*World, VariantClasses, [WeakThis, ProbedUnitIndex]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->HandleVariantProbesRead(ProbedUnitIndex);
		}
	});
}

void UW_UnitVariantSelectionMenu::HandleVariantProbesRead(const int32 ProbedUnitIndex)
{
	if (ProbedUnitIndex != M_SelectedUnitIndex || not M_Units.IsValidIndex(ProbedUnitIndex)
		|| not GetIsValidFactionPlayerController())
	{
		return;
	}
	const FTrainingOption& Unit = M_Units[ProbedUnitIndex];
	for (const UW_UnitVariantCard* const Card : M_Cards)
	{
		const UClass* const VariantClass = IsValid(Card)
			                                   ? M_FactionPlayerController->GetUnitVariantClass(Unit, Card->GetVariant()).Get()
			                                   : nullptr;
		const FUnitVariantProbeResult* const Result = M_Probes->FindResult(VariantClass);
		if (Result == nullptr)
		{
			continue;
		}
		FString WeaponList;
		for (const FUnitVariantWeaponStats& Weapon : Result->Weapons)
		{
			WeaponList += FString::Printf(TEXT("%s x%d (dmg %.0f pen %.0f reload %.1f range %.0f mag %d); "),
			                              *Weapon.WeaponRowName.ToString(), Weapon.Count, Weapon.Damage, Weapon.ArmorPen,
			                              Weapon.ReloadSeconds, Weapon.RangeMeters, Weapon.MagCapacity);
		}
		UE_LOG(LogRTSUnitVariantSelection, Display, TEXT("RTS_VARIANT_PROBE unit=%s variant=%s health=%.0f speed=%.2f weapons=%s"),
		       *GetUnitName(Unit).ToString(), *UEnum::GetValueAsString(Card->GetVariant()), Result->MaxHealth,
		       Result->MaxSpeedMultiplier, *WeaponList);
	}
	RefreshAbilityPanel();
}

const FUnitVariantProbeResult* UW_UnitVariantSelectionMenu::FindVariantProbe(const FTrainingOption& Unit,
                                                                            const EUnitVariant Variant) const
{
	if (not IsValid(M_Probes) || not GetIsValidFactionPlayerController())
	{
		return nullptr;
	}
	return M_Probes->FindResult(M_FactionPlayerController->GetUnitVariantClass(Unit, Variant).Get());
}

void UW_UnitVariantSelectionMenu::RefreshAbilityPanel()
{
	if (not GetIsValidDetailPanel())
	{
		return;
	}
	// Only while a card is hovered (or focused with the keys): the rest of the time the hangar unit stays in view.
	if (not bM_HasHoveredVariant)
	{
		M_DetailPanel->HidePanel();
		return;
	}
	const EUnitVariant ShownVariant = M_HoveredVariant;
	const FUnitVariantAbilityCardList* const AbilityCards = M_VariantAbilityCards.Find(ShownVariant);
	if (AbilityCards == nullptr || not M_Units.IsValidIndex(M_SelectedUnitIndex))
	{
		M_DetailPanel->HidePanel();
		return;
	}
	const FTrainingOption& Unit = M_Units[M_SelectedUnitIndex];
	FUnitVariantDetailContent Content;
	Content.VariantName = GetVariantName(Unit, ShownVariant);
	Content.AbilityCards = &AbilityCards->Cards;
	Content.Probe = FindVariantProbe(Unit, ShownVariant);
	// Base is the reference the variants are compared with; showing Base itself tags nothing.
	if (ShownVariant != EUnitVariant::Base)
	{
		const FUnitVariantAbilityCardList* const BaseAbilityCards = M_VariantAbilityCards.Find(EUnitVariant::Base);
		Content.BaseAbilityCards = BaseAbilityCards != nullptr ? &BaseAbilityCards->Cards : nullptr;
		Content.BaseProbe = FindVariantProbe(Unit, EUnitVariant::Base);
	}
	M_DetailPanel->ShowVariant(Content);
}

void UW_UnitVariantSelectionMenu::RefreshCardSelection()
{
	if (not M_Units.IsValidIndex(M_SelectedUnitIndex))
	{
		return;
	}
	const EUnitVariant ChosenVariant = GetChosenVariant(M_Units[M_SelectedUnitIndex]);
	for (UW_UnitVariantCard* const Card : M_Cards)
	{
		if (IsValid(Card))
		{
			Card->SetIsSelected(Card->GetVariant() == ChosenVariant);
		}
	}
}

void UW_UnitVariantSelectionMenu::RefreshEntryChosenVariant(const int32 EntryIndex)
{
	if (not M_UnitEntries.IsValidIndex(EntryIndex) || not M_Units.IsValidIndex(EntryIndex)
		|| not IsValid(M_UnitEntries[EntryIndex]))
	{
		return;
	}
	const FTrainingOption& Unit = M_Units[EntryIndex];
	const EUnitVariant ChosenVariant = GetChosenVariant(Unit);
	// Base needs no reminder; otherwise the short label ("Variant A"), as full names do not fit next to the count.
	M_UnitEntries[EntryIndex]->SetChosenVariantName(ChosenVariant == EUnitVariant::Base
		                                                ? FText::GetEmpty()
		                                                : UEnum::GetDisplayValueAsText(ChosenVariant));
	M_UnitEntries[EntryIndex]->SetHasUnconfirmedChange(GetHasUnconfirmedChange(Unit));
}

void UW_UnitVariantSelectionMenu::RequestPreviewOfSelectedUnit() const
{
	if (not M_Units.IsValidIndex(M_SelectedUnitIndex) || not GetIsValidFactionPlayerController())
	{
		return;
	}
	const FTrainingOption& Unit = M_Units[M_SelectedUnitIndex];
	M_FactionPlayerController->HandleUnitVariantPreviewRequested(Unit, GetChosenVariant(Unit));
}

FText UW_UnitVariantSelectionMenu::GetFactionDisplayName(const ERTSFaction Faction)
{
	// The enum display names carry the nation ("Ger Breakthrough Doctrine"); the header shows the doctrine only.
	FString FactionName = UEnum::GetDisplayValueAsText(Faction).ToString();
	FactionName.RemoveFromStart(TEXT("Ger "));
	return FText::FromString(FactionName);
}

EUnitVariant UW_UnitVariantSelectionMenu::GetChosenVariant(const FTrainingOption& Unit) const
{
	const EUnitVariant* ChosenVariant = M_Selections.Find(Unit);
	return ChosenVariant ? *ChosenVariant : EUnitVariant::Base;
}

FText UW_UnitVariantSelectionMenu::GetUnitName(const FTrainingOption& Unit) const
{
	const FText TableTitle = UUnitVariantPresentationLibrary::FindTrainingTableTitle(M_TrainingOptionsTable, Unit);
	if (not TableTitle.IsEmpty())
	{
		return TableTitle;
	}
	return UUnitVariantPresentationLibrary::GetUnitDisplayNameFallback(Unit);
}

FSlateBrush UW_UnitVariantSelectionMenu::MakeUnitIconBrush(const FTrainingOption& Unit)
{
	// The training icon when the unit has one; units without a DT_TrainingOptions row show their Base picture.
	const USlateBrushAsset* TableBrush = UUnitVariantPresentationLibrary::FindTrainingTableImageBrush(
		M_TrainingOptionsTable, Unit);
	if (IsValid(TableBrush))
	{
		return TableBrush->Brush;
	}
	return MakeVariantIconBrush(Unit, EUnitVariant::Base);
}

FText UW_UnitVariantSelectionMenu::GetVariantName(const FTrainingOption& Unit, const EUnitVariant Variant) const
{
	const FUnitVariantDefinition* Definition = IsValid(M_VariantDataAsset)
		                                           ? M_VariantDataAsset->FindVariantDefinition(Unit, Variant)
		                                           : nullptr;
	if (Definition && not Definition->DisplayName.IsEmpty())
	{
		return Definition->DisplayName;
	}
	if (Variant == EUnitVariant::Base)
	{
		return GetUnitName(Unit);
	}
	return UEnum::GetDisplayValueAsText(Variant);
}

FText UW_UnitVariantSelectionMenu::GetVariantDescription(const FTrainingOption& Unit, const EUnitVariant Variant) const
{
	const FUnitVariantDefinition* Definition = IsValid(M_VariantDataAsset)
		                                           ? M_VariantDataAsset->FindVariantDefinition(Unit, Variant)
		                                           : nullptr;
	return Definition ? Definition->Description : FText::GetEmpty();
}

FSlateBrush UW_UnitVariantSelectionMenu::MakeVariantIconBrush(const FTrainingOption& Unit, const EUnitVariant Variant)
{
	using namespace UnitVariantSelectionLayout;
	const FUnitVariantDefinition* Definition = IsValid(M_VariantDataAsset)
		                                           ? M_VariantDataAsset->FindVariantDefinition(Unit, Variant)
		                                           : nullptr;
	if (Definition && not Definition->Icon.IsNull())
	{
		UTexture2D* const IconTexture = Definition->Icon.LoadSynchronous();
		if (IsValid(IconTexture))
		{
			M_LoadedIcons.AddUnique(IconTexture);
			FSlateBrush IconBrush;
			IconBrush.SetResourceObject(IconTexture);
			IconBrush.ImageSize = CardIconSize;
			return IconBrush;
		}
	}
	if (Definition && Definition->bOverrideTrainingImageBrush && IsValid(Definition->TrainingImageBrush))
	{
		return Definition->TrainingImageBrush->Brush;
	}
	const USlateBrushAsset* TableBrush = UUnitVariantPresentationLibrary::FindTrainingTableImageBrush(
		M_TrainingOptionsTable, Unit);
	return IsValid(TableBrush) ? TableBrush->Brush : FSlateBrush();
}

void UW_UnitVariantSelectionMenu::OnConfirmClicked()
{
	if (not GetIsValidFactionPlayerController())
	{
		return;
	}
	M_FactionPlayerController->HandleUnitVariantSelectionConfirmed(M_Selections);
}

void UW_UnitVariantSelectionMenu::OnBackClicked()
{
	const int32 ChangeCount = GetUnconfirmedChangeCount();
	if (ChangeCount > 0 && GetIsValidDiscardDialog())
	{
		M_DiscardDialog->ShowDialog(ChangeCount);
		return;
	}
	HandleDiscardDialogChoice(true);
}

bool UW_UnitVariantSelectionMenu::GetIsValidFactionPlayerController() const
{
	if (M_FactionPlayerController.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_FactionPlayerController",
	                                                             "GetIsValidFactionPlayerController", this);
	return false;
}

bool UW_UnitVariantSelectionMenu::GetIsValidVariantDataAsset() const
{
	if (IsValid(M_VariantDataAsset))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_VariantDataAsset",
	                                                             "GetIsValidVariantDataAsset", this);
	return false;
}

bool UW_UnitVariantSelectionMenu::GetIsValidUnitList() const
{
	if (IsValid(M_UnitList))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_UnitList", "GetIsValidUnitList", this);
	return false;
}

bool UW_UnitVariantSelectionMenu::GetIsValidCardColumn() const
{
	if (IsValid(M_CardColumn))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_CardColumn", "GetIsValidCardColumn", this);
	return false;
}

bool UW_UnitVariantSelectionMenu::GetIsValidCardScrollBox() const
{
	if (IsValid(M_CardScrollBox))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_CardScrollBox", "GetIsValidCardScrollBox",
	                                                             this);
	return false;
}

bool UW_UnitVariantSelectionMenu::GetIsValidDetailPanel() const
{
	if (IsValid(M_DetailPanel))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_DetailPanel", "GetIsValidDetailPanel", this);
	return false;
}

bool UW_UnitVariantSelectionMenu::GetIsValidAbilityCardTable() const
{
	if (IsValid(M_AbilityCardTable))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_AbilityCardTable",
	                                                             "GetIsValidAbilityCardTable", this);
	return false;
}

#if !UE_BUILD_SHIPPING || RTS_WITH_MAP_TESTS
bool UW_UnitVariantSelectionMenu::Debug_GetIsDiscardDialogOpen() const
{
	return IsValid(M_DiscardDialog) && M_DiscardDialog->GetIsOpen();
}

EUnitVariant UW_UnitVariantSelectionMenu::Debug_GetChosenVariantOfSelectedUnit() const
{
	return M_Units.IsValidIndex(M_SelectedUnitIndex) ? GetChosenVariant(M_Units[M_SelectedUnitIndex]) : EUnitVariant::Base;
}
#endif

bool UW_UnitVariantSelectionMenu::GetIsValidDiscardDialog() const
{
	if (IsValid(M_DiscardDialog))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_DiscardDialog", "GetIsValidDiscardDialog",
	                                                             this);
	return false;
}
