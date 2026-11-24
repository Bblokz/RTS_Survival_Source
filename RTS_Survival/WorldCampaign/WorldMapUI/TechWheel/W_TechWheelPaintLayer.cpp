// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldMapUI/TechWheel/W_TechWheelPaintLayer.h"

#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

void FTechWheelPaintPass::AddConvexFill(const TArray<FVector2f>& Polygon, const FLinearColor& Color)
{
	constexpr int32 MinimumPolygonPoints = 3;
	if (Polygon.Num() < MinimumPolygonPoints)
	{
		return;
	}
	FTechWheelPaintFill& Fill = Fills.AddDefaulted_GetRef();
	Fill.Vertices = Polygon;
	Fill.Color = Color;
	for (int32 PointIndex = 1; PointIndex + 1 < Polygon.Num(); ++PointIndex)
	{
		Fill.Indices.Add(0);
		Fill.Indices.Add(static_cast<SlateIndex>(PointIndex));
		Fill.Indices.Add(static_cast<SlateIndex>(PointIndex + 1));
	}
}

void UW_TechWheelPaintLayer::SetPaintList(FTechWheelPaintList&& PaintList)
{
	M_PaintList = MoveTemp(PaintList);
	InvalidateLayoutAndVolatility();
}

void UW_TechWheelPaintLayer::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

int32 UW_TechWheelPaintLayer::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
                                          const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
                                          const int32 LayerId, const FWidgetStyle& InWidgetStyle,
                                          const bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId,
	                                           InWidgetStyle, bParentEnabled);
	const FLinearColor WidgetTint = InWidgetStyle.GetColorAndOpacityTint();
	const int32 BackLayer = PaintPass(M_PaintList.Back, AllottedGeometry, OutDrawElements, BaseLayer, WidgetTint);
	return PaintPass(M_PaintList.Front, AllottedGeometry, OutDrawElements, BackLayer + 1, WidgetTint);
}

int32 UW_TechWheelPaintLayer::PaintPass(const FTechWheelPaintPass& Pass, const FGeometry& AllottedGeometry,
                                        FSlateWindowElementList& OutDrawElements, const int32 LayerId,
                                        const FLinearColor& WidgetTint) const
{
	PaintFills(Pass.Fills, AllottedGeometry, OutDrawElements, LayerId, WidgetTint);
	const int32 StrokeLayer = LayerId + 1;
	PaintStrokes(Pass.Strokes, AllottedGeometry, OutDrawElements, StrokeLayer, WidgetTint);
	return StrokeLayer;
}

void UW_TechWheelPaintLayer::PaintFills(const TArray<FTechWheelPaintFill>& Fills, const FGeometry& AllottedGeometry,
                                        FSlateWindowElementList& OutDrawElements, const int32 LayerId,
                                        const FLinearColor& WidgetTint) const
{
	if (Fills.IsEmpty() || not FSlateApplication::IsInitialized())
	{
		return;
	}
	FSlateRenderer* const Renderer = FSlateApplication::Get().GetRenderer();
	const FSlateBrush* const WhiteBrush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	if (Renderer == nullptr || WhiteBrush == nullptr)
	{
		return;
	}

	// One draw call for every fill of the pass: the colour travels per vertex. Custom vertices are in window space,
	// so the local points go through the accumulated render transform.
	const FSlateRenderTransform& AccumulatedRenderTransform = AllottedGeometry.GetAccumulatedRenderTransform();
	TArray<FSlateVertex> Vertices;
	TArray<SlateIndex> Indices;
	for (const FTechWheelPaintFill& Fill : Fills)
	{
		const SlateIndex FirstVertex = static_cast<SlateIndex>(Vertices.Num());
		const FColor VertexColor = (Fill.Color * WidgetTint).ToFColorSRGB();
		for (const FVector2f& Vertex : Fill.Vertices)
		{
			Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
				AccumulatedRenderTransform, Vertex, FVector2f::ZeroVector, VertexColor));
		}
		for (const SlateIndex Index : Fill.Indices)
		{
			Indices.Add(FirstVertex + Index);
		}
	}
	const FSlateResourceHandle ResourceHandle = Renderer->GetResourceHandle(*WhiteBrush);
	FSlateDrawElement::MakeCustomVerts(OutDrawElements, LayerId, ResourceHandle, Vertices, Indices, nullptr, 0, 0);
}

void UW_TechWheelPaintLayer::PaintStrokes(const TArray<FTechWheelPaintStroke>& Strokes,
                                          const FGeometry& AllottedGeometry,
                                          FSlateWindowElementList& OutDrawElements, const int32 LayerId,
                                          const FLinearColor& WidgetTint) const
{
	const FPaintGeometry PaintGeometry = AllottedGeometry.ToPaintGeometry();
	for (const FTechWheelPaintStroke& Stroke : Strokes)
	{
		const FLinearColor StrokeColor = Stroke.Color * WidgetTint;
		if (Stroke.DashLength > 0.f)
		{
			FSlateDrawElement::MakeDashedLines(OutDrawElements, LayerId, PaintGeometry,
			                                   TArray<FVector2f>(Stroke.Points), ESlateDrawEffect::None, StrokeColor,
			                                   Stroke.Thickness, Stroke.DashLength);
			continue;
		}
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, PaintGeometry, Stroke.Points, ESlateDrawEffect::None,
		                             StrokeColor, true, Stroke.Thickness);
	}
}
