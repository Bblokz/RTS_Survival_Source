// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_SurvivalArmyTab.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantSelectionButton.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantSelectionWidgetHelpers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "SurvivalUiLayout.h"
#include "SurvivalUiStyle.h"
#include "W_SurvivalArmySelection.h"

void UW_SurvivalArmyTab::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UW_SurvivalArmyTab::BuildWidgetTree()
{
	using namespace SurvivalUiLayout;
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}
	USizeBox* const SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SizeBox->SetWidthOverride(TabWidth);
	SizeBox->SetHeightOverride(TabHeight);
	WidgetTree->RootWidget = SizeBox;

	// Never takes keyboard focus, like the variant menu buttons.
	M_Button = WidgetTree->ConstructWidget<UUnitVariantSelectionButton>(UUnitVariantSelectionButton::StaticClass());
	M_Button->OnClicked.AddDynamic(this, &UW_SurvivalArmyTab::OnTabClicked);
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

void UW_SurvivalArmyTab::SetupTab(UW_SurvivalArmySelection* Owner, const ESurvivalRosterCategory Category,
                                  const USurvivalUiStyleAsset* StyleAsset)
{
	using namespace SurvivalUiLayout;
	M_Owner = Owner;
	M_Category = Category;
	M_StyleAsset = StyleAsset;
	if (not GetIsValidButton())
	{
		return;
	}
	static const FSurvivalUiStyle DefaultStyle;
	const FSurvivalUiStyle& Style = IsValid(M_StyleAsset) ? M_StyleAsset->Style : DefaultStyle;
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		M_LabelText, UnitVariantSelectionWidgets::MakeFont(Style.HeadingFont, TabFontSize, TabLetterSpacing),
		Style.TabTextColor);
	M_LabelText->SetText(SurvivalArmyUi::GetRosterCategoryLabel(Category));
	ApplySelectionLook();
}

void UW_SurvivalArmyTab::SetIsSelected(const bool bIsSelected)
{
	bM_IsSelected = bIsSelected;
	ApplySelectionLook();
}

void UW_SurvivalArmyTab::ApplySelectionLook()
{
	if (not GetIsValidButton() || not IsValid(M_StyleAsset))
	{
		return;
	}
	const FSurvivalUiStyle& Style = M_StyleAsset->Style;
	M_Button->SetStyle(bM_IsSelected ? Style.TabSelected : Style.Tab);
	M_LabelText->SetColorAndOpacity(bM_IsSelected ? Style.TextColor : Style.TabTextColor);
}

void UW_SurvivalArmyTab::OnTabClicked()
{
	if (not GetIsValidOwner())
	{
		return;
	}
	M_Owner->HandleTabClicked(M_Category);
}

bool UW_SurvivalArmyTab::GetIsValidOwner() const
{
	if (M_Owner.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_Owner", "GetIsValidOwner", this);
	return false;
}

bool UW_SurvivalArmyTab::GetIsValidButton() const
{
	if (IsValid(M_Button))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_Button", "GetIsValidButton", this);
	return false;
}
