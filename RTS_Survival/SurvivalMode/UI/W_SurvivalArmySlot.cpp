// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_SurvivalArmySlot.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantSelectionWidgetHelpers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "SurvivalArmyDragOperation.h"
#include "SurvivalArmyUiTypes.h"
#include "SurvivalUiLayout.h"
#include "SurvivalUiStyle.h"
#include "W_SurvivalArmySelection.h"

namespace SurvivalArmySlotPrivate
{
	const FLinearColor VariantBadgeColor(0.f, 0.f, 0.f, 0.65f);
	const FMargin VariantBadgePadding(4.f, 1.f, 4.f, 1.f);
	constexpr float DropHighlightOpacity = 0.28f;
	// The slot a unit is being dragged out of stays visible but faded, so the player sees where it came from.
	constexpr float DragSourceOpacity = 0.5f;
}

void UW_SurvivalArmySlot::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UW_SurvivalArmySlot::BuildWidgetTree()
{
	using namespace SurvivalUiLayout;
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}
	M_CellBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	M_CellBox->SetWidthOverride(SlotNativeWidth);
	M_CellBox->SetHeightOverride(SlotNativeHeight);
	WidgetTree->RootWidget = M_CellBox;

	// The slot is designed once at native size; the scale box fits it into whatever cell the grid computes.
	UScaleBox* const ScaleBox = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
	ScaleBox->SetStretch(EStretch::ScaleToFit);
	ScaleBox->SetStretchDirection(EStretchDirection::Both);
	M_CellBox->SetContent(ScaleBox);

	USizeBox* const NativeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	NativeBox->SetWidthOverride(SlotNativeWidth);
	NativeBox->SetHeightOverride(SlotNativeHeight);
	ScaleBox->SetContent(NativeBox);

	UOverlay* const SlotOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	NativeBox->SetContent(SlotOverlay);

	M_Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	M_Background->SetPadding(FMargin(0.f));
	UOverlaySlot* const BackgroundSlot = SlotOverlay->AddChildToOverlay(M_Background);
	BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
	BackgroundSlot->SetVerticalAlignment(VAlign_Fill);

	BuildIconWell(SlotOverlay);
	BuildBadges(SlotOverlay);
	BuildTexts(SlotOverlay);

	M_DropHighlight = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	M_DropHighlight->SetVisibility(ESlateVisibility::Collapsed);
	UOverlaySlot* const HighlightSlot = SlotOverlay->AddChildToOverlay(M_DropHighlight);
	HighlightSlot->SetHorizontalAlignment(HAlign_Fill);
	HighlightSlot->SetVerticalAlignment(VAlign_Fill);
}

void UW_SurvivalArmySlot::BuildIconWell(UOverlay* SlotOverlay)
{
	using namespace SurvivalUiLayout;
	USizeBox* const WellBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	WellBox->SetWidthOverride(SlotIconWellSize);
	WellBox->SetHeightOverride(SlotIconWellSize);
	WellBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlaySlot* const WellSlot = SlotOverlay->AddChildToOverlay(WellBox);
	WellSlot->SetHorizontalAlignment(HAlign_Left);
	WellSlot->SetVerticalAlignment(VAlign_Top);
	WellSlot->SetPadding(SlotIconWellOffset);

	M_IconWell = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	M_IconWell->SetPadding(SlotIconWellPadding);
	WellBox->SetContent(M_IconWell);
	M_Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	M_Icon->SetDesiredSizeOverride(FVector2D(SlotIconSize, SlotIconSize));
	M_Icon->SetVisibility(ESlateVisibility::Hidden);
	M_IconWell->SetContent(M_Icon);

	// Battle wear over frame and picture, like the faction unit tiles.
	M_WearImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	M_WearImage->SetVisibility(ESlateVisibility::Collapsed);
	UOverlaySlot* const WearSlot = SlotOverlay->AddChildToOverlay(M_WearImage);
	WearSlot->SetHorizontalAlignment(HAlign_Fill);
	WearSlot->SetVerticalAlignment(VAlign_Fill);
}

