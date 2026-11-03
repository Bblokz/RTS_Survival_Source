// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "SurvivalDefensePointFinder.generated.h"

class FSurvivalGroundProjector;

/**
 * @brief Where a start location's defense points come from.
 */
UENUM(BlueprintType)
enum class ESurvivalDefensePointSource : uint8
{
	// Only the points the designer dragged into place.
	DesignerOnly,
	// Only hill tops found on the landscape.
	AutoOnly,
	// Designer points first, then hill tops until MaxAutoPoints is reached.
	DesignerThenAuto
};

/**
 * @brief Controls the hill-top scan around a start location: a polar grid of landscape heights from inside the
 * build area out past its edge; local maxima high enough above the deploy centre with a flat top become points.
 */
USTRUCT(BlueprintType)
struct FSurvivalDefensePointSearchSettings
{
	GENERATED_BODY()

	// Scan starts at this fraction of the build radius (hills may poke into the build area).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Search", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InnerRadiusFraction = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Search", meta = (ClampMin = "0.0", Units = "cm"))
	float SearchDistanceBeyondBuildEdge = 4500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Search", meta = (ClampMin = "100.0", Units = "cm"))
	float RadialStep = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Search", meta = (ClampMin = "1.0", ClampMax = "45.0", Units = "deg"))
	float AngularStepDegrees = 6.f;

	// A hill top must be at least this much higher than the deploy centre.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hill Top", meta = (ClampMin = "0.0", Units = "cm"))
	float MinHeightAboveDeployCenter = 300.f;

	// Flatness of the top: heights SlopeSampleDistance away may differ by at most tan(MaxTopSlope) * distance.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hill Top", meta = (ClampMin = "0.0", ClampMax = "89.0", Units = "deg"))
	float MaxTopSlopeDegrees = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hill Top", meta = (ClampMin = "50.0", Units = "cm"))
	float SlopeSampleDistance = 300.f;

	// Minimum distance between two defense points (designer points included).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection", meta = (ClampMin = "0.0", Units = "cm"))
	float MinSpacing = 2500.f;

	// Total defense points (designer + auto) the scan fills up to.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection", meta = (ClampMin = "0"))
	int32 MaxPoints = 8;

	// When neither the designer nor the scan produced a point: this many evenly spaced points on the build edge.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection", meta = (ClampMin = "0"))
	int32 FallbackBuildEdgePointCount = 4;
};

/** @brief How a defense point was found; drives the editor colours and the pit priority. */
enum class ESurvivalDefensePointOrigin : uint8
{
	Designer,
	HillTop,
	BuildEdgeFallback
};

/** @brief One resolved defense point on the landscape. */
struct FSurvivalDefensePoint
{
	FVector Location = FVector::ZeroVector;
	float HeightAboveDeployCenter = 0.f;
	ESurvivalDefensePointOrigin Origin = ESurvivalDefensePointOrigin::Designer;
};

/**
 * @brief Resolves a start location's defense points: designer points snapped to the landscape, hill tops from a
 * height scan, and a build-edge fallback so pits and Soviet defenses always have somewhere to go.
 * @note PCGFindEnemyDefensePoints is not reused: it selects a rectangular symmetric formation around an
 * "EnemyBase" spatial input from given candidates and does not look at terrain height.
 */
class FSurvivalDefensePointFinder
{
public:
	/**
	 * @brief Builds the ordered defense point list (designer first, then hill tops by height, then fallback).
	 * @param Projector Landscape access for this world.
	 * @param DeployCenter Centre of the flat build area.
	 * @param BuildAreaRadius Radius of the flat build area.
	 * @param DesignerPointsWorld Designer points in world space (snapped to the landscape here).
	 * @param Source Which of the designer / auto points to use.
	 * @param Search Hill-top scan settings.
	 * @param OutPoints Receives the resolved points in priority order.
	 */
	static void ResolveDefensePoints(
		const FSurvivalGroundProjector& Projector,
		const FVector& DeployCenter,
		const float BuildAreaRadius,
		const TArray<FVector>& DesignerPointsWorld,
		const ESurvivalDefensePointSource Source,
		const FSurvivalDefensePointSearchSettings& Search,
		TArray<FSurvivalDefensePoint>& OutPoints);

private:
	struct FHillTopCandidate
	{
		FVector Location = FVector::ZeroVector;
		float HeightAboveCenter = 0.f;
		float DistanceToCenter = 0.f;
	};

	// Landscape heights on rings around the deploy centre; index = RadialIndex * AngularCount + AngularIndex.
	struct FPolarHeightGrid
	{
		int32 RadialCount = 0;
		int32 AngularCount = 0;
		TArray<float> Heights;
		TArray<bool> bIsValid;
		TArray<FVector2D> Positions;

		int32 GetIndex(const int32 RadialIndex, const int32 AngularIndex) const
		{
			return RadialIndex * AngularCount + AngularIndex;
		}
	};

	static void SampleHeightGrid(
		const FSurvivalGroundProjector& Projector,
		const FVector& DeployCenter,
		const float BuildAreaRadius,
		const FSurvivalDefensePointSearchSettings& Search,
		FPolarHeightGrid& OutGrid);

	// Plateaus count as maxima: neighbours may be up to LocalMaximumTolerance higher.
	static bool GetIsLocalMaximum(const FPolarHeightGrid& Grid, const int32 SampleIndex);

	static void AddDesignerPoints(
		const FSurvivalGroundProjector& Projector,
		const float CenterHeight,
		const TArray<FVector>& DesignerPointsWorld,
		TArray<FSurvivalDefensePoint>& OutPoints);

	static void FindHillTopCandidates(
		const FSurvivalGroundProjector& Projector,
		const FVector& DeployCenter,
		const float CenterHeight,
		const float BuildAreaRadius,
		const FSurvivalDefensePointSearchSettings& Search,
		TArray<FHillTopCandidate>& OutCandidates);

	static bool GetIsFlatTop(
		const FSurvivalGroundProjector& Projector,
		const FVector2D& Position,
		const float Height,
		const FSurvivalDefensePointSearchSettings& Search);

	static void SelectSpacedHillTops(
		const TArray<FHillTopCandidate>& Candidates,
		const FSurvivalDefensePointSearchSettings& Search,
		TArray<FSurvivalDefensePoint>& InOutPoints);

	static void AddBuildEdgeFallback(
		const FSurvivalGroundProjector& Projector,
		const FVector& DeployCenter,
		const float CenterHeight,
		const float BuildAreaRadius,
		const int32 PointCount,
		TArray<FSurvivalDefensePoint>& OutPoints);

	static bool GetIsFarFromPoints(
		const FVector& Location,
		const TArray<FSurvivalDefensePoint>& Points,
		const float MinSpacing);
};
