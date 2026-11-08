// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "MenuDesignFrame.generated.h"

class UCanvasPanel;
class USizeBox;
class UWidgetTree;
struct FGeometry;

namespace MenuDesignFrameLayout
{
	// The size the pre-game menus are laid out for, in slate units at the project's DPI scale.
	const FVector2D DesignSize(1920.f, 1080.f);
}

/**
 * @brief Keeps a pre-game menu's 1920x1080 layout whole on screens narrower than 16:9 (4:3, 5:4, 16:10).
 * The engine scales UI by the screen height, so a 1920x1440 screen only has 1440x1080 units; the frame shrinks its canvas
 * to fit the width and hands it the extra height, so top, bottom and edge anchors still reach the screen edges.
 * The owning widget builds its widgets into BuildFrame's canvas and calls UpdateFrame from NativeTick.
 */
USTRUCT()
struct FMenuDesignFrame
{
	GENERATED_BODY()

	/**
	 * @brief Adds the frame filling RootCanvas; everything the menu builds into the returned canvas keeps its
	 * 1920x1080 sizes and paddings at any aspect ratio.
	 * @param WidgetTree Tree of the owning widget.
	 * @param RootCanvas Root canvas of the owning widget; it must fill the screen.
	 * @return The canvas to build the menu into; null when the frame could not be built.
	 */
	UCanvasPanel* BuildFrame(UWidgetTree& WidgetTree, UCanvasPanel& RootCanvas);

	/** Resizes the frame's canvas when the screen (the owner's geometry) changed size. */
	void UpdateFrame(const FGeometry& OwnerGeometry);

	/** The canvas size the menu is laid out in now; 1920 wide or wider, 1080 high or higher. */
	FVector2D GetFrameSize() const { return M_FrameSize; }

private:
	UPROPERTY()
	TObjectPtr<USizeBox> M_FrameBox;

	FVector2D M_FrameSize = MenuDesignFrameLayout::DesignSize;

	// The owner size the frame was last fitted to; the frame only resizes when it changes.
	FVector2D M_FittedOwnerSize = FVector2D::ZeroVector;
};
