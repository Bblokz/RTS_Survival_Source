// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SubtitleStyleDefaults.h"

#include "Styling/SlateColor.h"

FSlateBrush SubtitleStyleDefaults::MakePanelBrush()
{
	constexpr float CornerRadius = 3.0f;
	constexpr float OutlineWidth = 1.0f;
	const FLinearColor PanelColor(0.012f, 0.016f, 0.02f, 0.86f);
	const FLinearColor OutlineColor(0.32f, 0.78f, 0.86f, 0.55f);

	FSlateBrush Panel;
	Panel.DrawAs = ESlateBrushDrawType::RoundedBox;
	Panel.TintColor = FSlateColor(PanelColor);
	Panel.OutlineSettings = FSlateBrushOutlineSettings(
		FVector4(CornerRadius, CornerRadius, CornerRadius, CornerRadius), FSlateColor(OutlineColor), OutlineWidth);
	Panel.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	return Panel;
}
