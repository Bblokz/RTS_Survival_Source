// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldCountryOccupation/WorldCountryEdgeDistance.h"

namespace WorldCountryEdgeDistancePrivate
{
	// Finer than this adds nothing a border band a few units wide could show.
	constexpr float MinMapUnitsPerPixel = 3.f;
	constexpr float StraightStep = 1.f;
	constexpr float DiagonalStep = 1.41421356f;
	constexpr float FarInside = 1.0e6f;
	constexpr float OutsideClipRect = -1.f;
	constexpr float OutsideMarginMapUnits = 12.f;

	struct FRasterGrid
	{
		FBox2D Bounds = FBox2D(ForceInit);
		float MapUnitsPerPixel = 1.f;
		int32 Width = 0;
		int32 Height = 0;

		FVector2D GetPixelCentre(const int32 PixelX, const int32 PixelY) const
		{
			return FVector2D(Bounds.Min.X + (PixelX + 0.5f) * MapUnitsPerPixel,
			                 Bounds.Min.Y + (PixelY + 0.5f) * MapUnitsPerPixel);
		}
	};

	bool TryMakeGrid(const TArray<FWorldCountryTriangle2D>& CountryTriangles, const FBox2D& ClipRect,
	                 const int32 MaxPixelsPerSide, FRasterGrid& OutGrid)
	{
		FBox2D CountryBounds(ForceInit);
		for (const FWorldCountryTriangle2D& Triangle : CountryTriangles)
		{
			if (Triangle.Bounds.bIsValid && ClipRect.Intersect(Triangle.Bounds))
			{
				CountryBounds += Triangle.Bounds;
			}
		}

		if (not CountryBounds.bIsValid || MaxPixelsPerSide <= 0)
		{
			return false;
		}

		// Room for a few outside pixels around the country, so its outermost coast also gets an edge.
		CountryBounds = CountryBounds.ExpandBy(OutsideMarginMapUnits);

		OutGrid.Bounds = FBox2D(FVector2D::Max(CountryBounds.Min, ClipRect.Min),
		                        FVector2D::Min(CountryBounds.Max, ClipRect.Max));
		const FVector2D Size = OutGrid.Bounds.GetSize();
		if (Size.X <= 0.0 || Size.Y <= 0.0)
		{
			return false;
		}

		OutGrid.MapUnitsPerPixel = FMath::Max(MinMapUnitsPerPixel,
		                                      static_cast<float>(FMath::Max(Size.X, Size.Y)) / MaxPixelsPerSide);
		OutGrid.Width = FMath::Max(1, FMath::CeilToInt32(Size.X / OutGrid.MapUnitsPerPixel));
		OutGrid.Height = FMath::Max(1, FMath::CeilToInt32(Size.Y / OutGrid.MapUnitsPerPixel));
		// The picture covers whole pixels, so its rectangle grows to match them.
		OutGrid.Bounds.Max = OutGrid.Bounds.Min
			+ FVector2D(OutGrid.Width * OutGrid.MapUnitsPerPixel, OutGrid.Height * OutGrid.MapUnitsPerPixel);
		return true;
	}

	void RasteriseTriangle(const FWorldCountryTriangle2D& Triangle, const FRasterGrid& Grid, TArray<float>& InOutDistance)
	{
		const int32 FirstX = FMath::Clamp(
			FMath::FloorToInt32((Triangle.Bounds.Min.X - Grid.Bounds.Min.X) / Grid.MapUnitsPerPixel), 0, Grid.Width - 1);
		const int32 LastX = FMath::Clamp(
			FMath::FloorToInt32((Triangle.Bounds.Max.X - Grid.Bounds.Min.X) / Grid.MapUnitsPerPixel), 0, Grid.Width - 1);
		const int32 FirstY = FMath::Clamp(
			FMath::FloorToInt32((Triangle.Bounds.Min.Y - Grid.Bounds.Min.Y) / Grid.MapUnitsPerPixel), 0, Grid.Height - 1);
		const int32 LastY = FMath::Clamp(
			FMath::FloorToInt32((Triangle.Bounds.Max.Y - Grid.Bounds.Min.Y) / Grid.MapUnitsPerPixel), 0, Grid.Height - 1);
		for (int32 PixelY = FirstY; PixelY <= LastY; PixelY++)
		{
			for (int32 PixelX = FirstX; PixelX <= LastX; PixelX++)
			{
				if (WorldCountryLiberationRules::GetIsPointInTriangle2D(Grid.GetPixelCentre(PixelX, PixelY), Triangle))
				{
					InOutDistance[PixelY * Grid.Width + PixelX] = FarInside;
				}
			}
		}
	}

