// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_UnitVariantUnitEntry.h"

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
#include "UnitVariantSelectionButton.h"
#include "UnitVariantSelectionWidgetHelpers.h"
#include "W_UnitVariantSelectionMenu.h"

// Measured on Custom_VariantUnitEntry (variant_selection_spec.md, section 2).
namespace UnitVariantUnitEntryLayout
{
	constexpr float EntryWidth = 352.f;
	constexpr float EntryHeight = 80.f;
	constexpr float IconWellSize = 72.f;
	constexpr float IconSize = 64.f;
	const FMargin IconWellOffset(16.f, 4.f, 0.f, 0.f);
	const FMargin IconWellPadding(4.f);
	// Text from x 102 with 32 units to the right edge; the two lines centre on y 30 and y 54.
	const FMargin TextOffset(102.f, 18.f, 32.f, 0.f);
	const FMargin DetailPadding(0.f, 2.f, 0.f, 0.f);
	constexpr int32 NameFontSize = 18;
	constexpr int32 DetailFontSize = 13;
	constexpr int32 DetailLetterSpacing = 50;
	// On the top right corner of the icon well (16..88, 4..76).
	constexpr float ChangedMarkerSize = 10.f;
	const FMargin ChangedMarkerOffset(82.f, 2.f, 0.f, 0.f);
}

void UW_UnitVariantUnitEntry::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UW_UnitVariantUnitEntry::BuildWidgetTree()
{
	using namespace UnitVariantUnitEntryLayout;
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}
	USizeBox* const SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SizeBox->SetWidthOverride(EntryWidth);
	SizeBox->SetHeightOverride(EntryHeight);
	WidgetTree->RootWidget = SizeBox;

	// Never takes keyboard focus, so the menu keeps the arrow keys for its navigation.
	M_Button = WidgetTree->ConstructWidget<UUnitVariantSelectionButton>(UUnitVariantSelectionButton::StaticClass());
	M_Button->OnClicked.AddDynamic(this, &UW_UnitVariantUnitEntry::OnEntryClicked);
	SizeBox->SetContent(M_Button);

	UOverlay* const EntryOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	// The button centres its content by default; the overlay must fill it so the spec offsets hold.
	if (UButtonSlot* const ContentSlot = Cast<UButtonSlot>(M_Button->SetContent(EntryOverlay)))
	{
		ContentSlot->SetPadding(FMargin(0.f));
		ContentSlot->SetHorizontalAlignment(HAlign_Fill);
		ContentSlot->SetVerticalAlignment(VAlign_Fill);
	}
	BuildIconWell(EntryOverlay);
	BuildTexts(EntryOverlay);
	BuildChangedMarker(EntryOverlay);
}

void UW_UnitVariantUnitEntry::BuildChangedMarker(UOverlay* EntryOverlay)
{
	using namespace UnitVariantUnitEntryLayout;
	USizeBox* const MarkerBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	MarkerBox->SetWidthOverride(ChangedMarkerSize);
	MarkerBox->SetHeightOverride(ChangedMarkerSize);
	UOverlaySlot* const MarkerSlot = EntryOverlay->AddChildToOverlay(MarkerBox);
	MarkerSlot->SetHorizontalAlignment(HAlign_Left);
	MarkerSlot->SetVerticalAlignment(VAlign_Top);
	MarkerSlot->SetPadding(ChangedMarkerOffset);
	// The default brush is a plain white quad; the tint makes it the accent colour.
	M_ChangedMarker = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	M_ChangedMarker->SetVisibility(ESlateVisibility::Collapsed);
	MarkerBox->SetContent(M_ChangedMarker);
}

void UW_UnitVariantUnitEntry::BuildIconWell(UOverlay* EntryOverlay)
{
	using namespace UnitVariantUnitEntryLayout;
	USizeBox* const WellBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	WellBox->SetWidthOverride(IconWellSize);
	WellBox->SetHeightOverride(IconWellSize);
	UOverlaySlot* const WellSlot = EntryOverlay->AddChildToOverlay(WellBox);
	WellSlot->SetHorizontalAlignment(HAlign_Left);
	WellSlot->SetVerticalAlignment(VAlign_Top);
	WellSlot->SetPadding(IconWellOffset);

	M_IconWell = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	M_IconWell->SetPadding(IconWellPadding);
	M_IconWell->SetVisibility(ESlateVisibility::HitTestInvisible);
	WellBox->SetContent(M_IconWell);
	M_Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	M_Icon->SetDesiredSizeOverride(FVector2D(IconSize, IconSize));
	M_IconWell->SetContent(M_Icon);
}

