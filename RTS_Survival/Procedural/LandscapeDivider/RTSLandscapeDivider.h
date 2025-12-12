// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGGraph.h"
#include "GameFramework/Actor.h"
#include "LandscapeGenerationSettings/BiomeDistributionSettings.h"
#include "LandscapeGenerationSettings/PresetRegionSettings.h"
#include "LandscapeGenerationSettings/RTS_PCG_Parameters.h"
#include "RTSDivisionBox/RTSDivisionBox.h"
#include "RTSLandscapeDivider.generated.h"

struct FRTSPCGParameters;
class URTSRuntimeSeedManager;
class ARTSBiome;

/**
 * @brief Actor that subdivides the landscape into regions using box components.
 *
 * This actor assumes that the landscape is square and that the actor is placed at its center.
 * The full landscape extents run from -LandscapeExtent to +LandscapeExtent along both X and Y.
 * A binary space partitioning approach is used to split the full landscape into (AmountRegions) regions.
 * Each region is represented by a box component which is sized and positioned to cover the region.
 */
UCLASS()
class RTS_SURVIVAL_API ARTSLandscapeDivider : public AActor
{
	GENERATED_BODY()

public:
	/// Sets default values for this actor's properties.
	ARTSLandscapeDivider();

	/**
	 * @brief Regenerates the landscape division and reassigns biome tags.
	 */
	UFUNCTION(CallInEditor, Category = "Landscape Divider")
	void Regenerate();

	URTSRuntimeSeedManager* GetSeedManager() const { return M_SeedManager; }

	/** @return The amount of regions in the preset region settings with the ERTSPresetRegion::PresetRegion_PlayerStart. */
	int32 GetAmountOfPlayerStartRegions() const;

protected:
	/// Called when the game starts or when spawned.
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landscape Divider")
	int32 Seed = 0;

	// Determines the amount of resource areas in the player start regions.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landscape Divider|GameGeneration")
	FRTSPCGParameters PcgParameters;
	
	/**
	 * @brief Landscape extent from the actor location.
	 *
	 * The actor is assumed to be at the center of a square landscape. The landscape
	 * goes from -LandscapeExtent to +LandscapeExtent in both X and Y.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landscape Divider")
	float LandscapeExtent = 50000.f;

	// Settings for generating preset regions.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landscape Divider")
	FRTSLandscapeRegionPresets RegionPresetSettings;

	/**
	 * @brief Settings for generating biomes after preset regions are placed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landscape Divider")
	FBiomeGenerationSettings BiomeGenerationSettings;

	/**
	 * @brief PCG component used to drive biome generation.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PCG_Graph")
	TObjectPtr<UPCGComponent> PcgComponent;

	/**
	 * @brief PCG Graph Instance used to configure the PCG component.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PCG_Graph")
	TObjectPtr<UPCGGraphInstance> PcGraphInstance;

private:
    /** 
     * @brief Array storing the FBox2D bounds of preset regions generated via backtracking.
     * 
     * @pre Preset regions have been generated.
     * @post M_PresetRegions contains the bounds for each preset region.
     */
    UPROPERTY()
    TArray<FBox2D> M_PresetRegions;

    /** 
     * @brief The generated box components (one per region).
     * 
     * @pre Regions have been generated.
     * @post M_Boxes contains all created box components.
     */
    UPROPERTY()
    TArray<TObjectPtr<URTSDivisionBox>> M_Boxes;

    ////////////////////////////////////////////////////////////////////////////////
    // Region Generation & Backtracking Helpers
    ////////////////////////////////////////////////////////////////////////////////

    /**
     * @brief Divides the landscape into regions.
     * 
     * Uses binary space partitioning and preset region generation to cover the landscape.
     * 
     * @pre LandscapeExtent is set.
     * @post The full landscape is partitioned into non-overlapping regions.
     */
    void GenerateRegions();

    /**
     * @brief Generates preset regions on the landscape using backtracking.
     * 
     * @pre RegionPresetSettings must be configured and valid.
     * @post Preset regions are created and stored in M_PresetRegions.
     */
    void GeneratePresetRegions(const FRandomStream& RandomStreamWithSeed);

    /**
     * @brief Checks that preset region settings are valid.
     * 
     * @pre RegionPresetSettings.PresetRegions must be defined.
     * @post Returns true if all preset settings are valid.
     * @return true if valid, false otherwise.
     */
    bool EnsurePresetRegionsAreValid();