	/** Distance of a neighbour as seen from a pixel; pixels beyond the picture do not count as "outside the country". */
	float GetNeighbourDistance(const TArray<float>& Distance, const FRasterGrid& Grid, const int32 PixelX,
	                           const int32 PixelY)
	{
		if (PixelX < 0 || PixelY < 0 || PixelX >= Grid.Width || PixelY >= Grid.Height)
		{
			return OutsideClipRect;
		}
		return Distance[PixelY * Grid.Width + PixelX];
	}

	void RelaxPixel(TArray<float>& InOutDistance, const FRasterGrid& Grid, const int32 PixelX, const int32 PixelY,
	                const int32 StepX, const int32 StepY, const float StepLength)
	{
		const float NeighbourDistance = GetNeighbourDistance(InOutDistance, Grid, PixelX + StepX, PixelY + StepY);
		if (NeighbourDistance < 0.f)
		{
			return;
		}

		float& PixelDistance = InOutDistance[PixelY * Grid.Width + PixelX];
		PixelDistance = FMath::Min(PixelDistance, NeighbourDistance + StepLength);
	}

	/** Two sweeps of a chamfer distance transform, in pixels; outside pixels start at 0 and stay 0. */
	void MeasureDistanceToOutside(TArray<float>& InOutDistance, const FRasterGrid& Grid)
	{
		for (int32 PixelY = 0; PixelY < Grid.Height; PixelY++)
		{
			for (int32 PixelX = 0; PixelX < Grid.Width; PixelX++)
			{
				RelaxPixel(InOutDistance, Grid, PixelX, PixelY, -1, 0, StraightStep);
				RelaxPixel(InOutDistance, Grid, PixelX, PixelY, 0, -1, StraightStep);
				RelaxPixel(InOutDistance, Grid, PixelX, PixelY, -1, -1, DiagonalStep);
				RelaxPixel(InOutDistance, Grid, PixelX, PixelY, 1, -1, DiagonalStep);
			}
		}

		for (int32 PixelY = Grid.Height - 1; PixelY >= 0; PixelY--)
		{
			for (int32 PixelX = Grid.Width - 1; PixelX >= 0; PixelX--)
			{
				RelaxPixel(InOutDistance, Grid, PixelX, PixelY, 1, 0, StraightStep);
				RelaxPixel(InOutDistance, Grid, PixelX, PixelY, 0, 1, StraightStep);
				RelaxPixel(InOutDistance, Grid, PixelX, PixelY, 1, 1, DiagonalStep);
				RelaxPixel(InOutDistance, Grid, PixelX, PixelY, -1, 1, DiagonalStep);
			}
		}
	}
}

FWorldCountryEdgeDistanceField WorldCountryEdgeDistance::BuildEdgeDistanceField(
	const TArray<FWorldCountryTriangle2D>& CountryTriangles, const FBox2D& ClipRect, const int32 MaxPixelsPerSide)
{
	using namespace WorldCountryEdgeDistancePrivate;
	FWorldCountryEdgeDistanceField Field;
	FRasterGrid Grid;
	if (not ClipRect.bIsValid || not TryMakeGrid(CountryTriangles, ClipRect, MaxPixelsPerSide, Grid))
	{
		return Field;
	}

	TArray<float> Distance;
	Distance.Init(0.f, Grid.Width * Grid.Height);
	for (const FWorldCountryTriangle2D& Triangle : CountryTriangles)
	{
		if (Triangle.Bounds.bIsValid && Grid.Bounds.Intersect(Triangle.Bounds))
		{
			RasteriseTriangle(Triangle, Grid, Distance);
		}
	}
	MeasureDistanceToOutside(Distance, Grid);

	Field.Bounds = Grid.Bounds;
	Field.Width = Grid.Width;
	Field.Height = Grid.Height;
	Field.Pixels.SetNumUninitialized(Distance.Num());
	for (int32 PixelIndex = 0; PixelIndex < Distance.Num(); PixelIndex++)
	{
		const float DistanceInMapUnits = Distance[PixelIndex] * Grid.MapUnitsPerPixel;
		Field.Pixels[PixelIndex] = static_cast<uint8>(
			FMath::Clamp(DistanceInMapUnits / MaxEncodedDistance, 0.f, 1.f) * 255.f);
	}
	return Field;
}
