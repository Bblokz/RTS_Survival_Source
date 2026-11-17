// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_DeckSectionEntry.h"

#include "DeckBuilderWidgetHelpers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "W_DeckBuilderMenu.h"

// Layout of a rail row (226 x 80 units); the entry art has a fixed height of 80.
namespace DeckSectionEntryLayout
{
	const FVector2D EntrySize(226.f, 80.f);
	const FVector2D IconWellPosition(16.f, 14.f);
	const FVector2D IconWellSize(52.f, 52.f);
	// The building portrait fills the well up to its rim.
	const FVector2D IconPosition(19.f, 17.f);
	const FVector2D IconSize(46.f, 46.f);
	const FVector2D NamePosition(80.f, 13.f);
	const FVector2D NameSize(126.f, 22.f);
	const FVector2D CountPosition(80.f, 37.f);
	const FVector2D CountSize(126.f, 16.f);
	const FVector2D FillBarPosition(80.f, 59.f);
	const FVector2D FillBarSize(110.f, 4.f);
	constexpr int32 NameFontSize = 12;
	constexpr int32 CountFontSize = 9;
	const FLinearColor FillBarBackColor(0.13f, 0.16f, 0.17f, 1.f);
	// A section the player has no slots for yet is drawn dimmed.
	constexpr float UnavailableOpacity = 0.45f;
}

void UW_DeckSectionEntry::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UW_DeckSectionEntry::BuildWidgetTree()
{
	using namespace DeckSectionEntryLayout;
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}
	USizeBox* const RootBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	RootBox->SetWidthOverride(EntrySize.X);
	RootBox->SetHeightOverride(EntrySize.Y);
	WidgetTree->RootWidget = RootBox;
	M_Button = WidgetTree->ConstructWidget<UUnitVariantSelectionButton>(UUnitVariantSelectionButton::StaticClass());
	M_Button->OnClicked.AddDynamic(this, &UW_DeckSectionEntry::OnEntryClicked);
	RootBox->SetContent(M_Button);
	M_Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	M_Canvas->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UButtonSlot* const CanvasSlot = Cast<UButtonSlot>(M_Button->SetContent(M_Canvas)))
	{
		CanvasSlot->SetPadding(FMargin(0.f));
		CanvasSlot->SetHorizontalAlignment(HAlign_Fill);
		CanvasSlot->SetVerticalAlignment(VAlign_Fill);
	}
	M_IconWell = DeckBuilderWidgets::MakeImage(*WidgetTree, FSlateBrush());
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, M_IconWell, IconWellPosition, IconWellSize);
	M_Icon = DeckBuilderWidgets::MakeImage(*WidgetTree, FSlateBrush());
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, M_Icon, IconPosition, IconSize);
	M_NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, M_NameText, NamePosition, NameSize);
	M_CountText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, M_CountText, CountPosition, CountSize);
	M_FillBarBack = DeckBuilderWidgets::MakeImage(*WidgetTree, FSlateColorBrush(FillBarBackColor));
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, M_FillBarBack, FillBarPosition, FillBarSize);
	M_FillBar = DeckBuilderWidgets::MakeImage(*WidgetTree, FSlateBrush());
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, M_FillBar, FillBarPosition, FillBarSize);
}

void UW_DeckSectionEntry::SetupEntry(UW_DeckBuilderMenu* Menu, const FDeckBuilderLook& Look,
                                     const EDeckSection Section)
{
	using namespace DeckSectionEntryLayout;
	M_Menu = Menu;
	M_Section = Section;
	if (not IsValid(M_Button) || not IsValid(M_NameText))
	{
		return;
	}
	M_IconWell->SetBrush(Look.IconWell);
	M_Icon->SetBrush(DeckBuilderWidgets::MakePictureBrush(Look.GetSectionIcon(Section), IconSize));
	M_NameText->SetText(DeckBuilder::GetSectionDisplayName(Section));
	M_NameText->SetFont(DeckBuilderWidgets::MakeFont(Look.HeadingFont, NameFontSize));
	M_NameText->SetColorAndOpacity(Look.Style.TextColor);
	M_NameText->SetShadowOffset(DeckBuilderWidgets::TextShadowOffset);
	M_NameText->SetShadowColorAndOpacity(DeckBuilderWidgets::TextShadowColor);
	M_CountText->SetFont(DeckBuilderWidgets::MakeFont(Look.BodyFont, CountFontSize));
	M_CountText->SetShadowOffset(DeckBuilderWidgets::TextShadowOffset);
	M_CountText->SetShadowColorAndOpacity(DeckBuilderWidgets::TextShadowColor);
	M_FillBar->SetBrush(FSlateColorBrush(Look.Style.AccentColor.GetSpecifiedColor()));
}

void UW_DeckSectionEntry::RefreshEntry(const FDeckBuilderLook& Look, const bool bIsAvailable, const int32 FilledSlots,
                                       const int32 SlotCount, const bool bIsSelected)
{
	using namespace DeckSectionEntryLayout;
	if (not IsValid(M_Button) || not IsValid(M_CountText))
	{
		return;
	}
	M_Button->SetStyle(bIsSelected ? Look.RailEntrySelected : Look.RailEntry);
	M_Canvas->SetRenderOpacity(bIsAvailable ? 1.f : UnavailableOpacity);
	const bool bIsFull = bIsAvailable && FilledSlots >= SlotCount;
	const FText CountText = bIsAvailable
		                        ? FText::Format(NSLOCTEXT("DeckBuilder", "RailCount", "{0} / {1} slots"),
		                                        FText::AsNumber(FilledSlots), FText::AsNumber(SlotCount))
		                        : NSLOCTEXT("DeckBuilder", "RailNotAvailable", "Not available yet");
	M_CountText->SetText(CountText);
	M_CountText->SetColorAndOpacity(bIsFull ? Look.Style.GoodColor : Look.Style.MutedTextColor);
	const float FillShare = bIsAvailable && SlotCount > 0
		                        ? FMath::Clamp(static_cast<float>(FilledSlots) / SlotCount, 0.f, 1.f)
		                        : 0.f;
	if (UCanvasPanelSlot* const FillSlot = Cast<UCanvasPanelSlot>(M_FillBar->Slot))
	{
		FillSlot->SetSize(FVector2D(FillBarSize.X * FillShare, FillBarSize.Y));
	}
}

void UW_DeckSectionEntry::OnEntryClicked()
{
	if (not GetIsValidMenu())
	{
		return;
	}
	M_Menu->HandleSectionClicked(M_Section);
}

bool UW_DeckSectionEntry::GetIsValidMenu() const
{
	if (M_Menu.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, TEXT("M_Menu"), TEXT("GetIsValidMenu"), this);
	return false;
}
