// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_DeckVariantCard.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "DeckBuilderWidgetHelpers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "W_DeckBuilderMenu.h"

// Layout of the narrow card (118 x 268 units), from the approved B2 sketch.
namespace DeckVariantCardLayout
{
	const FVector2D PicturePosition(13.f, 21.f);
	const FVector2D PictureSize(66.f, 66.f);
	const FVector2D TypeBarPosition(9.f, 94.f);
	const FVector2D TypeBarSize(74.f, 15.f);
	const FVector2D NamePosition(6.f, 113.f);
	const FVector2D NameSize(80.f, 30.f);
	const FVector2D FitsLabelPosition(6.f, 183.f);
	const FVector2D FitsLabelSize(80.f, 12.f);
	const FVector2D UnitPicturePosition(27.f, 198.f);
	const FVector2D UnitPictureSize(38.f, 38.f);
	// The spine text is laid out left to right and then turned a quarter; this is its box before the turn, placed
	// so its centre sits on the centre of the painted stripe (x 98, y 148).
	const FVector2D SpineTextSize(212.f, 22.f);
	const FVector2D SpineTextPosition(-8.f, 137.f);
	constexpr float SpineTextAngle = 90.f;
	const FVector2D SpineTextPivot(0.5f, 0.5f);
	constexpr int32 TypeFontSize = 7;
	constexpr int32 NameFontSize = 8;
	constexpr int32 FitsFontSize = 6;
	constexpr int32 FitsLetterSpacing = 150;
	constexpr int32 SpineFontSize = 11;
	constexpr int32 SpineLetterSpacing = 110;
	const FLinearColor TypeBarColor(0.02f, 0.022f, 0.024f, 0.9f);
	// What follows the cursor during a drag is see-through, so the drop target stays visible.
	constexpr float DragVisualOpacity = 0.85f;
	constexpr int32 HitAreaZOrder = 0;
	constexpr int32 ArtZOrder = 1;
	constexpr int32 ContentZOrder = 2;
}

void UW_DeckVariantCard::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UW_DeckVariantCard::BuildWidgetTree()
{
	using namespace DeckVariantCardLayout;
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}
	USizeBox* const RootBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	RootBox->SetWidthOverride(DeckBuilderWidgets::VariantCardSize.X);
	RootBox->SetHeightOverride(DeckBuilderWidgets::VariantCardSize.Y);
	WidgetTree->RootWidget = RootBox;
	M_Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	RootBox->SetContent(M_Canvas);

	// Invisible but hit-testable, so the whole card takes the hover, the right-click and the drag.
	UBorder* const HitArea = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	HitArea->SetBrush(FSlateColorBrush(FLinearColor::Transparent));
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, HitArea, FVector2D::ZeroVector, DeckBuilderWidgets::VariantCardSize,
	                                  HitAreaZOrder);

	M_Frame = DeckBuilderWidgets::MakeImage(*WidgetTree, FSlateBrush());
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, M_Frame, FVector2D::ZeroVector, DeckBuilderWidgets::VariantCardSize,
	                                  ArtZOrder);
	M_Picture = DeckBuilderWidgets::MakeImage(*WidgetTree, FSlateBrush());
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, M_Picture, PicturePosition, PictureSize, ContentZOrder);

	M_TypeBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	M_TypeBar->SetBrush(FSlateColorBrush(TypeBarColor));
	M_TypeBar->SetPadding(FMargin(0.f));
	M_TypeBar->SetHorizontalAlignment(HAlign_Center);
	M_TypeBar->SetVerticalAlignment(VAlign_Center);
	M_TypeBar->SetVisibility(ESlateVisibility::HitTestInvisible);
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, M_TypeBar, TypeBarPosition, TypeBarSize, ContentZOrder);
	M_TypeText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_TypeText->SetText(NSLOCTEXT("DeckBuilder", "VariantCardType", "Variant"));
	M_TypeBar->SetContent(M_TypeText);

	M_NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_NameText->SetAutoWrapText(true);
	M_NameText->SetJustification(ETextJustify::Center);
	M_NameText->SetVisibility(ESlateVisibility::HitTestInvisible);
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, M_NameText, NamePosition, NameSize, ContentZOrder);

	M_FitsLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_FitsLabel->SetText(NSLOCTEXT("DeckBuilder", "VariantCardFits", "FITS"));
	M_FitsLabel->SetJustification(ETextJustify::Center);
	M_FitsLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, M_FitsLabel, FitsLabelPosition, FitsLabelSize, ContentZOrder);
	M_UnitPicture = DeckBuilderWidgets::MakeImage(*WidgetTree, FSlateBrush());
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, M_UnitPicture, UnitPicturePosition, UnitPictureSize, ContentZOrder);

	M_SpineText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_SpineText->SetVisibility(ESlateVisibility::HitTestInvisible);
	M_SpineText->SetRenderTransformPivot(SpineTextPivot);
	M_SpineText->SetRenderTransformAngle(SpineTextAngle);
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, M_SpineText, SpineTextPosition, SpineTextSize, ContentZOrder);
}

