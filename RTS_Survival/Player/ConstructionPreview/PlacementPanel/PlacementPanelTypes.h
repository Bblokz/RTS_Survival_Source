// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"

/** Whether a click would place what the placement panel describes; colours the status line. */
enum class EPlacementPanelStatus : uint8
{
	// A click places it.
	Ready,
	// A click is refused; the status line says why.
	Blocked,
	// Neither: the status line tells the player what to do next (for instance where to click first).
	Info
};

/** One "label ... value" line of the placement panel, such as the rotation or the incline. */
struct FPlacementPanelStatRow
{
	FText Label;
	FText Value;
	// Draws the value in the warning colour: this number is why the placement is refused.
	bool bIsWarning = false;
};

/** One key hint of the placement panel: the key caps and what they do. */
struct FPlacementPanelKeyHint
{
	TArray<FText> Keys;
	FText Action;
};

/** Everything the placement panel shows; owners build it every tick and hand it to UW_PlacementPanel::SetContent. */
struct FPlacementPanelContent
{
	FText Title;
	FText StatusText;
	EPlacementPanelStatus Status = EPlacementPanelStatus::Info;
	TArray<FPlacementPanelStatRow> Stats;
	TArray<FPlacementPanelKeyHint> KeyHints;
};

/** Which side of the placed object's screen rectangle the panel sits on. */
enum class EPlacementPanelDock : uint8
{
	Right,
	Left,
	Below,
	Above
};

/** Input of PlacementPanelDocking::ChooseDock; all sizes in widget units (viewport pixels without DPI scale). */
struct FPlacementPanelDockRequest
{
	// Screen rectangle of what is being placed; the panel must stay off it.
	FBox2D AvoidRect = FBox2D(ForceInit);
	FVector2D PanelSize = FVector2D::ZeroVector;
	FVector2D ViewportSize = FVector2D::ZeroVector;
	// Screen bands the panel prefers to keep clear, such as the HUD's bottom panel. Given up only when the panel
	// cannot clear the avoided rectangle within them.
	FMargin SafeInsets;
	// Distance between the avoided rectangle and the panel.
	float Gap = 24.f;
	// Tried first so the panel does not hop between sides while the cursor moves (the side it is on now).
	EPlacementPanelDock PreferredDock = EPlacementPanelDock::Right;
};

struct FPlacementPanelDockResult
{
	FVector2D TopLeft = FVector2D::ZeroVector;
	EPlacementPanelDock Dock = EPlacementPanelDock::Right;
	// No side keeps the panel off the avoided rectangle (it fills the screen); the least covering spot was chosen.
	bool bOverlapsAvoidRect = false;
};

/** The pure layout maths of the placement panel, apart from the widget so the map tests can check it headless. */
namespace PlacementPanelDocking
{
	/**
	 * @brief Puts the panel beside the avoided rectangle: the preferred side, then right, left, below and above,
	 * each kept on screen. Falls back to the whole screen, then to the spot that covers the least of the rectangle.
	 * @param Request Rectangle to avoid, panel and viewport size, safe bands and the preferred side.
	 * @return Top left corner of the panel and the side it docks on.
	 */
	RTS_SURVIVAL_API FPlacementPanelDockResult ChooseDock(const FPlacementPanelDockRequest& Request);

	RTS_SURVIVAL_API FBox2D MakePanelRect(const FVector2D& TopLeft, const FVector2D& PanelSize);

	/** @return Whether the rectangles share area; touching edges do not count. */
	RTS_SURVIVAL_API bool GetDoRectsOverlap(const FBox2D& RectA, const FBox2D& RectB);
}