void UW_SurvivalArmySlot::BuildBadges(UOverlay* SlotOverlay)
{
	using namespace SurvivalUiLayout;
	using namespace SurvivalArmySlotPrivate;
	USizeBox* const SupplyBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SupplyBox->SetWidthOverride(SlotBadgeSize);
	SupplyBox->SetHeightOverride(SlotBadgeSize);
	SupplyBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlaySlot* const SupplySlot = SlotOverlay->AddChildToOverlay(SupplyBox);
	SupplySlot->SetHorizontalAlignment(HAlign_Left);
	SupplySlot->SetVerticalAlignment(VAlign_Top);
	SupplySlot->SetPadding(SlotSupplyBadgeOffset);
	M_SupplyBadge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	M_SupplyBadge->SetPadding(FMargin(0.f));
	M_SupplyBadge->SetHorizontalAlignment(HAlign_Center);
	M_SupplyBadge->SetVerticalAlignment(VAlign_Center);
	M_SupplyBadge->SetVisibility(ESlateVisibility::Collapsed);
	SupplyBox->SetContent(M_SupplyBadge);
	M_SupplyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_SupplyText->SetJustification(ETextJustify::Center);
	M_SupplyBadge->SetContent(M_SupplyText);

	USizeBox* const VariantBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	VariantBox->SetMaxDesiredWidth(SlotVariantBadgeMaxWidth);
	VariantBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlaySlot* const VariantSlot = SlotOverlay->AddChildToOverlay(VariantBox);
	VariantSlot->SetHorizontalAlignment(HAlign_Left);
	VariantSlot->SetVerticalAlignment(VAlign_Top);
	VariantSlot->SetPadding(SlotVariantBadgeOffset);
	M_VariantBadge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	M_VariantBadge->SetBrushColor(VariantBadgeColor);
	M_VariantBadge->SetPadding(VariantBadgePadding);
	M_VariantBadge->SetVisibility(ESlateVisibility::Collapsed);
	VariantBox->SetContent(M_VariantBadge);
	M_VariantText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_VariantText->SetClipping(EWidgetClipping::ClipToBounds);
	M_VariantBadge->SetContent(M_VariantText);
}

void UW_SurvivalArmySlot::BuildTexts(UOverlay* SlotOverlay)
{
	using namespace SurvivalUiLayout;
	USizeBox* const NameBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	NameBox->SetWidthOverride(SlotNameWidth);
	NameBox->SetHeightOverride(SlotNameHeight);
	NameBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlaySlot* const NameSlot = SlotOverlay->AddChildToOverlay(NameBox);
	NameSlot->SetHorizontalAlignment(HAlign_Left);
	NameSlot->SetVerticalAlignment(VAlign_Top);
	NameSlot->SetPadding(SlotNameOffset);
	M_NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_NameText->SetJustification(ETextJustify::Center);
	// A fixed wrap width: auto wrap measures a frame late (same fix as the faction unit tile).
	M_NameText->SetWrapTextAt(SlotNameWidth);
	M_NameText->SetClipping(EWidgetClipping::ClipToBounds);
	NameBox->SetContent(M_NameText);
	if (USizeBoxSlot* const TextSlot = Cast<USizeBoxSlot>(M_NameText->Slot))
	{
		TextSlot->SetHorizontalAlignment(HAlign_Fill);
		TextSlot->SetVerticalAlignment(VAlign_Center);
	}

	M_EmptyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_EmptyText->SetText(FText::FromString(TEXT("+")));
	M_EmptyText->SetJustification(ETextJustify::Center);
	M_EmptyText->SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlaySlot* const EmptySlot = SlotOverlay->AddChildToOverlay(M_EmptyText);
	EmptySlot->SetHorizontalAlignment(HAlign_Center);
	EmptySlot->SetVerticalAlignment(VAlign_Center);

	M_HqText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_HqText->SetText(NSLOCTEXT("SurvivalArmySelection", "HqLocked", "HQ"));
	M_HqText->SetVisibility(ESlateVisibility::Collapsed);
	UOverlaySlot* const HqSlot = SlotOverlay->AddChildToOverlay(M_HqText);
	HqSlot->SetHorizontalAlignment(HAlign_Left);
	HqSlot->SetVerticalAlignment(VAlign_Top);
	HqSlot->SetPadding(SlotIconWellOffset + FMargin(6.f, 4.f, 0.f, 0.f));
}

void UW_SurvivalArmySlot::SetupSlot(UW_SurvivalArmySelection* Owner, const int32 SlotIndex,
                                    const USurvivalUiStyleAsset* StyleAsset)
{
	M_Owner = Owner;
	M_SlotIndex = SlotIndex;
	M_StyleAsset = StyleAsset;
	ApplyTextStyles();
	ApplyBackgroundLook();
}

