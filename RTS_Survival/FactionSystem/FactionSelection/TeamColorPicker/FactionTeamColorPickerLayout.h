// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"

/** One team colour the player can pick for their vehicles. */
struct FFactionTeamColorOption
{
	FText Name;
	// Linear; alpha 0 keeps the vehicles' camo as authored.
	FLinearColor Color;
};

/** Sizes of the faction menu's team colour picker, in UI units; it sits under the faction buttons. */
namespace FactionTeamColorPickerLayout
{
	// Top left of the menu canvas, aligned with the faction panel above it.
	const FVector2D CanvasPosition(30.f, 368.f);
	constexpr float PanelWidth = 516.f;
	constexpr float PanelHeight = 132.f;

	constexpr float HeaderHeight = 52.f;
	const FMargin TitleOffset(32.f, 0.f, 0.f, 0.f);
	const FMargin ColorNameOffset(0.f, 0.f, 28.f, 0.f);
	constexpr int32 TitleFontSize = 16;
	constexpr int32 TitleLetterSpacing = 100;
	constexpr int32 ColorNameFontSize = 12;
	constexpr int32 ColorNameLetterSpacing = 60;

	const FMargin SwatchRowOffset(24.f, 62.f, 0.f, 0.f);
	constexpr float SwatchSize = 46.f;
	constexpr float SwatchSpacing = 6.f;
	// The colour chip inside the swatch frame.
	const FMargin ChipPadding(7.f);
	constexpr int32 DefaultLabelFontSize = 9;

	/** @return The colours in picker order; the first keeps the camo and is the default. */
	inline TArray<FFactionTeamColorOption> GetTeamColorOptions()
	{
		return {
			{NSLOCTEXT("FactionTeamColor", "Camo", "Camo"), FLinearColor::Transparent},
			{NSLOCTEXT("FactionTeamColor", "Red", "Red"), FLinearColor(0.80f, 0.02f, 0.02f, 1.f)},
			{NSLOCTEXT("FactionTeamColor", "Blue", "Blue"), FLinearColor(0.02f, 0.10f, 0.85f, 1.f)},
			{NSLOCTEXT("FactionTeamColor", "Green", "Green"), FLinearColor(0.05f, 0.50f, 0.03f, 1.f)},
			{NSLOCTEXT("FactionTeamColor", "Yellow", "Yellow"), FLinearColor(0.90f, 0.70f, 0.02f, 1.f)},
			{NSLOCTEXT("FactionTeamColor", "Orange", "Orange"), FLinearColor(0.90f, 0.25f, 0.01f, 1.f)},
			{NSLOCTEXT("FactionTeamColor", "Purple", "Purple"), FLinearColor(0.35f, 0.03f, 0.60f, 1.f)},
			{NSLOCTEXT("FactionTeamColor", "Teal", "Teal"), FLinearColor(0.02f, 0.50f, 0.50f, 1.f)},
			{NSLOCTEXT("FactionTeamColor", "White", "White"), FLinearColor(0.85f, 0.85f, 0.85f, 1.f)},
		};
	}
}
