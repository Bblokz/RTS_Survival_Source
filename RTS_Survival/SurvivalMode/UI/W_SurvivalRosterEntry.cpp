// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_SurvivalRosterEntry.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantSelectionWidgetHelpers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "SurvivalArmyUiTypes.h"
#include "SurvivalUiLayout.h"
#include "SurvivalUiStyle.h"
#include "W_SurvivalArmySelection.h"

void UW_SurvivalRosterEntry::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UW_SurvivalRosterEntry::BuildWidgetTree()
{
	using namespace SurvivalUiLayout;
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}
	USizeBox* const SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SizeBox->SetWidthOverride(RosterEntryWidth);
	SizeBox->SetHeightOverride(RosterEntryHeight);
	WidgetTree->RootWidget = SizeBox;

	UOverlay* const EntryOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	SizeBox->SetContent(EntryOverlay);

	M_Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	M_Background->SetPadding(FMargin(0.f));
	UOverlaySlot* const BackgroundSlot = EntryOverlay->AddChildToOverlay(M_Background);
	BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
	BackgroundSlot->SetVerticalAlignment(VAlign_Fill);

	BuildIcon(EntryOverlay);
	BuildTexts(EntryOverlay);
	BuildSupply(EntryOverlay);
}

void UW_SurvivalRosterEntry::BuildIcon(UOverlay* EntryOverlay)
{
	using namespace SurvivalUiLayout;
	USizeBox* const WellBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	WellBox->SetWidthOverride(RosterIconWellSize);
	WellBox->SetHeightOverride(RosterIconWellSize);
	WellBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlaySlot* const WellSlot = EntryOverlay->AddChildToOverlay(WellBox);
	WellSlot->SetHorizontalAlignment(HAlign_Left);
	WellSlot->SetVerticalAlignment(VAlign_Top);
	WellSlot->SetPadding(RosterIconWellOffset);
	M_IconWell = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	M_IconWell->SetPadding(RosterIconWellPadding);
	WellBox->SetContent(M_IconWell);
	M_Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	M_Icon->SetVisibility(ESlateVisibility::Hidden);
	M_IconWell->SetContent(M_Icon);
}

void UW_SurvivalRosterEntry::BuildTexts(UOverlay* EntryOverlay)
{
	using namespace SurvivalUiLayout;
	const auto AddText = [this, EntryOverlay](const FMargin& Offset) -> UTextBlock*
	{
		USizeBox* const TextBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		TextBox->SetWidthOverride(RosterNameWidth);
		TextBox->SetVisibility(ESlateVisibility::HitTestInvisible);
		UOverlaySlot* const TextSlot = EntryOverlay->AddChildToOverlay(TextBox);
		TextSlot->SetHorizontalAlignment(HAlign_Left);
		TextSlot->SetVerticalAlignment(VAlign_Top);
		TextSlot->SetPadding(Offset);
		UTextBlock* const Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Text->SetClipping(EWidgetClipping::ClipToBounds);
		TextBox->SetContent(Text);
		return Text;
	};
	M_NameText = AddText(RosterNameOffset);
	M_VariantText = AddText(RosterVariantOffset);
	M_CountText = AddText(RosterCountOffset);
}

void UW_SurvivalRosterEntry::BuildSupply(UOverlay* EntryOverlay)
{
	using namespace SurvivalUiLayout;
	UVerticalBox* const SupplyColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	SupplyColumn->SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlaySlot* const ColumnSlot = EntryOverlay->AddChildToOverlay(SupplyColumn);
	ColumnSlot->SetHorizontalAlignment(HAlign_Right);
	ColumnSlot->SetVerticalAlignment(VAlign_Center);
	ColumnSlot->SetPadding(RosterSupplyOffset);
	M_SupplyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_SupplyText->SetJustification(ETextJustify::Center);
	SupplyColumn->AddChildToVerticalBox(M_SupplyText)->SetHorizontalAlignment(HAlign_Center);
	M_SupplyLabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_SupplyLabelText->SetText(NSLOCTEXT("SurvivalArmySelection", "SupplyLabel", "SUPPLY"));
	SupplyColumn->AddChildToVerticalBox(M_SupplyLabelText)->SetHorizontalAlignment(HAlign_Center);
}