void UW_SurvivalArmySlot::SetSlotContent(const FSurvivalRosterItem* Item, const bool bIsLocked)
{
	if (not GetIsValidBackground())
	{
		return;
	}
	bM_IsFilled = Item != nullptr;
	bM_IsLocked = bIsLocked and bM_IsFilled;
	// The owner refreshes every slot after each drop, which also restores a faded drag source.
	SetRenderOpacity(1.f);
	const bool bHasIcon = bM_IsFilled and IsValid(Item->Icon.GetResourceObject());
	if (bHasIcon)
	{
		M_Icon->SetBrush(Item->Icon);
		M_Icon->SetDesiredSizeOverride(FVector2D(SurvivalUiLayout::SlotIconSize, SurvivalUiLayout::SlotIconSize));
	}
	M_Icon->SetVisibility(bHasIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	M_NameText->SetText(bM_IsFilled ? Item->Entry.DisplayName.ToUpper() : FText::GetEmpty());
	M_SupplyText->SetText(bM_IsFilled ? FText::AsNumber(Item->Entry.SupplyCost) : FText::GetEmpty());
	M_SupplyBadge->SetVisibility(bM_IsFilled and not bM_IsLocked
		                             ? ESlateVisibility::HitTestInvisible
		                             : ESlateVisibility::Collapsed);
	const bool bHasVariant = bM_IsFilled and not Item->VariantName.IsEmpty();
	M_VariantText->SetText(bHasVariant ? Item->VariantName.ToUpper() : FText::GetEmpty());
	M_VariantBadge->SetVisibility(bHasVariant ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	M_EmptyText->SetVisibility(bM_IsFilled ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	M_HqText->SetVisibility(bM_IsLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	SetToolTipText(bM_IsFilled ? Item->Tooltip : FText::GetEmpty());
	if (bM_IsFilled)
	{
		ApplyWear(Item->WearSeed);
	}
	else if (IsValid(M_WearImage))
	{
		M_WearImage->SetVisibility(ESlateVisibility::Collapsed);
	}
	ApplyNameVisibility();
	ApplyBackgroundLook();
}

void UW_SurvivalArmySlot::SetCellWidth(const float CellWidth)
{
	using namespace SurvivalUiLayout;
	if (not IsValid(M_CellBox))
	{
		return;
	}
	M_CellBox->SetWidthOverride(CellWidth);
	M_CellBox->SetHeightOverride(FMath::FloorToFloat(CellWidth * SlotAspect));
	bM_HideName = CellWidth / SlotNativeWidth < SlotNameHideScale;
	ApplyNameVisibility();
}

void UW_SurvivalArmySlot::SetDropPreview(const ESurvivalSlotDropPreview Preview)
{
	using namespace SurvivalArmySlotPrivate;
	if (not IsValid(M_DropHighlight))
	{
		return;
	}
	if (Preview == ESurvivalSlotDropPreview::None)
	{
		M_DropHighlight->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	const FSurvivalUiStyle& Style = GetStyle();
	FLinearColor Tint = (Preview == ESurvivalSlotDropPreview::Accept ? Style.AcceptColor : Style.WarningColor)
		.GetSpecifiedColor();
	Tint.A = DropHighlightOpacity;
	M_DropHighlight->SetBrushColor(Tint);
	M_DropHighlight->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UW_SurvivalArmySlot::SetIsDragVisual()
{
	M_Owner.Reset();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

FReply UW_SurvivalArmySlot::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (not GetCanDragOut())
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		if (GetIsValidOwner())
		{
			M_Owner->HandleSlotRemoveRequested(M_SlotIndex);
		}
		return FReply::Handled();
	}
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UW_SurvivalArmySlot::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent,
                                               UDragDropOperation*& OutOperation)
{
	if (not GetCanDragOut() || not GetIsValidOwner())
	{
		return;
	}
	OutOperation = M_Owner->CreateDragOperationFromSlot(M_SlotIndex);
	if (IsValid(OutOperation))
	{
		SetRenderOpacity(SurvivalArmySlotPrivate::DragSourceOpacity);
	}
}

bool UW_SurvivalArmySlot::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
                                       UDragDropOperation* InOperation)
{
	SetDropPreview(ESurvivalSlotDropPreview::None);
	const USurvivalArmyDragOperation* const ArmyOperation = Cast<USurvivalArmyDragOperation>(InOperation);
	if (not ArmyOperation || not GetIsValidOwner())
	{
		return false;
	}
	// Handled even when refused: the owner gave feedback, and a refused move must not count as a drag-out removal.
	M_Owner->HandleDropOnSlot(ArmyOperation, M_SlotIndex);
	return true;
}

void UW_SurvivalArmySlot::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
                                            UDragDropOperation* InOperation)
{
	const USurvivalArmyDragOperation* const ArmyOperation = Cast<USurvivalArmyDragOperation>(InOperation);
	if (not ArmyOperation || not M_Owner.IsValid())
	{
		return;
	}
	SetDropPreview(M_Owner->GetDropPreview(ArmyOperation, M_SlotIndex));
}

void UW_SurvivalArmySlot::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	SetDropPreview(ESurvivalSlotDropPreview::None);
}

void UW_SurvivalArmySlot::NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent,
                                                UDragDropOperation* InOperation)
{
	SetRenderOpacity(1.f);
	const USurvivalArmyDragOperation* const ArmyOperation = Cast<USurvivalArmyDragOperation>(InOperation);
	if (not ArmyOperation || ArmyOperation->SourceSlotIndex != M_SlotIndex || not GetIsValidOwner())
	{
		return;
	}
	// Dropped outside every slot (on the roster, the background, anywhere): the unit leaves the army.
	M_Owner->HandleSlotRemoveRequested(M_SlotIndex);
}

void UW_SurvivalArmySlot::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	bM_IsHovered = true;
	ApplyBackgroundLook();
}