void UW_DeckVariantCard::SetupVariantCard(UW_DeckBuilderMenu* Menu, const FDeckBuilderLook& Look,
                                          const FDeckVariantCardView& View, const EDeckVariantCardPlace Place)
{
	using namespace DeckVariantCardLayout;
	M_Menu = Menu;
	M_View = View;
	M_Place = Place;
	if (not IsValid(M_Frame) || not IsValid(M_SpineText))
	{
		return;
	}
	const FDeckBuilderStyle& Style = Look.Style;
	M_Frame->SetBrush(Look.GetSpineFrame(View.Rarity));
	M_Picture->SetBrush(DeckBuilderWidgets::MakePictureBrush(View.Picture, PictureSize));
	M_Picture->SetVisibility(View.Picture.GetResourceObject() ? ESlateVisibility::HitTestInvisible
		                         : ESlateVisibility::Hidden);
	M_UnitPicture->SetBrush(DeckBuilderWidgets::MakePictureBrush(View.UnitPicture, UnitPictureSize));
	const bool bHasUnitPicture = View.UnitPicture.GetResourceObject() != nullptr;
	M_UnitPicture->SetVisibility(bHasUnitPicture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	M_FitsLabel->SetVisibility(bHasUnitPicture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);

	M_TypeText->SetFont(DeckBuilderWidgets::MakeFont(Look.BodyFont, TypeFontSize));
	M_TypeText->SetColorAndOpacity(Style.TextColor);
	M_NameText->SetText(View.Name);
	M_NameText->SetFont(DeckBuilderWidgets::MakeFont(Look.BodyFont, NameFontSize));
	M_NameText->SetColorAndOpacity(Style.TextColor);
	M_FitsLabel->SetFont(DeckBuilderWidgets::MakeFont(Look.BodyFont, FitsFontSize, FitsLetterSpacing));
	M_FitsLabel->SetColorAndOpacity(Style.LabelColor);

	// Stencilled lettering: dark ink on the light paints, light ink on the two dark ones. No shadow on paint.
	M_SpineText->SetText(View.Name.ToUpper());
	M_SpineText->SetFont(DeckBuilderWidgets::MakeFont(Look.HeadingFont, SpineFontSize, SpineLetterSpacing));
	M_SpineText->SetColorAndOpacity(FDeckBuilderLook::GetIsDarkPaint(View.Rarity) ? Style.SpineInkOnDarkPaint
		                                : Style.SpineInkOnLightPaint);
	if (Place == EDeckVariantCardPlace::Preview)
	{
		SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UW_DeckVariantCard::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	if (M_Place == EDeckVariantCardPlace::Preview || not GetIsValidMenu())
	{
		return;
	}
	M_Menu->HandleVariantHovered(M_View.Variant);
}

FReply UW_DeckVariantCard::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (M_Place == EDeckVariantCardPlace::Preview || not GetIsValidMenu())
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		M_Menu->HandleVariantRightClicked(M_View.Variant, M_Place == EDeckVariantCardPlace::Clipped);
		return FReply::Handled();
	}
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UW_DeckVariantCard::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent,
                                              UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);
	if (not GetIsValidMenu())
	{
		return;
	}
	UDeckCardDragOperation* const DragOperation = NewObject<UDeckCardDragOperation>(this);
	DragOperation->bIsVariant = true;
	DragOperation->Variant = M_View.Variant;
	UW_DeckVariantCard* const DragVisual = CreateWidget<UW_DeckVariantCard>(this, GetClass());
	if (IsValid(DragVisual))
	{
		DragVisual->SetupVariantCard(M_Menu.Get(), M_Menu->GetLook(), M_View, EDeckVariantCardPlace::Preview);
		DragVisual->SetRenderOpacity(DeckVariantCardLayout::DragVisualOpacity);
		DragOperation->DefaultDragVisual = DragVisual;
	}
	DragOperation->Pivot = EDragPivot::MouseDown;
	OutOperation = DragOperation;
}

bool UW_DeckVariantCard::GetIsValidMenu() const
{
	if (M_Menu.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, TEXT("M_Menu"), TEXT("GetIsValidMenu"), this);
	return false;
}
