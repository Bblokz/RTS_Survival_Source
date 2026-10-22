// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_UnitVariantCard.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "UnitVariantSelectionWidgetHelpers.h"
#include "W_UnitVariantSelectionMenu.h"

// Measured on Custom_VariantCard (variant_selection_spec.md, section 2).
namespace UnitVariantCardLayout
{
	constexpr float CardWidth = 440.f;
	constexpr float CardHeight = 216.f;
	constexpr float IconWellSize = 136.f;
	constexpr float IconSize = 128.f;
	const FMargin IconWellOffset(16.f, 40.f, 0.f, 0.f);
	const FMargin IconWellPadding(4.f);
	// Text column x 168..424; the label centres on y 48, the name on y 74, the description starts at y 88.
	constexpr float TextColumnWidth = 256.f;
	const FMargin TextColumnOffset(168.f, 40.f, 0.f, 0.f);
	const FMargin NamePadding(0.f, 6.f, 0.f, 2.f);
	constexpr float MarkerSize = 28.f;
	const FMargin MarkerOffset(388.f, 34.f, 0.f, 0.f);
	// EQUIPPED ends at x 380 on the label's line.
	const FMargin EquippedOffset(0.f, 40.f, 60.f, 0.f);
	constexpr int32 LabelFontSize = 12;
	constexpr int32 LabelLetterSpacing = 100;
	constexpr int32 NameFontSize = 20;
	constexpr int32 DescriptionFontSize = 14;
	constexpr float DescriptionLineHeight = 1.15f;
}

void UW_UnitVariantCard::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UW_UnitVariantCard::BuildWidgetTree()
{
	using namespace UnitVariantCardLayout;
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}
	USizeBox* const SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SizeBox->SetWidthOverride(CardWidth);
	SizeBox->SetHeightOverride(CardHeight);
	WidgetTree->RootWidget = SizeBox;

	M_Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	M_Button->OnClicked.AddDynamic(this, &UW_UnitVariantCard::OnCardClicked);
	SizeBox->SetContent(M_Button);

	UOverlay* const CardOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	// The button centres its content by default; the overlay must fill it so the spec offsets hold.
	if (UButtonSlot* const ContentSlot = Cast<UButtonSlot>(M_Button->SetContent(CardOverlay)))
	{
		ContentSlot->SetPadding(FMargin(0.f));
		ContentSlot->SetHorizontalAlignment(HAlign_Fill);
		ContentSlot->SetVerticalAlignment(VAlign_Fill);
	}
	BuildIconWell(CardOverlay);
	BuildTextColumn(CardOverlay);
	BuildEquippedBadge(CardOverlay);
}

void UW_UnitVariantCard::BuildIconWell(UOverlay* CardOverlay)
{
	using namespace UnitVariantCardLayout;
	USizeBox* const WellBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	WellBox->SetWidthOverride(IconWellSize);
	WellBox->SetHeightOverride(IconWellSize);
	UOverlaySlot* const WellSlot = CardOverlay->AddChildToOverlay(WellBox);
	WellSlot->SetHorizontalAlignment(HAlign_Left);
	WellSlot->SetVerticalAlignment(VAlign_Top);
	WellSlot->SetPadding(IconWellOffset);

	M_IconWell = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	M_IconWell->SetPadding(IconWellPadding);
	M_IconWell->SetVisibility(ESlateVisibility::HitTestInvisible);
	WellBox->SetContent(M_IconWell);
	M_Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	M_IconWell->SetContent(M_Icon);
}

void UW_UnitVariantCard::BuildTextColumn(UOverlay* CardOverlay)
{
	using namespace UnitVariantCardLayout;
	USizeBox* const ColumnBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ColumnBox->SetWidthOverride(TextColumnWidth);
	ColumnBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlaySlot* const ColumnSlot = CardOverlay->AddChildToOverlay(ColumnBox);
	ColumnSlot->SetHorizontalAlignment(HAlign_Left);
	ColumnSlot->SetVerticalAlignment(VAlign_Top);
	ColumnSlot->SetPadding(TextColumnOffset);

	UVerticalBox* const Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	ColumnBox->SetContent(Column);
	M_LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Column->AddChildToVerticalBox(M_LabelText);
	M_NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	// Long names ("Panzer IV G (H hull)") wrap inside the column instead of running over the card's rim.
	M_NameText->SetAutoWrapText(true);
	Column->AddChildToVerticalBox(M_NameText)->SetPadding(NamePadding);
	M_DescriptionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_DescriptionText->SetAutoWrapText(true);
	M_DescriptionText->SetLineHeightPercentage(DescriptionLineHeight);
	Column->AddChildToVerticalBox(M_DescriptionText);
}