void UW_SurvivalArmySlot::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	bM_IsHovered = false;
	ApplyBackgroundLook();
}

void UW_SurvivalArmySlot::ApplyBackgroundLook()
{
	if (not IsValid(M_Background))
	{
		return;
	}
	const FSurvivalUiStyle& Style = GetStyle();
	if (bM_IsLocked)
	{
		M_Background->SetBrush(Style.SlotLocked);
	}
	else
	{
		const FButtonStyle& ButtonLook = bM_IsFilled ? Style.SlotFilled : Style.SlotEmpty;
		M_Background->SetBrush(bM_IsHovered ? ButtonLook.Hovered : ButtonLook.Normal);
	}
	if (IsValid(M_IconWell))
	{
		M_IconWell->SetBrush(Style.SlotIconWell);
		M_IconWell->SetRenderOpacity(bM_IsFilled ? 1.f : SurvivalUiLayout::RosterUnavailableOpacity);
	}
	if (IsValid(M_SupplyBadge))
	{
		M_SupplyBadge->SetBrush(Style.Badge);
	}
}

void UW_SurvivalArmySlot::ApplyTextStyles()
{
	using namespace SurvivalUiLayout;
	using namespace UnitVariantSelectionWidgets;
	if (not IsValid(M_NameText))
	{
		return;
	}
	const FSurvivalUiStyle& Style = GetStyle();
	ApplyTextOnMetal(M_NameText, MakeFont(Style.HeadingFont, SlotNameFontSize), Style.TextColor);
	ApplyTextOnMetal(M_SupplyText, MakeFont(Style.HeadingFont, SlotBadgeFontSize), Style.AccentColor);
	ApplyTextOnMetal(M_VariantText, MakeFont(Style.HeadingFont, SlotVariantFontSize), Style.AccentColor);
	ApplyTextOnMetal(M_EmptyText, MakeFont(Style.HeadingFont, SlotEmptyFontSize), Style.MutedTextColor);
	ApplyTextOnMetal(M_HqText, MakeFont(Style.HeadingFont, SlotHqFontSize, LabelLetterSpacing), Style.AccentColor);
}

void UW_SurvivalArmySlot::ApplyWear(const float WearSeed)
{
	using namespace SurvivalUiLayout;
	if (not IsValid(M_WearImage))
	{
		return;
	}
	const FSurvivalUiStyle& Style = GetStyle();
	UMaterialInterface* const WearSource = Cast<UMaterialInterface>(Style.SlotWear.GetResourceObject());
	if (not IsValid(WearSource))
	{
		// Wear is optional decoration; no error.
		M_WearImage->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	if (not IsValid(M_WearMaterial))
	{
		M_WearMaterial = UMaterialInstanceDynamic::Create(WearSource, this);
	}
	if (not IsValid(M_WearMaterial))
	{
		return;
	}
	const FName SeedParameter(TEXT("Seed"));
	M_WearMaterial->SetScalarParameterValue(SeedParameter, WearSeed);
	FSlateBrush WearBrush = Style.SlotWear;
	WearBrush.SetResourceObject(M_WearMaterial);
	WearBrush.ImageSize = FVector2D(SlotNativeWidth, SlotNativeHeight);
	M_WearImage->SetBrush(WearBrush);
	M_WearImage->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UW_SurvivalArmySlot::ApplyNameVisibility()
{
	if (not IsValid(M_NameText))
	{
		return;
	}
	const bool bShowName = bM_IsFilled and not bM_HideName;
	M_NameText->SetVisibility(bShowName ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

const FSurvivalUiStyle& UW_SurvivalArmySlot::GetStyle() const
{
	static const FSurvivalUiStyle DefaultStyle;
	return IsValid(M_StyleAsset) ? M_StyleAsset->Style : DefaultStyle;
}

bool UW_SurvivalArmySlot::GetIsValidOwner() const
{
	if (M_Owner.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_Owner", "GetIsValidOwner", this);
	return false;
}

bool UW_SurvivalArmySlot::GetIsValidBackground() const
{
	if (IsValid(M_Background))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_Background", "GetIsValidBackground", this);
	return false;
}
