// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_UnitVariantDiscardDialog.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
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

namespace UnitVariantDiscardDialogLayout
{
	const FLinearColor DimmerColor(0.f, 0.f, 0.f, 0.65f);
	constexpr float PlateWidth = 560.f;
	const FMargin PlatePadding(36.f, 28.f, 36.f, 32.f);
	const FMargin BodyPadding(0.f, 10.f, 0.f, 24.f);
	const FVector2D ButtonSize(220.f, 48.f);
	constexpr float ButtonGap = 16.f;
	constexpr int32 TitleFontSize = 22;
	constexpr int32 TitleLetterSpacing = 100;
	constexpr int32 BodyFontSize = 15;
}

void UW_UnitVariantDiscardDialog::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UW_UnitVariantDiscardDialog::BuildWidgetTree()
{
	using namespace UnitVariantDiscardDialogLayout;
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}
	UOverlay* const RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	WidgetTree->RootWidget = RootOverlay;
	M_Dimmer = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	M_Dimmer->SetColorAndOpacity(DimmerColor);
	UOverlaySlot* const DimmerSlot = RootOverlay->AddChildToOverlay(M_Dimmer);
	DimmerSlot->SetHorizontalAlignment(HAlign_Fill);
	DimmerSlot->SetVerticalAlignment(VAlign_Fill);

	USizeBox* const PlateBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	PlateBox->SetWidthOverride(PlateWidth);
	UOverlaySlot* const PlateSlot = RootOverlay->AddChildToOverlay(PlateBox);
	PlateSlot->SetHorizontalAlignment(HAlign_Center);
	PlateSlot->SetVerticalAlignment(VAlign_Center);
	M_Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	M_Plate->SetPadding(PlatePadding);
	PlateBox->SetContent(M_Plate);
	UVerticalBox* const PlateColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	M_Plate->SetContent(PlateColumn);
	M_TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_TitleText->SetText(NSLOCTEXT("UnitVariantSelection", "DiscardTitle", "DISCARD CHANGES?"));
	PlateColumn->AddChildToVerticalBox(M_TitleText)->SetHorizontalAlignment(HAlign_Center);
	M_BodyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_BodyText->SetAutoWrapText(true);
	M_BodyText->SetJustification(ETextJustify::Center);
	PlateColumn->AddChildToVerticalBox(M_BodyText)->SetPadding(BodyPadding);
	SetVisibility(ESlateVisibility::Collapsed);
}

void UW_UnitVariantDiscardDialog::SetupDialog(UW_UnitVariantSelectionMenu* Menu, const FUnitVariantSelectionStyle& Style)
{
	using namespace UnitVariantDiscardDialogLayout;
	M_Menu = Menu;
	if (not GetIsValidPlate() || IsValid(M_DiscardButton))
	{
		return;
	}
	M_Plate->SetBrush(Style.Card.Normal);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		M_TitleText, UnitVariantSelectionWidgets::MakeFont(Style.HeadingFont, TitleFontSize, TitleLetterSpacing),
		Style.SelectedTextColor);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		M_BodyText, UnitVariantSelectionWidgets::MakeFont(Style.BodyFont, BodyFontSize), Style.DescriptionColor);
	if (UVerticalBox* const PlateColumn = Cast<UVerticalBox>(M_Plate->GetContent()))
	{
		BuildButtonRow(PlateColumn, Style);
	}
}

void UW_UnitVariantDiscardDialog::BuildButtonRow(UVerticalBox* PlateColumn, const FUnitVariantSelectionStyle& Style)
{
	using namespace UnitVariantDiscardDialogLayout;
	UHorizontalBox* const ButtonRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	PlateColumn->AddChildToVerticalBox(ButtonRow)->SetHorizontalAlignment(HAlign_Center);
	UWidget* const KeepEditingBox = UnitVariantSelectionWidgets::BuildTextButton(
		*WidgetTree, Style, NSLOCTEXT("UnitVariantSelection", "KeepEditing", "KEEP EDITING"), Style.BackButton,
		ButtonSize, M_KeepEditingButton);
	M_KeepEditingButton->OnClicked.AddDynamic(this, &UW_UnitVariantDiscardDialog::OnKeepEditingClicked);
	ButtonRow->AddChildToHorizontalBox(KeepEditingBox)->SetPadding(FMargin(0.f, 0.f, ButtonGap, 0.f));
	UWidget* const DiscardBox = UnitVariantSelectionWidgets::BuildTextButton(
		*WidgetTree, Style, NSLOCTEXT("UnitVariantSelection", "Discard", "DISCARD"), Style.ConfirmButton,
		ButtonSize, M_DiscardButton);
	M_DiscardButton->OnClicked.AddDynamic(this, &UW_UnitVariantDiscardDialog::OnDiscardClicked);
	ButtonRow->AddChildToHorizontalBox(DiscardBox);
}

void UW_UnitVariantDiscardDialog::ShowDialog(const int32 ChangeCount)
{
	if (not GetIsValidPlate())
	{
		return;
	}
	M_BodyText->SetText(FText::Format(
		NSLOCTEXT("UnitVariantSelection", "DiscardBody",
		          "{0} {0}|plural(one=unit has,other=units have) a variant choice that is not confirmed. "
		          "Going back discards {0}|plural(one=it,other=them)."),
		FText::AsNumber(ChangeCount)));
	bM_IsOpen = true;
	SetVisibility(ESlateVisibility::Visible);
}

void UW_UnitVariantDiscardDialog::HideDialog()
{
	bM_IsOpen = false;
	SetVisibility(ESlateVisibility::Collapsed);
}

void UW_UnitVariantDiscardDialog::OnKeepEditingClicked()
{
	if (not GetIsValidMenu())
	{
		return;
	}
	M_Menu->HandleDiscardDialogChoice(false);
}

void UW_UnitVariantDiscardDialog::OnDiscardClicked()
{
	if (not GetIsValidMenu())
	{
		return;
	}
	M_Menu->HandleDiscardDialogChoice(true);
}

bool UW_UnitVariantDiscardDialog::GetIsValidMenu() const
{
	if (M_Menu.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_Menu", "GetIsValidMenu", this);
	return false;
}

bool UW_UnitVariantDiscardDialog::GetIsValidPlate() const
{
	if (IsValid(M_Plate) && IsValid(M_TitleText) && IsValid(M_BodyText))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_Plate", "GetIsValidPlate", this);
	return false;
}
