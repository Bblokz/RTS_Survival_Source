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
#include "Components/WrapBox.h"
#include "Engine/Texture2D.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "UnitVariantAbilityCards.h"
#include "UnitVariantSelectionButton.h"
#include "UnitVariantSelectionWidgetHelpers.h"
#include "W_UnitVariantSelectionMenu.h"

// Measured on Custom_VariantCard (variant_selection_spec.md, section 2). The 9-slice card art stretches, so the
// card takes the width of the column and grows taller than its 216 minimum when the description needs more lines.
namespace UnitVariantCardLayout
{
	constexpr float CardMinHeight = 216.f;
	constexpr float IconWellSize = 136.f;
	constexpr float IconSize = 128.f;
	const FMargin IconWellOffset(16.f, 40.f, 0.f, 0.f);
	const FMargin IconWellPadding(4.f);
	// Text column from x 168 to 16 short of the right rim; the label centres on y 48, the name on y 74, the
	// description starts at y 88. The bottom keeps the description off the rim when it makes the card grow.
	const FMargin TextColumnOffset(168.f, 40.f, 16.f, 24.f);
	const FMargin NamePadding(0.f, 6.f, 0.f, 2.f);
	constexpr float MarkerSize = 28.f;
	// Spec (388,34) on a 440 wide card, kept from the right so it follows the rim when a scroll bar narrows the card.
	const FMargin MarkerOffset(0.f, 34.f, 24.f, 0.f);
	// EQUIPPED ends at x 380 on the label's line.
	const FMargin EquippedOffset(0.f, 40.f, 60.f, 0.f);
	constexpr int32 LabelFontSize = 12;
	constexpr int32 LabelLetterSpacing = 100;
	constexpr int32 NameFontSize = 20;
	constexpr int32 DescriptionFontSize = 14;
	constexpr float DescriptionLineHeight = 1.15f;
	// Ability icons under the description, in a smaller copy of the unit list's icon well.
	const FMargin AbilityIconRowPadding(0.f, 10.f, 0.f, 0.f);
	constexpr float AbilityIconWellSize = 40.f;
	const FMargin AbilityIconWellPadding(3.f);
	constexpr float AbilityIconSpacing = 6.f;
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
	SizeBox->SetMinDesiredHeight(CardMinHeight);
	WidgetTree->RootWidget = SizeBox;

	// Never takes keyboard focus, so the menu keeps the arrow keys for its navigation.
	M_Button = WidgetTree->ConstructWidget<UUnitVariantSelectionButton>(UUnitVariantSelectionButton::StaticClass());
	M_Button->OnClicked.AddDynamic(this, &UW_UnitVariantCard::OnCardClicked);
	M_Button->OnHovered.AddDynamic(this, &UW_UnitVariantCard::OnCardHovered);
	M_Button->OnUnhovered.AddDynamic(this, &UW_UnitVariantCard::OnCardUnhovered);
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
	UVerticalBox* const Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Column->SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlaySlot* const ColumnSlot = CardOverlay->AddChildToOverlay(Column);
	// Fill, so the wrapping texts take the column width the card is given and their height sizes the card.
	ColumnSlot->SetHorizontalAlignment(HAlign_Fill);
	ColumnSlot->SetVerticalAlignment(VAlign_Top);
	ColumnSlot->SetPadding(TextColumnOffset);

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
	M_AbilityIconRow = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
	M_AbilityIconRow->SetInnerSlotPadding(FVector2D(AbilityIconSpacing, AbilityIconSpacing));
	M_AbilityIconRow->SetVisibility(ESlateVisibility::Collapsed);
	Column->AddChildToVerticalBox(M_AbilityIconRow)->SetPadding(AbilityIconRowPadding);
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
	MarkerSlot->SetHorizontalAlignment(HAlign_Right);
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

void UW_UnitVariantCard::SetAbilityIcons(const TArray<FUnitVariantAbilityCard>& AbilityCards)
{
	using namespace UnitVariantCardLayout;
	if (not GetIsValidAbilityIconRow())
	{
		return;
	}
	M_AbilityIconRow->ClearChildren();
	for (const FUnitVariantAbilityCard& AbilityCard : AbilityCards)
	{
		if (not IsValid(AbilityCard.Icon))
		{
			continue;
		}
		USizeBox* const IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		IconBox->SetWidthOverride(AbilityIconWellSize);
		IconBox->SetHeightOverride(AbilityIconWellSize);
		UBorder* const IconWell = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		IconWell->SetBrush(M_Style.UnitIconWell);
		IconWell->SetPadding(AbilityIconWellPadding);
		IconBox->SetContent(IconWell);
		UImage* const Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Icon->SetBrushFromTexture(AbilityCard.Icon);
		IconWell->SetContent(Icon);
		M_AbilityIconRow->AddChildToWrapBox(IconBox);
	}
	M_AbilityIconRow->SetVisibility(M_AbilityIconRow->HasAnyChildren()
		                                ? ESlateVisibility::HitTestInvisible
		                                : ESlateVisibility::Collapsed);
}

void UW_UnitVariantCard::SetIsKeyboardFocused(const bool bIsKeyboardFocused)
{
	bM_IsKeyboardFocused = bIsKeyboardFocused;
	ApplySelectionLook();
}

void UW_UnitVariantCard::ApplySelectionLook()
{
	if (not GetIsValidButton())
	{
		return;
	}
	FButtonStyle ButtonStyle = bM_IsSelected ? M_Style.CardSelected : M_Style.Card;
	if (bM_IsKeyboardFocused)
	{
		ButtonStyle.Normal = ButtonStyle.Hovered;
	}
	M_Button->SetStyle(ButtonStyle);
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

void UW_UnitVariantCard::OnCardHovered()
{
	if (not GetIsValidMenu())
	{
		return;
	}
	M_Menu->HandleVariantCardHovered(M_Variant, true);
}

void UW_UnitVariantCard::OnCardUnhovered()
{
	if (not GetIsValidMenu())
	{
		return;
	}
	M_Menu->HandleVariantCardHovered(M_Variant, false);
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

bool UW_UnitVariantCard::GetIsValidAbilityIconRow() const
{
	if (IsValid(M_AbilityIconRow))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_AbilityIconRow", "GetIsValidAbilityIconRow",
	                                                             this);
	return false;
}
