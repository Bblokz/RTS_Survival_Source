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
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "RTS_Survival/FactionSystem/FactionSelection/FactionPlayerController.h"
#include "RTS_Survival/Units/Variants/UnitVariantDataAsset.h"
#include "RTS_Survival/Units/Variants/UnitVariantPresentationLibrary.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "Slate/SlateBrushAsset.h"
#include "UnitVariantSelectionWidgetHelpers.h"
#include "W_UnitVariantCard.h"
#include "W_UnitVariantUnitEntry.h"

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
	const FMargin ButtonTextPadding(36.f, 4.f, 36.f, 10.f);
	constexpr int32 TitleFontSize = 28;
	constexpr int32 TitleLetterSpacing = 150;
	constexpr int32 FactionFontSize = 15;
	constexpr int32 FactionLetterSpacing = 100;
	constexpr int32 PanelTitleFontSize = 18;
	constexpr int32 PanelTitleLetterSpacing = 100;
	constexpr int32 PanelCountFontSize = 13;
	constexpr int32 FootnoteFontSize = 13;
	constexpr int32 ButtonFontSize = 18;
	constexpr int32 ButtonLetterSpacing = 100;
	// Size of a selection icon texture on a card; the card's well draws it at 128x128.
	const FVector2D CardIconSize(128.f, 128.f);
}

void UW_UnitVariantSelectionMenu::NativeOnInitialized()
{
	Super::NativeOnInitialized();
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
	BuildButtons(RootCanvas);
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
	UOverlay* const PanelOverlay = BuildSidePanel(RootCanvas, UnitPanelSize, PanelSlot, PanelTitle, M_UnitCountText, M_UnitPanel);
	PanelSlot->SetAnchors(FAnchors(0.f, 0.f));
	PanelSlot->SetPosition(FVector2D(ScreenMargin, PanelTop));
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
	M_CardColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Body->AddChildToVerticalBox(M_CardColumn);
	USpacer* const Filler = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	Body->AddChildToVerticalBox(Filler)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
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
	UWidget* const BackBox = BuildTextButton(NSLOCTEXT("UnitVariantSelection", "Back", "BACK"), M_Style.BackButton,
	                                         BackButtonSize, M_BackButton);
	M_BackButton->OnClicked.AddDynamic(this, &UW_UnitVariantSelectionMenu::OnBackClicked);
	UCanvasPanelSlot* const BackSlot = RootCanvas->AddChildToCanvas(BackBox);
	BackSlot->SetAnchors(FAnchors(0.f, 1.f));
	BackSlot->SetAlignment(FVector2D(0.f, 1.f));
	BackSlot->SetPosition(FVector2D(ScreenMargin, -ScreenMargin));
	BackSlot->SetAutoSize(true);

	UWidget* const ConfirmBox = BuildTextButton(NSLOCTEXT("UnitVariantSelection", "Confirm", "CONFIRM"),
	                                            M_Style.ConfirmButton, ConfirmButtonSize, M_ConfirmButton);
	M_ConfirmButton->OnClicked.AddDynamic(this, &UW_UnitVariantSelectionMenu::OnConfirmClicked);
	UCanvasPanelSlot* const ConfirmSlot = RootCanvas->AddChildToCanvas(ConfirmBox);
	ConfirmSlot->SetAnchors(FAnchors(1.f, 1.f));
	ConfirmSlot->SetAlignment(FVector2D(1.f, 1.f));
	ConfirmSlot->SetPosition(FVector2D(-ScreenMargin, -ScreenMargin));
	ConfirmSlot->SetAutoSize(true);
}

UWidget* UW_UnitVariantSelectionMenu::BuildTextButton(const FText& Label, const FButtonStyle& ButtonStyle,
                                                      const FVector2D& ButtonSize, TObjectPtr<UButton>& OutButton)
{
	using namespace UnitVariantSelectionLayout;
	USizeBox* const ButtonBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ButtonBox->SetWidthOverride(ButtonSize.X);
	ButtonBox->SetHeightOverride(ButtonSize.Y);
	OutButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	OutButton->SetStyle(ButtonStyle);
	ButtonBox->SetContent(OutButton);
	UTextBlock* const LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LabelText->SetText(Label);
	LabelText->SetVisibility(ESlateVisibility::HitTestInvisible);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		LabelText, UnitVariantSelectionWidgets::MakeFont(M_Style.HeadingFont, ButtonFontSize, ButtonLetterSpacing),
		M_Style.TextColor);
	if (UButtonSlot* const LabelSlot = Cast<UButtonSlot>(OutButton->SetContent(LabelText)))
	{
		LabelSlot->SetPadding(ButtonTextPadding);
		LabelSlot->SetHorizontalAlignment(HAlign_Center);
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}
	return ButtonBox;
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
	for (UTextBlock* const CountText : {M_UnitCountText.Get(), M_VariantCountText.Get()})
	{
		UnitVariantSelectionWidgets::ApplyTextOnMetal(
			CountText, UnitVariantSelectionWidgets::MakeFont(M_Style.BodyFont, PanelCountFontSize, PanelTitleLetterSpacing),
			M_Style.MutedTextColor);
	}
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
	if (IsValid(M_SubtitleText))
	{
		M_SubtitleText->SetText(GetFactionDisplayName(Faction).ToUpper());
	}
	if (IsValid(M_UnitCountText))
	{
		M_UnitCountText->SetText(FText::Format(NSLOCTEXT("UnitVariantSelection", "UnitCount", "{0} WITH VARIANTS"),
		                                       FText::AsNumber(M_Units.Num())));
	}
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
		Entry->SetupEntry(this, UnitIndex, M_Style,
		                  UUnitVariantPresentationLibrary::FindTrainingTableImageBrush(M_TrainingOptionsTable, Unit),
		                  GetUnitName(Unit), M_VariantDataAsset->GetImplementedVariantCount(Unit));
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
	RefreshCardSelection();
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
	return FText::FromString(Unit.GetDisplayName());
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
	if (not GetIsValidFactionPlayerController())
	{
		return;
	}
	M_FactionPlayerController->HandleUnitVariantSelectionBack();
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
