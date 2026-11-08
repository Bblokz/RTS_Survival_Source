// Copyright (C) Bas Blokzijl - All rights reserved.

#include "MenuDesignFrame.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Layout/Geometry.h"

namespace MenuDesignFramePrivate
{
	// Owner size changes below this (slate units) are rounding, not a new screen.
	constexpr float ResizeTolerance = 0.5f;
}

UCanvasPanel* FMenuDesignFrame::BuildFrame(UWidgetTree& WidgetTree, UCanvasPanel& RootCanvas)
{
	using namespace MenuDesignFrameLayout;
	UScaleBox* const FrameScale = WidgetTree.ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
	// Only ever shrinks: a canvas wider than the screen is fitted, the canvas never grows past its units.
	FrameScale->SetStretch(EStretch::ScaleToFit);
	FrameScale->SetStretchDirection(EStretchDirection::DownOnly);
	FrameScale->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UCanvasPanelSlot* const FrameSlot = RootCanvas.AddChildToCanvas(FrameScale);
	FrameSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	FrameSlot->SetOffsets(FMargin(0.f));

	M_FrameBox = WidgetTree.ConstructWidget<USizeBox>(USizeBox::StaticClass());
	M_FrameBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	M_FrameBox->SetWidthOverride(DesignSize.X);
	M_FrameBox->SetHeightOverride(DesignSize.Y);
	FrameScale->SetContent(M_FrameBox);
	M_FrameSize = DesignSize;

	UCanvasPanel* const FrameCanvas = WidgetTree.ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	FrameCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	M_FrameBox->SetContent(FrameCanvas);
	return FrameCanvas;
}

void FMenuDesignFrame::UpdateFrame(const FGeometry& OwnerGeometry)
{
	using namespace MenuDesignFrameLayout;
	using namespace MenuDesignFramePrivate;
	const FVector2D OwnerSize = OwnerGeometry.GetLocalSize();
	if (not IsValid(M_FrameBox) || OwnerSize.X <= KINDA_SMALL_NUMBER || OwnerSize.Y <= KINDA_SMALL_NUMBER
		|| OwnerSize.Equals(M_FittedOwnerSize, ResizeTolerance))
	{
		return;
	}
	M_FittedOwnerSize = OwnerSize;
	// Below 16:9 the width runs out first: lay out in a canvas 1920 wide with the same aspect as the screen.
	const double FitScale = FMath::Min3(1.0, OwnerSize.X / DesignSize.X, OwnerSize.Y / DesignSize.Y);
	M_FrameSize = OwnerSize / FitScale;
	M_FrameBox->SetWidthOverride(M_FrameSize.X);
	M_FrameBox->SetHeightOverride(M_FrameSize.Y);
}
