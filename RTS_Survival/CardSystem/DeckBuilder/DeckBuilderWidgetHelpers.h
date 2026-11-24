// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "DeckBuilderStyle.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantSelectionButton.h"

/** Small helpers the self-building deck builder widgets share. */
namespace DeckBuilderWidgets
{
	const FVector2D TextShadowOffset(1.f, 1.f);
	const FLinearColor TextShadowColor(0.f, 0.f, 0.f, 0.8f);
	// The unit card art and the narrow variant card art, in UI units at the 1920x1080 reference.
	const FVector2D CardSize(240.f, 315.f);
	const FVector2D VariantCardSize(118.f, 268.f);
	// A unit card with a spine (or the lane where one goes) showing past its right edge.
	constexpr float CardWidthWithSpine = 262.f;

	inline FSlateFontInfo MakeFont(const FSlateFontInfo& BaseFont, const int32 Size, const int32 LetterSpacing = 0)
	{
		FSlateFontInfo Font = BaseFont;
		Font.Size = Size;
		Font.LetterSpacing = LetterSpacing;
		return Font;
	}

	/** A text block with the 1 px dark shadow every text on metal gets. */
	inline UTextBlock* MakeText(UWidgetTree& Tree, const FText& Text, const FSlateFontInfo& Font,
	                            const FSlateColor& Color)
	{
		UTextBlock* const TextBlock = Tree.ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		TextBlock->SetText(Text);
		TextBlock->SetFont(Font);
		TextBlock->SetColorAndOpacity(Color);
		TextBlock->SetShadowOffset(TextShadowOffset);
		TextBlock->SetShadowColorAndOpacity(TextShadowColor);
		TextBlock->SetVisibility(ESlateVisibility::HitTestInvisible);
		return TextBlock;
	}

	/** An image that never takes the mouse, so the card or row it sits on gets the hover, click and drag. */
	inline UImage* MakeImage(UWidgetTree& Tree, const FSlateBrush& Brush)
	{
		UImage* const Image = Tree.ConstructWidget<UImage>(UImage::StaticClass());
		Image->SetBrush(Brush);
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Image;
	}

	inline UCanvasPanelSlot* PlaceOnCanvas(UCanvasPanel& Canvas, UWidget* Widget, const FVector2D& Position,
	                                       const FVector2D& Size, const int32 ZOrder = 0)
	{
		UCanvasPanelSlot* const CanvasSlot = Canvas.AddChildToCanvas(Widget);
		CanvasSlot->SetAnchors(FAnchors(0.f, 0.f));
		CanvasSlot->SetPosition(Position);
		CanvasSlot->SetSize(Size);
		CanvasSlot->SetZOrder(ZOrder);
		return CanvasSlot;
	}

	/** Puts Content in the size box, aligned to its top left instead of stretched over it. */
	inline void SetTopLeftContent(USizeBox& SizeBox, UWidget* Content)
	{
		if (USizeBoxSlot* const ContentSlot = Cast<USizeBoxSlot>(SizeBox.SetContent(Content)))
		{
			ContentSlot->SetHorizontalAlignment(HAlign_Left);
			ContentSlot->SetVerticalAlignment(VAlign_Top);
		}
	}

	/** A picture brush drawn at exactly the size of the box it is placed in. */
	inline FSlateBrush MakePictureBrush(const FSlateBrush& Source, const FVector2D& DrawSize)
	{
		FSlateBrush Brush = Source;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = DrawSize;
		return Brush;
	}

	/**
	 * @brief A skin button with a label; it never takes keyboard focus, like the hangar screen's buttons.
	 * @param Tree The widget tree of the widget that owns the button.
	 * @param Label The button text.
	 * @param ButtonStyle The button art.
	 * @param Font Font of the label, already at its size.
	 * @param TextColor Colour of the label.
	 * @param ButtonSize Drawn size.
	 * @param OutButton Receives the button, to bind its click.
	 * @param OutLabel Receives the label, to change its text or colour later.
	 * @return The size box to place.
	 */
	inline UWidget* BuildTextButton(UWidgetTree& Tree, const FText& Label, const FButtonStyle& ButtonStyle,
	                                const FSlateFontInfo& Font, const FSlateColor& TextColor,
	                                const FVector2D& ButtonSize, TObjectPtr<UButton>& OutButton,
	                                TObjectPtr<UTextBlock>& OutLabel)
	{
		USizeBox* const ButtonBox = Tree.ConstructWidget<USizeBox>(USizeBox::StaticClass());
		ButtonBox->SetWidthOverride(ButtonSize.X);
		ButtonBox->SetHeightOverride(ButtonSize.Y);
		OutButton = Tree.ConstructWidget<UUnitVariantSelectionButton>(UUnitVariantSelectionButton::StaticClass());
		OutButton->SetStyle(ButtonStyle);
		ButtonBox->SetContent(OutButton);
		OutLabel = MakeText(Tree, Label, Font, TextColor);
		if (UButtonSlot* const LabelSlot = Cast<UButtonSlot>(OutButton->SetContent(OutLabel)))
		{
			LabelSlot->SetPadding(FMargin(0.f));
			LabelSlot->SetHorizontalAlignment(HAlign_Center);
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}
		return ButtonBox;
	}
}
