// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

struct FEnemyFootprint;

/**
 * @brief What a base knows about the ground around its start location, measured from real traces so it follows
 * whatever terrain the map has. Fed sample by sample (the component spreads the traces over frames), then Finish()
 * derives slopes, drivable ground, open approaches and overlooks. Pure data, so the layout step stays UObject free.
 */
class FSurvivalSovietSiteMap
{
public:
	/**
	 * @brief Sizes the sample grid around the centre.
	 * @param InBuildRadius Radius of the flat build area.
	 * @param InReach How far past the build area to look (ridges, ramps).
	 */
	void Init(const FVector2D& InCenter, double InCenterHeight, double InBuildRadius, double InReach);

	int32 GetSampleCount() const { return M_Heights.Num(); }
	double GetCenterHeight() const { return M_CenterHeight; }
	bool GetHasGround(const int32 SampleIndex) const { return M_HasGround[SampleIndex]; }
	double GetSampleSlope(const int32 SampleIndex) const { return M_Slopes[SampleIndex]; }

	FVector2D GetSamplePosition(int32 SampleIndex) const;

	/** @param bHasGround False when the trace found nothing; that sample then counts as a cliff. */
	void SetSample(int32 SampleIndex, double Height, bool bHasGround);

	/** @brief Derives everything the queries below return; call once after the last sample. */
	void Finish();

	/** @return True when the whole footprint stands on level ground (no slope, no step under any corner). */
	bool GetIsFootprintBuildable(const FEnemyFootprint& Footprint) const;

	/** @return True when a vehicle can get from the centre to this point over the ground alone. */
	bool GetIsDrivable(const FVector2D& Position) const;

	/** @return Height relative to the centre; 0 outside the grid. */
	double GetRelativeHeight(const FVector2D& Position) const;

	/** @return 0..1, how open the ground is in this direction just past the build area; 1 = free drive outward. */
	double GetApproachOpenness(double YawRadians) const;

	/** @return Middle of each continuous open arc around the build area, widest first. */
	const TArray<double>& GetApproachYaws() const { return M_ApproachYaws; }

	/** @return Level raised spots past the inner part of the base: ridge tops and platforms, spaced out. */
	const TArray<FVector2D>& GetOverlooks() const { return M_Overlooks; }

	/**
	 * @brief Finds a drivable polyline over the sample grid; used for tracks to ramp heads and strongpoints.
	 * @return Simplified points from From to To; empty when no drivable route exists.
	 */
	TArray<FVector2D> FindDrivablePath(const FVector2D& From, const FVector2D& To) const;

private:
	FVector2D M_Center = FVector2D::ZeroVector;
	FVector2D M_Origin = FVector2D::ZeroVector;
	double M_CenterHeight = 0.0;
	double M_BuildRadius = 0.0;
	double M_OuterRadius = 0.0;
	int32 M_Dimension = 0;

	// Relative to the centre height; parallel arrays over the square grid.
	TArray<float> M_Heights;
	TArray<bool> M_HasGround;
	TArray<float> M_Slopes;
	TArray<bool> M_Drivable;
	TArray<double> M_SectorOpenness;
	TArray<double> M_ApproachYaws;
	TArray<FVector2D> M_Overlooks;

	int32 GetIndex(int32 X, int32 Y) const { return Y * M_Dimension + X; }
	bool GetIsValidCell(int32 X, int32 Y) const { return X >= 0 and Y >= 0 and X < M_Dimension and Y < M_Dimension; }
	FIntPoint GetCell(const FVector2D& Position) const;
	double SampleSlopeAt(int32 X, int32 Y) const;
	bool GetIsCellDrivable(const FVector2D& Position) const;

	void BuildSlopes();
	void FloodDrivable();
	void BuildApproaches();
	void BuildOverlooks();
	bool GetIsCellOverlook(int32 X, int32 Y) const;
};
