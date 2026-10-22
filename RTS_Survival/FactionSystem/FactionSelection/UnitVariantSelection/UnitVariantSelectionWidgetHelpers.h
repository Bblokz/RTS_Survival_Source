// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/TextBlock.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/CoreStyle.h"

/** Small helpers the self-building unit variant selection widgets share. */
namespace UnitVariantSelectionWidgets
{
	const FVector2D TextShadowOffset(1.0f, 1.0f);
	const FLinearColor TextShadowColor(0.0f, 0.0f, 0.0f, 0.8f);

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
}
