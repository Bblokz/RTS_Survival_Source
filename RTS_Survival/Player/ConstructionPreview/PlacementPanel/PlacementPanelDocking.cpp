// Copyright (C) Bas Blokzijl - All rights reserved.

#include "PlacementPanelTypes.h"

namespace PlacementPanelDockingPrivate
{
	// Without safe bands the panel still keeps this far from the screen edges.
	constexpr float MinimumScreenEdgeMargin = 8.f;

	FVector2D GetUnclampedTopLeft(const FPlacementPanelDockRequest& Request, const EPlacementPanelDock Dock)
	{
		const FBox2D& Avoid = Request.AvoidRect;
		const FVector2D Center = Avoid.GetCenter();
		const FVector2D& Size = Request.PanelSize;
		switch (Dock)
		{
		case EPlacementPanelDock::Right:
			return FVector2D(Avoid.Max.X + Request.Gap, Center.Y - Size.Y * 0.5f);
		case EPlacementPanelDock::Left:
			return FVector2D(Avoid.Min.X - Request.Gap - Size.X, Center.Y - Size.Y * 0.5f);
		case EPlacementPanelDock::Below:
			return FVector2D(Center.X - Size.X * 0.5f, Avoid.Max.Y + Request.Gap);
		case EPlacementPanelDock::Above:
			return FVector2D(Center.X - Size.X * 0.5f, Avoid.Min.Y - Request.Gap - Size.Y);
		}
		return Avoid.Max;
	}

	FVector2D ClampToArea(const FVector2D& TopLeft, const FVector2D& PanelSize, const FVector2D& ViewportSize,
	                      const FMargin& Insets)
	{
		const FVector2D AreaMin(Insets.Left, Insets.Top);
		const FVector2D AreaMax = ViewportSize - FVector2D(Insets.Right, Insets.Bottom) - PanelSize;
		// A panel larger than the area sticks to its top left corner.
		return FVector2D(FMath::Clamp(TopLeft.X, AreaMin.X, FMath::Max(AreaMin.X, AreaMax.X)),
		                 FMath::Clamp(TopLeft.Y, AreaMin.Y, FMath::Max(AreaMin.Y, AreaMax.Y)));
	}

	float GetOverlapArea(const FBox2D& RectA, const FBox2D& RectB)
	{
		const float OverlapX = FMath::Min(RectA.Max.X, RectB.Max.X) - FMath::Max(RectA.Min.X, RectB.Min.X);
		const float OverlapY = FMath::Min(RectA.Max.Y, RectB.Max.Y) - FMath::Max(RectA.Min.Y, RectB.Min.Y);
		return FMath::Max(0.f, OverlapX) * FMath::Max(0.f, OverlapY);
	}

	TArray<EPlacementPanelDock, TInlineAllocator<5>> GetDockOrder(const EPlacementPanelDock PreferredDock)
	{
		TArray<EPlacementPanelDock, TInlineAllocator<5>> Order;
		Order.Add(PreferredDock);
		for (const EPlacementPanelDock Dock : {
			     EPlacementPanelDock::Right, EPlacementPanelDock::Left,
			     EPlacementPanelDock::Below, EPlacementPanelDock::Above
		     })
		{
			Order.AddUnique(Dock);
		}
		return Order;
	}

	/**
	 * @return Whether a side within these insets keeps the panel off the avoided rectangle; OutResult holds it,
	 * or else the spot that covers the least of the rectangle.
	 */
	bool TryDockWithinInsets(const FPlacementPanelDockRequest& Request, const FMargin& Insets,
	                         FPlacementPanelDockResult& OutResult)
	{
		float LeastOverlapArea = TNumericLimits<float>::Max();
		for (const EPlacementPanelDock Dock : GetDockOrder(Request.PreferredDock))
		{
			const FVector2D TopLeft = ClampToArea(GetUnclampedTopLeft(Request, Dock), Request.PanelSize,
			                                      Request.ViewportSize, Insets);
			const float OverlapArea = GetOverlapArea(PlacementPanelDocking::MakePanelRect(TopLeft, Request.PanelSize),
			                                         Request.AvoidRect);
			if (OverlapArea <= 0.f)
			{
				OutResult = {TopLeft, Dock, false};
				return true;
			}
			if (OverlapArea < LeastOverlapArea)
			{
				LeastOverlapArea = OverlapArea;
				OutResult = {TopLeft, Dock, true};
			}
		}
		return false;
	}
}

FPlacementPanelDockResult PlacementPanelDocking::ChooseDock(const FPlacementPanelDockRequest& Request)
{
	using namespace PlacementPanelDockingPrivate;
	FPlacementPanelDockResult Result;
	if (not Request.AvoidRect.bIsValid)
	{
		// Nothing to avoid: sit just above the safe bottom band, centred.
		Result.TopLeft = ClampToArea(FVector2D((Request.ViewportSize.X - Request.PanelSize.X) * 0.5f,
		                                       Request.ViewportSize.Y),
		                             Request.PanelSize, Request.ViewportSize, Request.SafeInsets);
		return Result;
	}
	if (TryDockWithinInsets(Request, Request.SafeInsets, Result))
	{
		return Result;
	}
	// Over the HUD bands is better than over the building.
	TryDockWithinInsets(Request, FMargin(MinimumScreenEdgeMargin), Result);
	return Result;
}

FBox2D PlacementPanelDocking::MakePanelRect(const FVector2D& TopLeft, const FVector2D& PanelSize)
{
	return FBox2D(TopLeft, TopLeft + PanelSize);
}

bool PlacementPanelDocking::GetDoRectsOverlap(const FBox2D& RectA, const FBox2D& RectB)
{
	return PlacementPanelDockingPrivate::GetOverlapArea(RectA, RectB) > 0.f;
}