    /**
     * @brief Recursively attempts to place preset regions via backtracking.
     * 
     * @pre FullArea is defined.
     * @param RegionIndex Index of the preset region to place.
     * @param OutPlacements Array to store successful placements.
     * @param FullArea The full landscape area.
     * @param PresetSettings The preset region configuration.
     * @param RandomStreamWithSeed
     * @return true if all preset regions are successfully placed.
     * @post OutPlacements contains the placed regions.
     */
    bool BacktrackingPlacePresetRegions(const int32 RegionIndex, TArray<struct FPresetRegionPlacement>& OutPlacements,
                               const FBox2D& FullArea, const FRTSLandscapeRegionPresets& PresetSettings, const FRandomStream& RandomStreamWithSeed) const;

    /**
     * @brief Retrieves candidate grid positions for a region of a given size.
     * 
     * @pre FullArea is defined.
     * @param RegionSize The size of the region.
     * @param FullArea The full landscape area.
     * @param bRandomizeOrder If true, candidate order is randomized.
     * @param RandomStreamWithSeed
     * @return Array of bottom-left grid positions.
     * @post Candidate positions are computed.
     */
    TArray<FVector2D> GetCandidateGridPositions(const FVector2D& RegionSize, const FBox2D& FullArea,
                                                const bool bRandomizeOrder, const FRandomStream& RandomStreamWithSeed) const;

    /**
     * @brief Generates a random dimension within a range rounded to a grid step.
     * 
     * @pre MinMaxRange and Stepsize are set.
     * @param MinMaxRange The minimum and maximum allowed dimension.
     * @param Stepsize The grid step for rounding.
     * @param RandomStreamWithSeed
     * @return A dimension value rounded down to a multiple of Stepsize.
     * @post Returned value is >= Stepsize.
     */
    float GenerateDimension(const FVector2D& MinMaxRange, const float Stepsize, const FRandomStream& RandomStreamWithSeed) const;

    /**
     * @brief Calculates the distance between two 2D boxes.
     * 
     * @pre Both BoxA and BoxB are valid.
     * @param BoxA The first box.
     * @param BoxB The second box.
     * @return Euclidean distance between BoxA and BoxB.
     * @post Distance is non-negative.
     */
    float GetDistanceBetweenBoxes(const FBox2D& BoxA, const FBox2D& BoxB) const;

    /**
     * @brief Creates a box component representing a specified region.
     * 
     * @pre RegionBox must be a valid rectangle.
     * @param RegionBox The region's bounding box (Min is bottom–left, Max is top–right).
     * @post A new box component is created and attached.
     */
    void CreateBoxFromRegion(const FBox2D& RegionBox);

    /**
     * @brief Sets up the PCG component with the provided graph instance.
     * 
     * @pre PcGraphInstance and its graph must be valid.
     * @post The PCG component is created (or reused) and generated.
     */
    void SetupPcgComponent();

    /**
     * @brief Called when a valid box component cannot be generated.
     * 
     * @pre Box generation has failed.
     * @post An error is reported.
     */
    void OnInvalidBoxGeneration();

    ////////////////////////////////////////////////////////////////////////////////
    // Biome Distribution Functions
    ////////////////////////////////////////////////////////////////////////////////

    /**
     * @brief Distributes biome tags among the generated boxes.
     * 
     * First assigns boxes based on minimal amounts from each biome distribution,
     * then assigns the remaining vacant boxes based on biome weights.
     * 
     * @pre Boxes have been generated.
     * @post Biome tags are assigned to each box.
     */
    void DistributeBiomes(const FRandomStream& RandomStreamWithSeed);

    /**
     * @brief Assigns minimal biome tags to boxes.
     * 
     * @pre VacantBoxes contains boxes with no biome tag.
     * @param VacantBoxes Array of vacant box components.
     * @post Each box gets a minimal biome tag as required.
     */
    void AssignMinimalBiomes(TArray<URTSDivisionBox*>& VacantBoxes);