void UW_UnitVariantCard::BuildEquippedBadge(UOverlay* CardOverlay)
{
	using namespace UnitVariantCardLayout;
	M_EquippedText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_EquippedText->SetText(NSLOCTEXT("UnitVariantSelection", "Equipped", "EQUIPPED"));
	M_EquippedText->SetVisibility(ESlateVisibility::Collapsed);
	UOverlaySlot* const EquippedSlot = CardOverlay->AddChildToOverlay(M_EquippedText);
	EquippedSlot->SetHorizontalAlignment(HAlign_Right);
	EquippedSlot->SetVerticalAlignment(VAlign_Top);
	EquippedSlot->SetPadding(EquippedOffset);

	USizeBox* const MarkerBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	MarkerBox->SetWidthOverride(MarkerSize);
	MarkerBox->SetHeightOverride(MarkerSize);
	UOverlaySlot* const MarkerSlot = CardOverlay->AddChildToOverlay(MarkerBox);
	MarkerSlot->SetHorizontalAlignment(HAlign_Left);
	MarkerSlot->SetVerticalAlignment(VAlign_Top);
	MarkerSlot->SetPadding(MarkerOffset);
	M_SelectedMarker = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	M_SelectedMarker->SetVisibility(ESlateVisibility::Collapsed);
	MarkerBox->SetContent(M_SelectedMarker);
}

void UW_UnitVariantCard::SetupCard(UW_UnitVariantSelectionMenu* Menu, const EUnitVariant Variant,
                                   const FUnitVariantSelectionStyle& Style, const FSlateBrush& IconBrush,
                                   const FText& VariantName, const FText& Description)
{
	using namespace UnitVariantCardLayout;
	M_Menu = Menu;
	M_Variant = Variant;
	M_Style = Style;
	if (not GetIsValidButton())
	{
		return;
	}
	// An empty brush would draw Slate's default white box, so the well stays empty instead.
	const bool bHasIcon = IsValid(IconBrush.GetResourceObject());
	M_Icon->SetBrush(IconBrush);
	M_Icon->SetDesiredSizeOverride(FVector2D(IconSize, IconSize));
	M_Icon->SetVisibility(bHasIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	M_SelectedMarker->SetBrush(Style.SelectedMarker);

	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		M_LabelText, UnitVariantSelectionWidgets::MakeFont(Style.HeadingFont, LabelFontSize, LabelLetterSpacing),
		Style.LabelColor);
	M_LabelText->SetText(UEnum::GetDisplayValueAsText(Variant).ToUpper());
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		M_NameText, UnitVariantSelectionWidgets::MakeFont(Style.HeadingFont, NameFontSize), Style.TextColor);
	M_NameText->SetText(VariantName.ToUpper());
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		M_DescriptionText, UnitVariantSelectionWidgets::MakeFont(Style.BodyFont, DescriptionFontSize),
		Style.DescriptionColor);
	M_DescriptionText->SetText(Description);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		M_EquippedText, UnitVariantSelectionWidgets::MakeFont(Style.HeadingFont, LabelFontSize, LabelLetterSpacing),
		Style.AccentColor);
	ApplySelectionLook();
}

void UW_UnitVariantCard::SetIsSelected(const bool bIsSelected)
{
	bM_IsSelected = bIsSelected;
	ApplySelectionLook();
}

void UW_UnitVariantCard::ApplySelectionLook()
{
	if (not GetIsValidButton())
	{
		return;
	}
	M_Button->SetStyle(bM_IsSelected ? M_Style.CardSelected : M_Style.Card);
	M_IconWell->SetBrush(bM_IsSelected ? M_Style.CardIconWellSelected : M_Style.CardIconWell);
	M_NameText->SetColorAndOpacity(bM_IsSelected ? M_Style.SelectedTextColor : M_Style.TextColor);
	const ESlateVisibility BadgeVisibility = bM_IsSelected
		                                         ? ESlateVisibility::HitTestInvisible
		                                         : ESlateVisibility::Collapsed;
	M_EquippedText->SetVisibility(BadgeVisibility);
	M_SelectedMarker->SetVisibility(BadgeVisibility);
}

void UW_UnitVariantCard::OnCardClicked()
{
	if (not GetIsValidMenu())
	{
		return;
	}
	M_Menu->HandleVariantCardClicked(M_Variant);
}

bool UW_UnitVariantCard::GetIsValidMenu() const
{
	if (M_Menu.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_Menu", "GetIsValidMenu", this);
	return false;
}

bool UW_UnitVariantCard::GetIsValidButton() const
{
	if (IsValid(M_Button))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_Button", "GetIsValidButton", this);
	return false;
}
