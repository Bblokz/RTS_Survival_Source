// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_FactionTeamColorPicker.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantSelectionWidgetHelpers.h"
#include "RTS_Survival/FactionSystem/FactionSelection/W_FactionSelectionMenu.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "W_FactionTeamColorSwatch.h"

void UW_FactionTeamColorPicker::SetupPicker(UW_FactionSelectionMenu* Menu, const FFactionUnitBrowserStyle& Style,
                                            const FLinearColor& CurrentTeamColor)
{
	M_Menu = Menu;
	M_Options = FactionTeamColorPickerLayout::GetTeamColorOptions();
	BuildWidgetTree(Style);
	SelectSwatch(FindOptionIndex(CurrentTeamColor));
}

void UW_FactionTeamColorPicker::BuildWidgetTree(const FFactionUnitBrowserStyle& Style)
{
	using namespace FactionTeamColorPickerLayout;
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}
	USizeBox* const SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SizeBox->SetWidthOverride(PanelWidth);
	SizeBox->SetHeightOverride(PanelHeight);
	WidgetTree->RootWidget = SizeBox;

	UBorder* const Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Panel->SetBrush(Style.Panel);
	Panel->SetPadding(FMargin(0.f));
	Panel->SetHorizontalAlignment(HAlign_Fill);
	Panel->SetVerticalAlignment(VAlign_Fill);
	SizeBox->SetContent(Panel);

	UOverlay* const PanelOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	Panel->SetContent(PanelOverlay);
	BuildHeader(PanelOverlay, Style);

	M_SwatchRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UOverlaySlot* const RowSlot = PanelOverlay->AddChildToOverlay(M_SwatchRow);
	RowSlot->SetHorizontalAlignment(HAlign_Left);
	RowSlot->SetVerticalAlignment(VAlign_Top);
	RowSlot->SetPadding(SwatchRowOffset);
	BuildSwatches(Style);
}

void UW_FactionTeamColorPicker::BuildHeader(UOverlay* PanelOverlay, const FFactionUnitBrowserStyle& Style)
{
	using namespace FactionTeamColorPickerLayout;
	USizeBox* const HeaderBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	HeaderBox->SetHeightOverride(HeaderHeight);
	HeaderBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlaySlot* const HeaderSlot = PanelOverlay->AddChildToOverlay(HeaderBox);
	HeaderSlot->SetHorizontalAlignment(HAlign_Fill);
	HeaderSlot->SetVerticalAlignment(VAlign_Top);

	UOverlay* const HeaderOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	HeaderBox->SetContent(HeaderOverlay);
	UTextBlock* const TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		TitleText, UnitVariantSelectionWidgets::MakeFont(Style.HeadingFont, TitleFontSize, TitleLetterSpacing),
		Style.TextColor);
	TitleText->SetText(NSLOCTEXT("FactionTeamColor", "Title", "TEAM COLOUR"));
	UOverlaySlot* const TitleSlot = HeaderOverlay->AddChildToOverlay(TitleText);
	TitleSlot->SetHorizontalAlignment(HAlign_Left);
	TitleSlot->SetVerticalAlignment(VAlign_Center);
	TitleSlot->SetPadding(TitleOffset);

	// The picked colour's name on the right of the header.
	M_ColorNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		M_ColorNameText,
		UnitVariantSelectionWidgets::MakeFont(Style.HeadingFont, ColorNameFontSize, ColorNameLetterSpacing),
		Style.TabTextColor);
	UOverlaySlot* const NameSlot = HeaderOverlay->AddChildToOverlay(M_ColorNameText);
	NameSlot->SetHorizontalAlignment(HAlign_Right);
	NameSlot->SetVerticalAlignment(VAlign_Center);
	NameSlot->SetPadding(ColorNameOffset);
}

void UW_FactionTeamColorPicker::BuildSwatches(const FFactionUnitBrowserStyle& Style)
{
	using namespace FactionTeamColorPickerLayout;
	M_Swatches.Reset();
	for (int32 OptionIndex = 0; OptionIndex < M_Options.Num(); ++OptionIndex)
	{
		UW_FactionTeamColorSwatch* const Swatch = CreateWidget<UW_FactionTeamColorSwatch>(
			this, UW_FactionTeamColorSwatch::StaticClass());
		if (not IsValid(Swatch))
		{
			continue;
		}
		Swatch->SetupSwatch(this, OptionIndex, M_Options[OptionIndex], Style);
		UHorizontalBoxSlot* const SwatchSlot = M_SwatchRow->AddChildToHorizontalBox(Swatch);
		SwatchSlot->SetPadding(FMargin(OptionIndex == 0 ? 0.f : SwatchSpacing, 0.f, 0.f, 0.f));
		M_Swatches.Add(Swatch);
	}
}

void UW_FactionTeamColorPicker::HandleSwatchClicked(const int32 SwatchIndex)
{
	if (not M_Options.IsValidIndex(SwatchIndex))
	{
		return;
	}
	SelectSwatch(SwatchIndex);
	if (not GetIsValidMenu())
	{
		return;
	}
	M_Menu->HandleTeamColorSelected(M_Options[SwatchIndex].Color);
}

UW_FactionTeamColorSwatch* UW_FactionTeamColorPicker::GetSwatch(const int32 SwatchIndex) const
{
	return M_Swatches.IsValidIndex(SwatchIndex) ? M_Swatches[SwatchIndex].Get() : nullptr;
}

void UW_FactionTeamColorPicker::SelectSwatch(const int32 SwatchIndex)
{
	M_SelectedIndex = SwatchIndex;
	for (int32 Index = 0; Index < M_Swatches.Num(); ++Index)
	{
		if (IsValid(M_Swatches[Index]))
		{
			M_Swatches[Index]->SetIsSelected(Index == SwatchIndex);
		}
	}
	if (IsValid(M_ColorNameText) && M_Options.IsValidIndex(SwatchIndex))
	{
		M_ColorNameText->SetText(M_Options[SwatchIndex].Name.ToUpper());
	}
}

int32 UW_FactionTeamColorPicker::FindOptionIndex(const FLinearColor& TeamColor) const
{
	// Every colour with alpha 0 is the camo default, whatever its RGB.
	constexpr float ColorTolerance = 0.01f;
	for (int32 OptionIndex = 0; OptionIndex < M_Options.Num(); ++OptionIndex)
	{
		const FLinearColor& OptionColor = M_Options[OptionIndex].Color;
		const bool bBothDefault = OptionColor.A <= 0.f && TeamColor.A <= 0.f;
		if (bBothDefault || OptionColor.Equals(TeamColor, ColorTolerance))
		{
			return OptionIndex;
		}
	}
	return 0;
}

bool UW_FactionTeamColorPicker::GetIsValidMenu() const
{
	if (M_Menu.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, "M_Menu", "UW_FactionTeamColorPicker::GetIsValidMenu", this);
	return false;
}