    /**
     * @brief Assigns weighted biome tags to remaining boxes.
     * 
     * @pre VacantBoxes contains boxes without minimal biome assignment.
     * @param VacantBoxes Array of vacant box components.
     * @param RandomStreamWithSeed
     * @post Remaining boxes receive weighted biome tags.
     */
    void AssignWeightedBiomes(TArray<URTSDivisionBox*>& VacantBoxes, const FRandomStream& RandomStreamWithSeed);

    /**
     * @brief Retrieves the largest box (by area) from an array.
     * 
     * @pre Boxes array is provided.
     * @param Boxes Array of box components.
     * @return Pointer to the largest box, or nullptr if empty.
     * @post Largest box is identified.
     */
    URTSDivisionBox* GetLargestBox(const TArray<URTSDivisionBox*>& Boxes);

    /**
     * @brief Retrieves the smallest box (by area) from an array.
     * 
     * @pre Boxes array is provided.
     * @param Boxes Array of box components.
     * @return Pointer to the smallest box, or nullptr if empty.
     * @post Smallest box is identified.
     */
    URTSDivisionBox* GetSmallestBox(const TArray<URTSDivisionBox*>& Boxes);

    ////////////////////////////////////////////////////////////////////////////////
    // Biome Region Generation for Uncovered Area
    ////////////////////////////////////////////////////////////////////////////////

    /**
     * @brief Generates biome candidate regions covering the uncovered area.
     * 
     * @pre FullArea is defined and preset boxes have been subtracted.
     * @param FullArea The entire landscape area.
     * @param PresetBoxes Array of preset region bounds.
     * @param TargetRegions Desired number of biome regions.
     * @param RandomStreamWithSeed
     * @return Array of FBox2D covering the remaining area.
     * @post The uncovered area is partitioned into candidate biome regions.
     */
    TArray<FBox2D> GenerateBiomeRegions(const FBox2D& FullArea, const TArray<FBox2D>& PresetBoxes, int32 TargetRegions, const FRandomStream&
                                        RandomStreamWithSeed) const;

    /**
     * @brief Subtracts preset boxes from a full area, returning uncovered regions.
     * 
     * @pre FullArea is defined.
     * @param FullArea The area to subtract from.
     * @param PresetBoxes Array of preset region boxes.
     * @return Array of FBox2D representing the uncovered parts.
     * @post The returned regions exactly cover FullArea minus the preset boxes.
     */
    TArray<FBox2D> SubtractPresetRegions(const FBox2D& FullArea, const TArray<FBox2D>& PresetBoxes) const;

    /**
     * @brief Subtracts rectangle B from rectangle A, returning remaining sub-rectangles.
     * 
     * @pre Rectangles A and B are valid and may intersect.
     * @param A The original rectangle.
     * @param B The rectangle to subtract.
     * @return Array of FBox2D representing A minus B.
     * @post The returned rectangles do not overlap B.
     */
    TArray<FBox2D> SubtractRect(const FBox2D& A, const FBox2D& B) const;

	// Component that keeps a random stream Initialized with the same seed as the landscape divider.
	// Allows placed objects to get a random seeded value.
	UPROPERTY()
	URTSRuntimeSeedManager* M_SeedManager;

	/** @return Whether the seed manager is valid; may attempt to create a new manager*/
	bool EnsureValidSeedManager();

	/** @brief Adjusts the PCG parameters if needed to make sure we do not provide the pcg with conflicting parameters. */
	void EnsurePcgParametersAreConsistent(FRTSPCGParameters& OutParametersToCheck) const;


	/**
	 * @brief There is a bug in PCG where i a specific branch / set of chosen points is empty the graph gets stuck.
	 * This function will ensure that it is not possible to create empty set of specific resource points.
	 * @param OutParametersToCheck The paramters that will be set on the PCG graph. 
	 */
	void EnsureResourceRegionsNonZero(FRTSPCGParameters& OutParametersToCheck) const;
	/**
	 * @brief Sets the parameters on the PCG graph by using the name of the parameter and the value.
	 * @param ParametersToSet The Parameter collection for the PCG graph.
	 * @param Graph The graph to set the parameters on.
	 * @pre The parameters are checked for consistency.
	 * @pre The parameter names are the exact same as in the graph.
	 */
	void SetupPcgGraphParameters(const FRTSPCGParameters& ParametersToSet, UPCGGraph* Graph) const;

	
	void CheckPcgParameterResult(const EPropertyBagResult Result, const FName& ParameterName) const;

};
