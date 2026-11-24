// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Rendering/RenderingCommon.h"
#include "W_TechWheelPaintLayer.generated.h"

/** A line through Points in the paint layer's local units; closed shapes repeat their first point at the end. */
struct FTechWheelPaintStroke
{
	TArray<FVector2f> Points;
	FLinearColor Color = FLinearColor::White;
	float Thickness = 1.f;
	// 0 draws a solid line, anything above draws dashes of that many pixels.
	float DashLength = 0.f;
};

/** A filled shape as a triangle list in the paint layer's local units. */
struct FTechWheelPaintFill
{
	TArray<FVector2f> Vertices;
	TArray<SlateIndex> Indices;
	FLinearColor Color = FLinearColor::White;
};

/** Fills, then strokes in the order they were added. */
struct FTechWheelPaintPass
{
	TArray<FTechWheelPaintFill> Fills;
	TArray<FTechWheelPaintStroke> Strokes;

	void Reset()
	{
		Fills.Reset();
		Strokes.Reset();
	}

	void AddStroke(TArray<FVector2f>&& Points, const FLinearColor& Color, const float Thickness,
	               const float DashLength = 0.f)
	{
		FTechWheelPaintStroke& Stroke = Strokes.AddDefaulted_GetRef();
		Stroke.Points = MoveTemp(Points);
		Stroke.Color = Color;
		Stroke.Thickness = Thickness;
		Stroke.DashLength = DashLength;
	}

	/** A convex polygon filled as a fan from its first point. */
	void AddConvexFill(const TArray<FVector2f>& Polygon, const FLinearColor& Color);
};

/** Everything one paint layer draws: the back pass, then the front pass over it. */
struct FTechWheelPaintList
{
	FTechWheelPaintPass Back;
	FTechWheelPaintPass Front;

	void Reset()
	{
		Back.Reset();
		Front.Reset();
	}
};

/**
 * @brief A childless widget that only paints a precomputed list of lines and filled shapes. The tech wheel places one
 * under its node widgets (wedges, paths, outlines, pips) and one over them (key badge marks), because a user widget's
 * own NativePaint draws on top of its children. Never takes the mouse.
 */
UCLASS()
class RTS_SURVIVAL_API UW_TechWheelPaintLayer : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Replaces what is drawn and asks Slate to paint again. */
	void SetPaintList(FTechWheelPaintList&& PaintList);

protected:
	virtual void NativeOnInitialized() override;

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	                          const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	                          int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	/** @return The highest layer used. */
	int32 PaintPass(const FTechWheelPaintPass& Pass, const FGeometry& AllottedGeometry,
	                FSlateWindowElementList& OutDrawElements, int32 LayerId, const FLinearColor& WidgetTint) const;
	void PaintFills(const TArray<FTechWheelPaintFill>& Fills, const FGeometry& AllottedGeometry,
	                FSlateWindowElementList& OutDrawElements, int32 LayerId, const FLinearColor& WidgetTint) const;
	void PaintStrokes(const TArray<FTechWheelPaintStroke>& Strokes, const FGeometry& AllottedGeometry,
	                  FSlateWindowElementList& OutDrawElements, int32 LayerId, const FLinearColor& WidgetTint) const;

	FTechWheelPaintList M_PaintList;
};
