// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/MapTags/WorldMapTagsMath.h"
#include "W_WorldMapArrowLayer.generated.h"

/** One dashed arrow from an enemy piece to where it is heading, already in the arrow layer's local space. */
struct RTS_SURVIVAL_API FWorldMapArrowPaint
{
	TArray<FWorldMapDashSegment> Dashes;

	// The two short strokes of the head meet at HeadTip; only drawn when the target itself is on screen.
	bool bHasHead = false;
	FVector2D HeadTip = FVector2D::ZeroVector;
	FVector2D HeadLeft = FVector2D::ZeroVector;
	FVector2D HeadRight = FVector2D::ZeroVector;

	FLinearColor Color = FLinearColor::White;
};

/**
 * @brief Paints the dashed arrows of UW_WorldMapTags. A separate layer under the tag plates, because a UUserWidget
 * paints its own lines after (on top of) its children. UW_WorldMapTags creates it and hands it new arrows every tick.
 */
UCLASS()
class RTS_SURVIVAL_API UW_WorldMapArrowLayer : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetArrows(TArray<FWorldMapArrowPaint>&& Arrows);
	void ClearArrows();

protected:
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	                          const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	                          int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	TArray<FWorldMapArrowPaint> M_Arrows;
};
