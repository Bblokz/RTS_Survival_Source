// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_FactionTeamColorSwatch.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "FactionTeamColorPickerLayout.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantSelectionWidgetHelpers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "W_FactionTeamColorPicker.h"

void UW_FactionTeamColorSwatch::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UW_FactionTeamColorSwatch::BuildWidgetTree()
{
	using namespace FactionTeamColorPickerLayout;
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}
	USizeBox* const SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SizeBox->SetWidthOverride(SwatchSize);
	SizeBox->SetHeightOverride(SwatchSize);
	WidgetTree->RootWidget = SizeBox;

	M_Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	M_Button->OnClicked.AddDynamic(this, &UW_FactionTeamColorSwatch::OnSwatchClicked);
	SizeBox->SetContent(M_Button);

	UOverlay* const SwatchOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	if (UButtonSlot* const ContentSlot = Cast<UButtonSlot>(M_Button->SetContent(SwatchOverlay)))
	{
		ContentSlot->SetPadding(FMargin(0.f));
		ContentSlot->SetHorizontalAlignment(HAlign_Fill);
		ContentSlot->SetVerticalAlignment(VAlign_Fill);
	}
	M_Chip = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	M_Chip->SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlaySlot* const ChipSlot = SwatchOverlay->AddChildToOverlay(M_Chip);
	ChipSlot->SetHorizontalAlignment(HAlign_Fill);
	ChipSlot->SetVerticalAlignment(VAlign_Fill);
	ChipSlot->SetPadding(ChipPadding);

	M_DefaultLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_DefaultLabel->SetVisibility(ESlateVisibility::Collapsed);
	M_DefaultLabel->SetJustification(ETextJustify::Center);
	UOverlaySlot* const LabelSlot = SwatchOverlay->AddChildToOverlay(M_DefaultLabel);
	LabelSlot->SetHorizontalAlignment(HAlign_Center);
	LabelSlot->SetVerticalAlignment(VAlign_Center);
}

void UW_FactionTeamColorSwatch::SetupSwatch(UW_FactionTeamColorPicker* Picker, const int32 SwatchIndex,
                                            const FFactionTeamColorOption& Option,
                                            const FFactionUnitBrowserStyle& Style)
{
	using namespace FactionTeamColorPickerLayout;
	M_Picker = Picker;
	M_SwatchIndex = SwatchIndex;
	M_Style = Style;
	if (not GetIsValidButton())
	{
		return;
	}
	const bool bIsCamoDefault = Option.Color.A <= 0.f;
	M_Chip->SetColorAndOpacity(FLinearColor(Option.Color.R, Option.Color.G, Option.Color.B, 1.f));
	M_Chip->SetVisibility(bIsCamoDefault ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	if (bIsCamoDefault)
	{
		UnitVariantSelectionWidgets::ApplyTextOnMetal(
			M_DefaultLabel, UnitVariantSelectionWidgets::MakeFont(Style.HeadingFont, DefaultLabelFontSize),
			Style.TabTextColor);
		M_DefaultLabel->SetText(Option.Name.ToUpper());
		M_DefaultLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	M_Button->SetToolTipText(bIsCamoDefault
		                         ? NSLOCTEXT("FactionTeamColor", "CamoTooltip", "No team colour: every vehicle keeps its camo.")
		                         : Option.Name);
	SetIsSelected(bM_IsSelected);
}

void UW_FactionTeamColorSwatch::SetIsSelected(const bool bIsSelected)
{
	bM_IsSelected = bIsSelected;
	if (not GetIsValidButton())
	{
		return;
	}
	M_Button->SetStyle(bM_IsSelected ? M_Style.TabSelected : M_Style.Tab);
	M_DefaultLabel->SetColorAndOpacity(bM_IsSelected ? M_Style.SelectedTextColor : M_Style.TabTextColor);
}

void UW_FactionTeamColorSwatch::SimulateClick()
{
	if (not GetIsValidButton())
	{
		return;
	}
	M_Button->OnClicked.Broadcast();
}

void UW_FactionTeamColorSwatch::OnSwatchClicked()
{
	if (not GetIsValidPicker())
	{
		return;
	}
	M_Picker->HandleSwatchClicked(M_SwatchIndex);
}

bool UW_FactionTeamColorSwatch::GetIsValidPicker() const
{
	if (M_Picker.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, "M_Picker", "UW_FactionTeamColorSwatch::GetIsValidPicker", this);
	return false;
}

bool UW_FactionTeamColorSwatch::GetIsValidButton() const
{
	if (IsValid(M_Button) && IsValid(M_Chip) && IsValid(M_DefaultLabel))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, "M_Button", "UW_FactionTeamColorSwatch::GetIsValidButton", this);
	return false;
}
