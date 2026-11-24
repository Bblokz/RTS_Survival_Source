// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_DeckVariantRow.h"

#include "DeckBuilderWidgetHelpers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "W_DeckBuilderMenu.h"

// Layout of a dock row (378 x 94 units): two rows across the dock, three lines of description.
namespace DeckVariantRowLayout
{
	const FVector2D RowSize(378.f, 94.f);
	const FVector2D PicturePosition(12.f, 14.f);
	const FVector2D PictureSize(66.f, 66.f);
	const FVector2D LabelPosition(90.f, 10.f);
	const FVector2D LabelSize(120.f, 12.f);
	const FVector2D NamePosition(90.f, 23.f);
	const FVector2D NameSize(276.f, 20.f);
	const FVector2D DescriptionPosition(90.f, 45.f);
	const FVector2D DescriptionSize(276.f, 43.f);
	const FVector2D EquippedTextPosition(270.f, 10.f);
	const FVector2D EquippedTextSize(72.f, 12.f);
	const FVector2D MarkerPosition(346.f, 5.f);
	const FVector2D MarkerSize(22.f, 22.f);
	constexpr int32 LabelFontSize = 7;
	constexpr int32 LabelLetterSpacing = 150;
	constexpr int32 NameFontSize = 11;
	constexpr int32 DescriptionFontSize = 8;
}

void UW_DeckVariantRow::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UW_DeckVariantRow::BuildWidgetTree()
{
	using namespace DeckVariantRowLayout;
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}
	USizeBox* const RootBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	RootBox->SetWidthOverride(RowSize.X);
	RootBox->SetHeightOverride(RowSize.Y);
	WidgetTree->RootWidget = RootBox;
	M_Button = WidgetTree->ConstructWidget<UUnitVariantSelectionButton>(UUnitVariantSelectionButton::StaticClass());
	M_Button->OnClicked.AddDynamic(this, &UW_DeckVariantRow::OnRowClicked);
	RootBox->SetContent(M_Button);
	M_Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	M_Canvas->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UButtonSlot* const CanvasSlot = Cast<UButtonSlot>(M_Button->SetContent(M_Canvas)))
	{
		CanvasSlot->SetPadding(FMargin(0.f));
		CanvasSlot->SetHorizontalAlignment(HAlign_Fill);
		CanvasSlot->SetVerticalAlignment(VAlign_Fill);
	}
	M_Picture = DeckBuilderWidgets::MakeImage(*WidgetTree, FSlateBrush());
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, M_Picture, PicturePosition, PictureSize);
	M_LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, M_LabelText, LabelPosition, LabelSize);
	M_NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, M_NameText, NamePosition, NameSize);
	M_DescriptionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_DescriptionText->SetAutoWrapText(true);
	M_DescriptionText->SetClipping(EWidgetClipping::ClipToBounds);
	// A longer text ends in an ellipsis instead of a line cut in half.
	M_DescriptionText->SetTextOverflowPolicy(ETextOverflowPolicy::MultilineEllipsis);
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, M_DescriptionText, DescriptionPosition, DescriptionSize);
	M_EquippedText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_EquippedText->SetText(NSLOCTEXT("DeckBuilder", "RowEquipped", "EQUIPPED"));
	M_EquippedText->SetJustification(ETextJustify::Right);
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, M_EquippedText, EquippedTextPosition, EquippedTextSize);
	M_EquippedMarker = DeckBuilderWidgets::MakeImage(*WidgetTree, FSlateBrush());
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, M_EquippedMarker, MarkerPosition, MarkerSize);
}

void UW_DeckVariantRow::SetupRow(UW_DeckBuilderMenu* Menu, const FDeckBuilderLook& Look,
                                 const FDeckVariantCardView& View, const bool bIsEquipped)
{
	using namespace DeckVariantRowLayout;
	M_Menu = Menu;
	M_Variant = View.Variant;
	if (not IsValid(M_Button) || not IsValid(M_DescriptionText))
	{
		return;
	}
	const FDeckBuilderStyle& Style = Look.Style;
	M_Button->SetStyle(bIsEquipped ? Look.VariantRowSelected : Look.VariantRow);
	M_Picture->SetBrush(DeckBuilderWidgets::MakePictureBrush(View.Picture, PictureSize));
	M_Picture->SetVisibility(View.Picture.GetResourceObject() ? ESlateVisibility::HitTestInvisible
		                         : ESlateVisibility::Hidden);
	const auto StyleText = [](UTextBlock* TextBlock, const FText& Text, const FSlateFontInfo& Font,
	                          const FSlateColor& Color)
	{
		TextBlock->SetText(Text);
		TextBlock->SetFont(Font);
		TextBlock->SetColorAndOpacity(Color);
		TextBlock->SetShadowOffset(DeckBuilderWidgets::TextShadowOffset);
		TextBlock->SetShadowColorAndOpacity(DeckBuilderWidgets::TextShadowColor);
	};
	const FSlateFontInfo LabelFont = DeckBuilderWidgets::MakeFont(Look.BodyFont, LabelFontSize, LabelLetterSpacing);
	StyleText(M_LabelText, View.Label, LabelFont, Style.LabelColor);
	StyleText(M_NameText, View.Name.ToUpper(), DeckBuilderWidgets::MakeFont(Look.HeadingFont, NameFontSize),
	          Style.TextColor);
	StyleText(M_DescriptionText, View.Description, DeckBuilderWidgets::MakeFont(Look.BodyFont, DescriptionFontSize),
	          Style.DescriptionColor);
	StyleText(M_EquippedText, M_EquippedText->GetText(), LabelFont, Style.AccentColor);
	const ESlateVisibility EquippedVisibility = bIsEquipped ? ESlateVisibility::HitTestInvisible
		                                            : ESlateVisibility::Collapsed;
	M_EquippedText->SetVisibility(EquippedVisibility);
	M_EquippedMarker->SetBrush(Look.SelectedMarker);
	M_EquippedMarker->SetVisibility(EquippedVisibility);
}

void UW_DeckVariantRow::OnRowClicked()
{
	if (not GetIsValidMenu())
	{
		return;
	}
	M_Menu->HandleDockVariantClicked(M_Variant);
}

bool UW_DeckVariantRow::GetIsValidMenu() const
{
	if (M_Menu.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, TEXT("M_Menu"), TEXT("GetIsValidMenu"), this);
	return false;
}