void UW_SurvivalRosterEntry::SetupEntry(UW_SurvivalArmySelection* Owner, const int32 RosterIndex,
                                        const FSurvivalRosterItem& Item, const USurvivalUiStyleAsset* StyleAsset)
{
	using namespace SurvivalUiLayout;
	M_Owner = Owner;
	M_RosterIndex = RosterIndex;
	M_StyleAsset = StyleAsset;
	M_MaxCopies = Item.Entry.MaxCopies;
	if (not GetIsValidBackground())
	{
		return;
	}
	ApplyTextStyles();
	const bool bHasIcon = IsValid(Item.Icon.GetResourceObject());
	if (bHasIcon)
	{
		M_Icon->SetBrush(Item.Icon);
		const float IconSize = RosterIconWellSize - RosterIconWellPadding.Left - RosterIconWellPadding.Right;
		M_Icon->SetDesiredSizeOverride(FVector2D(IconSize, IconSize));
	}
	M_Icon->SetVisibility(bHasIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	M_NameText->SetText(Item.Entry.DisplayName.ToUpper());
	M_VariantText->SetText(Item.VariantName.ToUpper());
	M_VariantText->SetVisibility(Item.VariantName.IsEmpty()
		                             ? ESlateVisibility::Collapsed
		                             : ESlateVisibility::HitTestInvisible);
	M_SupplyText->SetText(FText::AsNumber(Item.Entry.SupplyCost));
	SetToolTipText(Item.Tooltip);
	SetArmyState(0, true);
	ApplyBackgroundLook();
}

void UW_SurvivalRosterEntry::SetArmyState(const int32 CopiesInArmy, const bool bCanAdd)
{
	if (not IsValid(M_CountText))
	{
		return;
	}
	if (M_MaxCopies > 0)
	{
		M_CountText->SetText(FText::Format(NSLOCTEXT("SurvivalArmySelection", "CopiesOfMax", "IN ARMY {0} / {1}"),
		                                   FText::AsNumber(CopiesInArmy), FText::AsNumber(M_MaxCopies)));
	}
	else
	{
		M_CountText->SetText(CopiesInArmy > 0
			                     ? FText::Format(NSLOCTEXT("SurvivalArmySelection", "Copies", "IN ARMY {0}"),
			                                     FText::AsNumber(CopiesInArmy))
			                     : FText::GetEmpty());
	}
	SetRenderOpacity(bCanAdd ? 1.f : SurvivalUiLayout::RosterUnavailableOpacity);
}

FReply UW_SurvivalRosterEntry::NativeOnMouseButtonDown(const FGeometry& InGeometry,
                                                       const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UW_SurvivalRosterEntry::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry,
                                                              const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || not GetIsValidOwner())
	{
		return Super::NativeOnMouseButtonDoubleClick(InGeometry, InMouseEvent);
	}
	M_Owner->HandleRosterEntryActivated(M_RosterIndex);
	return FReply::Handled();
}

void UW_SurvivalRosterEntry::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent,
                                                  UDragDropOperation*& OutOperation)
{
	if (not GetIsValidOwner())
	{
		return;
	}
	OutOperation = M_Owner->CreateDragOperationFromRoster(M_RosterIndex);
}

void UW_SurvivalRosterEntry::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	bM_IsHovered = true;
	ApplyBackgroundLook();
}

void UW_SurvivalRosterEntry::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	bM_IsHovered = false;
	ApplyBackgroundLook();
}

void UW_SurvivalRosterEntry::ApplyBackgroundLook()
{
	if (not IsValid(M_Background))
	{
		return;
	}
	const FSurvivalUiStyle& Style = GetStyle();
	M_Background->SetBrush(bM_IsHovered ? Style.RosterEntry.Hovered : Style.RosterEntry.Normal);
	if (IsValid(M_IconWell))
	{
		M_IconWell->SetBrush(Style.RosterIconWell);
	}
}

void UW_SurvivalRosterEntry::ApplyTextStyles()
{
	using namespace SurvivalUiLayout;
	using namespace UnitVariantSelectionWidgets;
	const FSurvivalUiStyle& Style = GetStyle();
	ApplyTextOnMetal(M_NameText, MakeFont(Style.HeadingFont, RosterNameFontSize), Style.TextColor);
	ApplyTextOnMetal(M_VariantText, MakeFont(Style.HeadingFont, RosterVariantFontSize, LabelLetterSpacing),
	                 Style.AccentColor);
	ApplyTextOnMetal(M_CountText, MakeFont(Style.BodyFont, RosterCountFontSize), Style.MutedTextColor);
	ApplyTextOnMetal(M_SupplyText, MakeFont(Style.HeadingFont, RosterSupplyFontSize), Style.AccentColor);
	ApplyTextOnMetal(M_SupplyLabelText, MakeFont(Style.HeadingFont, RosterSupplyLabelFontSize, LabelLetterSpacing),
	                 Style.MutedTextColor);
}

const FSurvivalUiStyle& UW_SurvivalRosterEntry::GetStyle() const
{
	static const FSurvivalUiStyle DefaultStyle;
	return IsValid(M_StyleAsset) ? M_StyleAsset->Style : DefaultStyle;
}

bool UW_SurvivalRosterEntry::GetIsValidOwner() const
{
	if (M_Owner.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_Owner", "GetIsValidOwner", this);
	return false;
}

bool UW_SurvivalRosterEntry::GetIsValidBackground() const
{
	if (IsValid(M_Background))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_Background", "GetIsValidBackground", this);
	return false;
}