void UW_UnitVariantUnitEntry::BuildTexts(UOverlay* EntryOverlay)
{
	using namespace UnitVariantUnitEntryLayout;
	UVerticalBox* const TextColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	TextColumn->SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlaySlot* const TextSlot = EntryOverlay->AddChildToOverlay(TextColumn);
	TextSlot->SetHorizontalAlignment(HAlign_Fill);
	TextSlot->SetVerticalAlignment(VAlign_Top);
	TextSlot->SetPadding(TextOffset);

	M_NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_NameText->SetClipping(EWidgetClipping::ClipToBounds);
	TextColumn->AddChildToVerticalBox(M_NameText);
	M_DetailText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TextColumn->AddChildToVerticalBox(M_DetailText)->SetPadding(DetailPadding);
}

void UW_UnitVariantUnitEntry::SetupEntry(UW_UnitVariantSelectionMenu* Menu, const int32 EntryIndex,
                                         const FUnitVariantSelectionStyle& Style, const FSlateBrush& UnitIcon,
                                         const FText& UnitName, const int32 VariantCount)
{
	using namespace UnitVariantUnitEntryLayout;
	M_Menu = Menu;
	M_EntryIndex = EntryIndex;
	M_Style = Style;
	M_VariantCount = VariantCount;
	if (not GetIsValidButton())
	{
		return;
	}
	// The image's brush is a UPROPERTY, so it keeps the icon texture loaded. An empty brush would draw Slate's
	// default white box, so the well stays empty instead.
	const bool bHasIcon = IsValid(UnitIcon.GetResourceObject());
	if (bHasIcon)
	{
		M_Icon->SetBrush(UnitIcon);
		M_Icon->SetDesiredSizeOverride(FVector2D(IconSize, IconSize));
	}
	M_Icon->SetVisibility(bHasIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		M_NameText, UnitVariantSelectionWidgets::MakeFont(Style.HeadingFont, NameFontSize), Style.TextColor);
	M_NameText->SetText(UnitName.ToUpper());
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		M_DetailText, UnitVariantSelectionWidgets::MakeFont(Style.BodyFont, DetailFontSize, DetailLetterSpacing),
		Style.MutedTextColor);
	SetChosenVariantName(FText::GetEmpty());
	ApplySelectionLook();
}

void UW_UnitVariantUnitEntry::SetIsSelected(const bool bIsSelected)
{
	bM_IsSelected = bIsSelected;
	ApplySelectionLook();
}

void UW_UnitVariantUnitEntry::SetChosenVariantName(const FText& ChosenVariantName)
{
	if (not IsValid(M_DetailText))
	{
		return;
	}
	const FText VariantCountText = FText::Format(
		NSLOCTEXT("UnitVariantSelection", "VariantCount", "{0} VARIANTS"), FText::AsNumber(M_VariantCount));
	if (ChosenVariantName.IsEmpty())
	{
		M_DetailText->SetText(VariantCountText);
		return;
	}
	M_DetailText->SetText(FText::Format(NSLOCTEXT("UnitVariantSelection", "ChosenVariant", "{0}  |  {1}"),
	                                    VariantCountText, ChosenVariantName.ToUpper()));
}

void UW_UnitVariantUnitEntry::SetHasUnconfirmedChange(const bool bHasUnconfirmedChange)
{
	if (not GetIsValidButton())
	{
		return;
	}
	M_ChangedMarker->SetColorAndOpacity(M_Style.AccentColor.GetSpecifiedColor());
	M_ChangedMarker->SetVisibility(bHasUnconfirmedChange ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UW_UnitVariantUnitEntry::ApplySelectionLook()
{
	if (not GetIsValidButton())
	{
		return;
	}
	M_Button->SetStyle(bM_IsSelected ? M_Style.UnitEntrySelected : M_Style.UnitEntry);
	M_IconWell->SetBrush(bM_IsSelected ? M_Style.UnitIconWellSelected : M_Style.UnitIconWell);
	M_NameText->SetColorAndOpacity(bM_IsSelected ? M_Style.SelectedTextColor : M_Style.TextColor);
	M_DetailText->SetColorAndOpacity(bM_IsSelected ? M_Style.SelectedMutedTextColor : M_Style.MutedTextColor);
}

void UW_UnitVariantUnitEntry::OnEntryClicked()
{
	if (not GetIsValidMenu())
	{
		return;
	}
	M_Menu->HandleUnitEntryClicked(M_EntryIndex);
}

bool UW_UnitVariantUnitEntry::GetIsValidMenu() const
{
	if (M_Menu.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_Menu", "GetIsValidMenu", this);
	return false;
}

bool UW_UnitVariantUnitEntry::GetIsValidButton() const
{
	if (IsValid(M_Button))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_Button", "GetIsValidButton", this);
	return false;
}
