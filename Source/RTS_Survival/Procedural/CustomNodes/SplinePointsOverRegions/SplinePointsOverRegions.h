// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "PCGSettings.h"
#include "PCGData.h"
#include "SplinePointsOverRegions.generated.h"

/**
 * Settings for the GenerateSplinePointsOverRegions node.
 *
 * Parameters:
 * - MaxPoints / MinPoints: The maximum/minimum number of points to generate in the largest/smallest region.
 * - PreferredMinDistance: The minimal distance between any two generated points.
 * - MinAngle / MaxAngle: When generating more than 2 points in a region, the angle (in degrees) between consecutive segments
 *   must lie between these values.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UPCGGenerateSplinePointsOverRegionsSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	// ~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
	virtual FText GetDefaultNodeTitle() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Sampler; }
#endif
	virtual bool UseSeed() const override { return true; }
	virtual bool HasDynamicPins() const override { return false; }

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	// ~End UPCGSettings interface

public:
	/** Maximum number of points to generate in the largest region. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ClampMin = "0"))
	int32 MaxPoints = 10;

	/** Minimum number of points to generate in the smallest region. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ClampMin = "0"))
	int32 MinPoints = 3;
	
	/** Preferred minimal distance between any two generated points.
	 * a higher value usually means less sharp angles*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ClampMin = "0"))
	float PreferredMinDistance = 8000.f;

	// Set the Z value on each point generated.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ClampMin = "0"))
	float ZValue = 1200.f;
};

/**
 * Execution element for GenerateSplinePointsOverRegions.
 */
class FPCGGenerateSplinePointsOverRegionsElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};


namespace SplinePointsHelpers
{
	/**
 * @brief Extracts spatial regions from input tagged data and returns the largest connected component.
 *
 * This function casts each tagged input to spatial data, groups them into connected components
 * (using BuildConnectedComponents), and returns the largest component.
 *
 * @param Inputs Array of tagged input data.
 * @return TArray<const UPCGSpatialData*> The largest connected component of spatial regions.
 */
	TArray<const UPCGSpatialData*> ExtractLargestConnectedComponentOfRegions(const TArray<FPCGTaggedData>& Inputs);

/**
	* @brief Computes volumes for each region and returns the number of points to generate per region.
	*
	* Volumes are computed based on the region's bounding box. Using the minimum and maximum volume,
	* a ratio is computed for each region and used to interpolate between GlobalMinPoints and GlobalMaxPoints.
	*
	* @param SelectedRegions Array of regions.
	* @param Settings Spline generation settings containing MinPoints and MaxPoints.
	* @return TArray<int32> An array of point counts corresponding to each region.
*/
	TArray<int32> ComputeRegionPointCounts(const TArray<const UPCGSpatialData*>& SelectedRegions,
	                                       const UPCGGenerateSplinePointsOverRegionsSettings* Settings);


	/**
	 * Generates the last point for a region on its border.
	 * Samples a candidate along one of the region’s faces (using BorderRadius to define a narrow border zone).
	 * If a neighbor region is provided, this function should favor the shared border.
	 */
	bool GenerateLastRegionPoint(const FBox& RegionBox, const FVector& ReferencePoint, float BorderRadius,
	                             FRandomStream& RandStream, FVector& OutPoint);

	/**
	 * Generates intermediate points (if any) for a region between FirstPoint and LastPoint.
	 * Each candidate must be at least PreferredMinDistance from the immediately previous point and satisfy the angle constraint.
	 * Additional checks (e.g. line intersection tests) may be performed to avoid crossing segments.
	 */
	bool GenerateIntermediateRegionPoints(const FBox& RegionBox, int32 NumPoints, const FVector& FirstPoint,
	                                      const FVector& LastPoint, float PreferredMinDistance, float MinAngleDeg,
	                                      float MaxAngleDeg, int32 MaxAttempts, FRandomStream& RandStream,
	                                      TArray<FVector>& OutPoints);

	/**
	 * Generates all spline points for a region.
	 * Calls GenerateFirstRegionPoint, then GenerateIntermediateRegionPoints (if needed), then GenerateLastRegionPoint.
	 * Returns a list of points for that region.
	 */
	bool GenerateRegionSplinePoints(const FBox& RegionBox, int32 NumPoints, const FVector& LastSplinePoint,
	                                float PreferredMinDistance, float MinAngleDeg, float MaxAngleDeg,
	                                float BorderRadius, FRandomStream& RandStream, TArray<FVector>& OutPoints);

	/**
	* @brief Finds the longest chain of adjacent regions.
	*
	* This function exhaustively searches through the given regions using a depth-first search
	* to determine the longest sequence in which each consecutive region is directly adjacent.
	* It returns the ordered array of regions representing that chain.
	*
	* @param SelectedRegions The array of regions to search.
	* @return The longest chain of adjacent regions.
	*/
	TArray<const UPCGSpatialData*>
	FindLongestAdjacentRegionChain(const TArray<const UPCGSpatialData*>& SelectedRegions);


	/**
	 * @brief Applies Laplacian smoothing to in-between spline points to reduce sharp angles.
	 *
	 * For each in-between point (identified by its index in InbetweenIndices), this function calculates
	 * the angle between its immediate neighbors. If the angle is below the given threshold, the point is
	 * moved toward the midpoint of its neighbors.
	 *
	 * @param GlobalSplinePoints The array of spline points (will be modified in place).
	 * @param InbetweenIndices The indices of spline points that occur between regions.
	 * @param ThresholdAngleDegrees The threshold angle (in degrees) below which smoothing is applied (default is 160°).
	 */
	void LaplacianSmoothing(TArray<FVector>& GlobalSplinePoints, const TArray<int32>& InbetweenIndices, float ThresholdAngleDegrees = 160.f);


	/**
 * @brief Creates the output point data from the given spline points and populates the PCG context.
 *
 * Converts each FVector in GlobalSplinePoints to an FPCGPoint, creates the output point data,
 * and then updates the context’s output collection.
 *
 * @param Context The PCG context to populate.
 * @param GlobalSplinePoints Array of spline point positions.
 * @return bool True if the output data was successfully created.
 */
	bool CreateOutputData(FPCGContext* Context, const TArray<FVector>& GlobalSplinePoints);

	void EnsurePairsSatisfyDistanceConstraint(const UPCGGenerateSplinePointsOverRegionsSettings* Settings,
	                                          TArray<FVector>& GlobalSplinePoints);
}
