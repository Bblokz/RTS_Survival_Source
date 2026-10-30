// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"

/**
 * Sizes the survival mission UI is drawn for, in UI units of the 1920x1080 reference (UMG DPI scaling maps 1280x720
 * onto the same units). The Blueprint widget trees in Docs/ui_specs use the same numbers.
 */
namespace SurvivalUiLayout
{
	// ------------------------------------------------------------------ Army slot (native design, scaled to the cell)
	// Every slot is laid out at this size and a scale box fits it into the computed grid cell, so text, icon and
	// badges keep their proportions from 8 to 60 slots.
	constexpr float SlotNativeWidth = 120.f;
	constexpr float SlotNativeHeight = 164.f;
	constexpr float SlotAspect = SlotNativeHeight / SlotNativeWidth;
	const FMargin SlotIconWellOffset(8.f, 8.f, 0.f, 0.f);
	constexpr float SlotIconWellSize = 104.f;
	const FMargin SlotIconWellPadding(4.f);
	constexpr float SlotIconSize = 96.f;
	const FMargin SlotNameOffset(6.f, 116.f, 0.f, 0.f);
	constexpr float SlotNameWidth = 108.f;
	constexpr float SlotNameHeight = 40.f;
	constexpr int32 SlotNameFontSize = 11;
	constexpr float SlotBadgeSize = 28.f;
	const FMargin SlotSupplyBadgeOffset(84.f, 10.f, 0.f, 0.f);
	constexpr int32 SlotBadgeFontSize = 12;
	const FMargin SlotVariantBadgeOffset(12.f, 90.f, 0.f, 0.f);
	constexpr float SlotVariantBadgeMaxWidth = 96.f;
	constexpr int32 SlotVariantFontSize = 9;
	constexpr int32 SlotEmptyFontSize = 28;
	constexpr int32 SlotHqFontSize = 12;
	// Below this scale the unit name would be under ~8 px; it is collapsed and shown in the tooltip only.
	constexpr float SlotNameHideScale = 0.7f;

	// ------------------------------------------------------------------ Slot grid
	constexpr float SlotGap = 8.f;
	// Smallest cell before the grid stops shrinking and scrolls instead.
	constexpr float SlotMinCellWidth = 56.f;
	// Largest cell; 8 slots would otherwise blow the tiles up past the resolution of their art.
	constexpr float SlotMaxCellWidth = 180.f;
	// Room kept for the vertical scroll bar so the grid never has to shrink when the bar appears.
	constexpr float SlotScrollBarReserve = 14.f;
	// The slot area of the reference layout (Docs/ui_specs/W_SurvivalArmySelection.json); used at design time and
	// until the scroll box has been laid out once.
	constexpr float DefaultSlotAreaWidth = 1056.f;
	constexpr float DefaultSlotAreaHeight = 640.f;

	// ------------------------------------------------------------------ Roster
	constexpr float RosterEntryWidth = 440.f;
	constexpr float RosterEntryHeight = 80.f;
	constexpr float RosterEntrySpacing = 6.f;
	const FMargin RosterIconWellOffset(8.f, 4.f, 0.f, 0.f);
	constexpr float RosterIconWellSize = 72.f;
	const FMargin RosterIconWellPadding(3.f);
	const FMargin RosterNameOffset(92.f, 10.f, 0.f, 0.f);
	constexpr float RosterNameWidth = 250.f;
	constexpr int32 RosterNameFontSize = 14;
	const FMargin RosterVariantOffset(92.f, 34.f, 0.f, 0.f);
	constexpr int32 RosterVariantFontSize = 11;
	const FMargin RosterCountOffset(92.f, 54.f, 0.f, 0.f);
	constexpr int32 RosterCountFontSize = 10;
	const FMargin RosterSupplyOffset(0.f, 0.f, 20.f, 0.f);
	constexpr int32 RosterSupplyFontSize = 22;
	constexpr int32 RosterSupplyLabelFontSize = 9;
	// Entries the player cannot add right now (over budget or at max copies) are drawn at this opacity.
	constexpr float RosterUnavailableOpacity = 0.45f;

