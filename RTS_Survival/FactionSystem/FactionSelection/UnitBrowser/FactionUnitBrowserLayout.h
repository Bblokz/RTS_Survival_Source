// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"

/** Sizes the faction unit browser art is drawn for (UISkins/_generator/faction_unit_browser_spec.md), in UI units. */
namespace FactionUnitBrowserLayout
{
	// The whole band, anchored at the bottom centre of the menu canvas (spec section 4).
	constexpr float BrowserWidth = 1312.f;
	constexpr float BrowserHeight = 254.f;
	constexpr float BottomMargin = 8.f;

	// The panel's header strip; title, tabs and count sit in it.
	constexpr float HeaderHeight = 52.f;
	const FMargin TitleOffset(32.f, 0.f, 0.f, 0.f);
	constexpr int32 TitleFontSize = 16;
	constexpr int32 TitleLetterSpacing = 100;

	const FMargin TabRowOffset(224.f, 9.f, 0.f, 0.f);
	constexpr float TabWidth = 104.f;
	constexpr float TabHeight = 32.f;
	constexpr float TabSpacing = 4.f;
	// Slate centres the line box and capitals sit above its middle, so the box goes 1 unit low: the capitals then end
	// up 2 units above the tab centre, clear of the underline lamp, as drawn in the spec.
	const FMargin TabTextPadding(8.f, 2.f, 8.f, 0.f);
	constexpr int32 TabFontSize = 12;
	constexpr int32 TabLetterSpacing = 80;

	// The glass sheet behind the tile row: from under the header hairline down to just above the panel's lower
	// brackets, so the scroll bar sits on the glass too.
	const FMargin GlassOffset(14.f, 56.f, 0.f, 0.f);
	constexpr float GlassWidth = 1284.f;
	constexpr float GlassHeight = 192.f;

	// Ten tiles fill the row exactly, so there are never half tiles at rest.
	const FMargin TileRowOffset(20.f, 62.f, 0.f, 0.f);
	constexpr float TileRowWidth = 1272.f;
	constexpr float TileRowHeight = 180.f;
	const FMargin ScrollBarPadding(4.f, 8.f, 4.f, 0.f);
	constexpr float ScrollBarThickness = 8.f;

	constexpr float TileWidth = 120.f;
	constexpr float TileHeight = 164.f;
	constexpr float TileSpacing = 8.f;
	const FMargin IconWellOffset(8.f, 8.f, 0.f, 0.f);
	constexpr float IconWellSize = 104.f;
	const FMargin IconWellPadding(4.f);
	constexpr float IconSize = 96.f;
	// Name area (6,114)-(114,154): two lines of Bold 11 under the icon.
	const FMargin NameOffset(6.f, 114.f, 0.f, 0.f);
	constexpr float NameWidth = 108.f;
	constexpr float NameHeight = 40.f;
	constexpr int32 NameFontSize = 11;
}
