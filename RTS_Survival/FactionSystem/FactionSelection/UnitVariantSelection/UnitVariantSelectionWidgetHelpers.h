// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/CoreStyle.h"
#include "UnitVariantSelectionButton.h"
#include "UnitVariantSelectionStyle.h"

/** Small helpers the self-building unit variant selection widgets share. */
namespace UnitVariantSelectionWidgets
{
	const FVector2D TextShadowOffset(1.0f, 1.0f);
	const FLinearColor TextShadowColor(0.0f, 0.0f, 0.0f, 0.8f);
	// Text sits 2 units above centre, clear of the lamp strip of the button art (variant_selection_spec.md).
	const FMargin ButtonTextPadding(36.f, 4.f, 36.f, 10.f);
	constexpr int32 ButtonFontSize = 18;
	constexpr int32 ButtonLetterSpacing = 100;

	/**
	 * @brief The style's font face at the size and letter spacing the art was laid out for.
	 * @param StyleFont Font object and typeface from the Blueprint; the engine default when left empty.
	 * @param Size Whole-number size from variant_selection_spec.md.
	 * @param LetterSpacing In 1/1000 em, as UMG stores it.
	 * @return The font to put on a text block.
	 */
	inline FSlateFontInfo MakeFont(const FSlateFontInfo& StyleFont, const int32 Size, const int32 LetterSpacing = 0)
	{
		FSlateFontInfo Font = StyleFont.HasValidFont() ? StyleFont : FCoreStyle::GetDefaultFontStyle("Bold", Size);
		Font.Size = Size;
		Font.LetterSpacing = LetterSpacing;
		return Font;
	}

	/** The 1 px dark shadow every text on metal gets (Docs/UI/Skins/ShippingReadyUI.md, section 9). */
	inline void ApplyTextOnMetal(UTextBlock* TextBlock, const FSlateFontInfo& Font, const FSlateColor& Color)
	{
		TextBlock->SetFont(Font);
		TextBlock->SetColorAndOpacity(Color);
		TextBlock->SetShadowOffset(TextShadowOffset);
		TextBlock->SetShadowColorAndOpacity(TextShadowColor);
	}

	/**
	 * @brief A labelled skin button; it never takes keyboard focus, so the screen keeps the keys for navigation.
	 * @param Tree The widget tree of the widget that owns the button.
	 * @param Style Font and text colour of the label.
	 * @param Label The button text.
	 * @param ButtonStyle One of the button styles of Style.
	 * @param ButtonSize Drawn size its instances were made for.
	 * @param OutButton Receives the button, to bind its click.
	 * @return The size box to place.
	 */
	inline UWidget* BuildTextButton(UWidgetTree& Tree, const FUnitVariantSelectionStyle& Style, const FText& Label,
	                                const FButtonStyle& ButtonStyle, const FVector2D& ButtonSize,
	                                TObjectPtr<UButton>& OutButton)
	{
		USizeBox* const ButtonBox = Tree.ConstructWidget<USizeBox>(USizeBox::StaticClass());
		ButtonBox->SetWidthOverride(ButtonSize.X);
		ButtonBox->SetHeightOverride(ButtonSize.Y);
		OutButton = Tree.ConstructWidget<UUnitVariantSelectionButton>(UUnitVariantSelectionButton::StaticClass());
		OutButton->SetStyle(ButtonStyle);
		ButtonBox->SetContent(OutButton);
		UTextBlock* const LabelText = Tree.ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		LabelText->SetText(Label);
		LabelText->SetVisibility(ESlateVisibility::HitTestInvisible);
		ApplyTextOnMetal(LabelText, MakeFont(Style.HeadingFont, ButtonFontSize, ButtonLetterSpacing), Style.TextColor);
		if (UButtonSlot* const LabelSlot = Cast<UButtonSlot>(OutButton->SetContent(LabelText)))
		{
			LabelSlot->SetPadding(ButtonTextPadding);
			LabelSlot->SetHorizontalAlignment(HAlign_Center);
			LabelSlot->SetVerticalAlignment(VAlign_Center);
		}
		return ButtonBox;
	}
}
