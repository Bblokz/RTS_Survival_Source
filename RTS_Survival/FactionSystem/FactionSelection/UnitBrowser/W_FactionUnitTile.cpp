// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_FactionUnitTile.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "FactionUnitBrowserLayout.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantSelectionWidgetHelpers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "W_FactionUnitBrowser.h"

void UW_FactionUnitTile::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UW_FactionUnitTile::BuildWidgetTree()
{
	using namespace FactionUnitBrowserLayout;
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}
	USizeBox* const SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SizeBox->SetWidthOverride(TileWidth);
	SizeBox->SetHeightOverride(TileHeight);
	WidgetTree->RootWidget = SizeBox;

	M_Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	M_Button->OnClicked.AddDynamic(this, &UW_FactionUnitTile::OnTileClicked);
	SizeBox->SetContent(M_Button);

	UOverlay* const TileOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	// The button centres its content by default; the overlay must fill it so the spec offsets hold.
	if (UButtonSlot* const ContentSlot = Cast<UButtonSlot>(M_Button->SetContent(TileOverlay)))
	{
		ContentSlot->SetPadding(FMargin(0.f));
		ContentSlot->SetHorizontalAlignment(HAlign_Fill);
		ContentSlot->SetVerticalAlignment(VAlign_Fill);
	}
	BuildIcon(TileOverlay);
	BuildName(TileOverlay);
	BuildWear(TileOverlay);
}

void UW_FactionUnitTile::BuildIcon(UOverlay* TileOverlay)
{
	using namespace FactionUnitBrowserLayout;
	USizeBox* const WellBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	WellBox->SetWidthOverride(IconWellSize);
	WellBox->SetHeightOverride(IconWellSize);
	UOverlaySlot* const WellSlot = TileOverlay->AddChildToOverlay(WellBox);
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

void UW_FactionUnitTile::BuildName(UOverlay* TileOverlay)
{
	using namespace FactionUnitBrowserLayout;
	USizeBox* const NameBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	NameBox->SetWidthOverride(NameWidth);
	NameBox->SetHeightOverride(NameHeight);
	NameBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlaySlot* const NameSlot = TileOverlay->AddChildToOverlay(NameBox);
	NameSlot->SetHorizontalAlignment(HAlign_Left);
	NameSlot->SetVerticalAlignment(VAlign_Top);
	NameSlot->SetPadding(NameOffset);

	M_NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_NameText->SetJustification(ETextJustify::Center);
	// A fixed wrap width: auto wrap measures a frame late, which left long names on one clipped line.
	M_NameText->SetWrapTextAt(NameWidth);
	M_NameText->SetClipping(EWidgetClipping::ClipToBounds);
	NameBox->SetContent(M_NameText);
	if (USizeBoxSlot* const TextSlot = Cast<USizeBoxSlot>(M_NameText->Slot))
	{
		TextSlot->SetHorizontalAlignment(HAlign_Fill);
		TextSlot->SetVerticalAlignment(VAlign_Center);
	}
}

void UW_FactionUnitTile::BuildWear(UOverlay* TileOverlay)
{
	// On top of frame, picture and name; the material keeps the name band clean and the picture nearly so.
	M_WearImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	M_WearImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlaySlot* const WearSlot = TileOverlay->AddChildToOverlay(M_WearImage);
	WearSlot->SetHorizontalAlignment(HAlign_Fill);
	WearSlot->SetVerticalAlignment(VAlign_Fill);
}

void UW_FactionUnitTile::ApplyWear(const FSlateBrush& WearBrush, const float WearSeed)
{
	using namespace FactionUnitBrowserLayout;
	UMaterialInterface* const WearSource = Cast<UMaterialInterface>(WearBrush.GetResourceObject());
	if (not IsValid(WearSource) || not IsValid(M_WearImage))
	{
		RTSFunctionLibrary::ReportError(TEXT("Faction unit tile needs the TileWear material brush in its style."));
		return;
	}
	M_WearMaterial = UMaterialInstanceDynamic::Create(WearSource, this);
	if (not IsValid(M_WearMaterial))
	{
		return;
	}
	const FName SeedParameter(TEXT("Seed"));
	M_WearMaterial->SetScalarParameterValue(SeedParameter, WearSeed);
	FSlateBrush TileWearBrush = WearBrush;
	TileWearBrush.SetResourceObject(M_WearMaterial);
	TileWearBrush.ImageSize = FVector2D(TileWidth, TileHeight);
	M_WearImage->SetBrush(TileWearBrush);
}

void UW_FactionUnitTile::SetupTile(UW_FactionUnitBrowser* Browser, const int32 TileIndex,
                                   const FFactionUnitBrowserStyle& Style, const FSlateBrush& UnitIcon,
                                   const FText& UnitName, const float WearSeed)
{
	using namespace FactionUnitBrowserLayout;
	M_Browser = Browser;
	M_TileIndex = TileIndex;
	M_Style = Style;
	if (not GetIsValidButton())
	{
		return;
	}
	// The image's brush is a UPROPERTY, so it keeps the icon texture loaded. An empty brush would draw Slate's
	// default white box, so the tile stays empty instead.
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
	// Names longer than two lines clip; the full name shows on hover.
	M_Button->SetToolTipText(UnitName);
	ApplyWear(Style.TileWear, WearSeed);
	ApplySelectionLook();
}

void UW_FactionUnitTile::SetIsSelected(const bool bIsSelected)
{
	bM_IsSelected = bIsSelected;
	ApplySelectionLook();
}

void UW_FactionUnitTile::SimulateClick()
{
	if (not GetIsValidButton())
	{
		return;
	}
	M_Button->OnClicked.Broadcast();
}

void UW_FactionUnitTile::ApplySelectionLook()
{
	if (not GetIsValidButton())
	{
		return;
	}
	M_Button->SetStyle(bM_IsSelected ? M_Style.TileSelected : M_Style.Tile);
	M_IconWell->SetBrush(bM_IsSelected ? M_Style.IconWellSelected : M_Style.IconWell);
	M_NameText->SetColorAndOpacity(bM_IsSelected ? M_Style.SelectedTextColor : M_Style.TextColor);
}

void UW_FactionUnitTile::OnTileClicked()
{
	if (not GetIsValidBrowser())
	{
		return;
	}
	M_Browser->HandleTileClicked(M_TileIndex);
}

bool UW_FactionUnitTile::GetIsValidBrowser() const
{
	if (M_Browser.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_Browser", "GetIsValidBrowser", this);
	return false;
}

bool UW_FactionUnitTile::GetIsValidButton() const
{
	if (IsValid(M_Button))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_Button", "GetIsValidButton", this);
	return false;
}
