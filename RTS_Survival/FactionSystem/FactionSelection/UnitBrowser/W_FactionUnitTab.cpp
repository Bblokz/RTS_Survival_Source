// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_FactionUnitTab.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "FactionUnitBrowserLayout.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantSelectionWidgetHelpers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "W_FactionUnitBrowser.h"

void UW_FactionUnitTab::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UW_FactionUnitTab::BuildWidgetTree()
{
	using namespace FactionUnitBrowserLayout;
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}
	USizeBox* const SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SizeBox->SetWidthOverride(TabWidth);
	SizeBox->SetHeightOverride(TabHeight);
	WidgetTree->RootWidget = SizeBox;

	M_Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	M_Button->OnClicked.AddDynamic(this, &UW_FactionUnitTab::OnTabClicked);
	SizeBox->SetContent(M_Button);

	M_LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_LabelText->SetVisibility(ESlateVisibility::HitTestInvisible);
	M_LabelText->SetJustification(ETextJustify::Center);
	if (UButtonSlot* const LabelSlot = Cast<UButtonSlot>(M_Button->SetContent(M_LabelText)))
	{
		LabelSlot->SetPadding(TabTextPadding);
		LabelSlot->SetHorizontalAlignment(HAlign_Center);
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}
}

void UW_FactionUnitTab::SetupTab(UW_FactionUnitBrowser* Browser, const int32 TabIndex,
                                 const FFactionUnitBrowserStyle& Style, const FText& Label)
{
	using namespace FactionUnitBrowserLayout;
	M_Browser = Browser;
	M_TabIndex = TabIndex;
	M_Style = Style;
	if (not GetIsValidButton())
	{
		return;
	}
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		M_LabelText, UnitVariantSelectionWidgets::MakeFont(Style.HeadingFont, TabFontSize, TabLetterSpacing),
		Style.TabTextColor);
	M_LabelText->SetText(Label.ToUpper());
	ApplySelectionLook();
}

void UW_FactionUnitTab::SetIsSelected(const bool bIsSelected)
{
	bM_IsSelected = bIsSelected;
	ApplySelectionLook();
}

void UW_FactionUnitTab::SimulateClick()
{
	if (not GetIsValidButton())
	{
		return;
	}
	M_Button->OnClicked.Broadcast();
}

void UW_FactionUnitTab::ApplySelectionLook()
{
	if (not GetIsValidButton())
	{
		return;
	}
	M_Button->SetStyle(bM_IsSelected ? M_Style.TabSelected : M_Style.Tab);
	M_LabelText->SetColorAndOpacity(bM_IsSelected ? M_Style.SelectedTextColor : M_Style.TabTextColor);
}

void UW_FactionUnitTab::OnTabClicked()
{
	if (not GetIsValidBrowser())
	{
		return;
	}
	M_Browser->HandleTabClicked(M_TabIndex);
}

bool UW_FactionUnitTab::GetIsValidBrowser() const
{
	if (M_Browser.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_Browser", "GetIsValidBrowser", this);
	return false;
}

bool UW_FactionUnitTab::GetIsValidButton() const
{
	if (IsValid(M_Button))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_Button", "GetIsValidButton", this);
	return false;
}
