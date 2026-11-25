// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldMapUI/MapTags/W_WorldMapArrowLayer.h"

#include "Rendering/DrawElements.h"

namespace WorldMapArrowLayerPrivate
{
	// The 3 px dashes of the sketch at the 1920x1080 reference.
	constexpr float LineThickness = 2.f;

	void PaintStroke(FSlateWindowElementList& OutDrawElements, const int32 LayerId, const FGeometry& AllottedGeometry,
	                 const FVector2D& Start, const FVector2D& End, const FLinearColor& Color)
	{
		TArray<FVector2f> Points;
		Points.Reserve(2);
		Points.Add(FVector2f(static_cast<float>(Start.X), static_cast<float>(Start.Y)));
		Points.Add(FVector2f(static_cast<float>(End.X), static_cast<float>(End.Y)));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), MoveTemp(Points),
		                             ESlateDrawEffect::None, Color, true, LineThickness);
	}
}

void UW_WorldMapArrowLayer::SetArrows(TArray<FWorldMapArrowPaint>&& Arrows)
{
	M_Arrows = MoveTemp(Arrows);
}

void UW_WorldMapArrowLayer::ClearArrows()
{
	M_Arrows.Reset();
}

int32 UW_WorldMapArrowLayer::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
                                         const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
                                         const int32 LayerId, const FWidgetStyle& InWidgetStyle,
                                         const bool bParentEnabled) const
{
	using namespace WorldMapArrowLayerPrivate;
	const int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId,
	                                          InWidgetStyle, bParentEnabled);
	const int32 ArrowLayer = MaxLayer + 1;
	for (const FWorldMapArrowPaint& Arrow : M_Arrows)
	{
		for (const FWorldMapDashSegment& Dash : Arrow.Dashes)
		{
			PaintStroke(OutDrawElements, ArrowLayer, AllottedGeometry, Dash.Start, Dash.End, Arrow.Color);
		}
		if (not Arrow.bHasHead)
		{
			continue;
		}
		PaintStroke(OutDrawElements, ArrowLayer, AllottedGeometry, Arrow.HeadLeft, Arrow.HeadTip, Arrow.Color);
		PaintStroke(OutDrawElements, ArrowLayer, AllottedGeometry, Arrow.HeadRight, Arrow.HeadTip, Arrow.Color);
	}
	return ArrowLayer;
}