	constexpr float TabWidth = 84.f;
	constexpr float TabHeight = 32.f;
	constexpr float TabSpacing = 4.f;
	const FMargin TabTextPadding(4.f, 2.f, 4.f, 0.f);
	constexpr int32 TabFontSize = 11;
	constexpr int32 TabLetterSpacing = 60;

	// ------------------------------------------------------------------ Text sizes shared by the screens
	constexpr int32 TitleFontSize = 22;
	constexpr int32 TitleLetterSpacing = 120;
	constexpr int32 SubtitleFontSize = 13;
	constexpr int32 LabelFontSize = 14;
	constexpr int32 LabelLetterSpacing = 80;
	constexpr int32 BodyFontSize = 13;
	constexpr int32 ActionButtonFontSize = 18;
	constexpr int32 ActionButtonLetterSpacing = 100;
	constexpr int32 LocationNameFontSize = 26;
	constexpr int32 HudTimerFontSize = 34;
	constexpr int32 HudLabelFontSize = 12;
	constexpr int32 BannerTitleFontSize = 20;
	constexpr int32 ResultTitleFontSize = 34;
	constexpr int32 ResultValueFontSize = 20;

	// ------------------------------------------------------------------ Timings
	constexpr float FeedbackSeconds = 3.f;
	constexpr float HudRefreshSeconds = 0.25f;
	constexpr float DefaultBannerSeconds = 6.f;

	/** The cell size and column count the slot grid uses for a slot count in an area. */
	struct FSlotGridLayout
	{
		float CellWidth = SlotNativeWidth;
		float CellHeight = SlotNativeHeight;
		int32 Columns = 1;
		int32 Rows = 1;
		// True when even the smallest cell does not fit the area height; the grid then scrolls vertically.
		bool bNeedsScroll = false;
	};

	/**
	 * @brief Picks the column count that gives the largest cell while every slot stays visible, so 8, 16 and 40
	 * slots all fill the area; only past ~100 slots (reference area) does it fall back to minimum cells and a
	 * vertical scroll.
	 * @param SlotCount Total slots including the HQ slot.
	 * @param AreaWidth Width available to the grid, scroll bar reserve already removed.
	 * @param AreaHeight Height available to the grid.
	 * @return Cell size, columns and rows; cells keep the native slot aspect.
	 */
	inline FSlotGridLayout ComputeSlotGridLayout(const int32 SlotCount, const float AreaWidth, const float AreaHeight)
	{
		FSlotGridLayout Result;
		const int32 Count = FMath::Max(1, SlotCount);
		const float Width = FMath::Max(AreaWidth, SlotMinCellWidth);
		const float Height = FMath::Max(AreaHeight, SlotMinCellWidth * SlotAspect);
		float BestCellWidth = 0.f;
		int32 BestColumns = 1;
		for (int32 Columns = 1; Columns <= Count; ++Columns)
		{
			const int32 Rows = FMath::DivideAndRoundUp(Count, Columns);
			const float CellWidthByWidth = (Width - (Columns - 1) * SlotGap) / Columns;
			const float CellWidthByHeight = (Height - (Rows - 1) * SlotGap) / (Rows * SlotAspect);
			const float CellWidth = FMath::Min(CellWidthByWidth, CellWidthByHeight);
			if (CellWidth > BestCellWidth)
			{
				BestCellWidth = CellWidth;
				BestColumns = Columns;
			}
		}
		if (BestCellWidth < SlotMinCellWidth)
		{
			// Too many slots to show at once: minimum cells, as many columns as fit, scroll the rest.
			BestCellWidth = SlotMinCellWidth;
			BestColumns = FMath::Max(1, FMath::FloorToInt((Width + SlotGap) / (SlotMinCellWidth + SlotGap)));
			Result.bNeedsScroll = true;
		}
		Result.CellWidth = FMath::FloorToFloat(FMath::Min(BestCellWidth, SlotMaxCellWidth));
		Result.CellHeight = FMath::FloorToFloat(Result.CellWidth * SlotAspect);
		Result.Columns = FMath::Min(BestColumns, Count);
		Result.Rows = FMath::DivideAndRoundUp(Count, Result.Columns);
		return Result;
	}
}
