// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_DeckVariantRow.h"

#include "DeckBuilderWidgetHelpers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "W_DeckBuilderMenu.h"

// Layout of a dock row (254 x 90 units).
namespace DeckVariantRowLayout
{
	const FVector2D RowSize(254.f, 90.f);
	const FVector2D PicturePosition(12.f, 12.f);
	const FVector2D PictureSize(54.f, 54.f);
	const FVector2D LabelPosition(76.f, 11.f);
	const FVector2D LabelSize(100.f, 12.f);
	const FVector2D NamePosition(76.f, 24.f);
	const FVector2D NameSize(166.f, 20.f);
	const FVector2D DescriptionPosition(76.f, 46.f);
	const FVector2D DescriptionSize(166.f, 36.f);
	const FVector2D EquippedTextPosition(150.f, 11.f);
	const FVector2D EquippedTextSize(68.f, 12.f);
	const FVector2D MarkerPosition(222.f, 6.f);
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
