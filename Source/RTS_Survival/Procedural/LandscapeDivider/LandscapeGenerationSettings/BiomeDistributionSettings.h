#pragma once

#include "CoreMinimal.h"

#include "BiomeDistributionSettings.generated.h"

USTRUCT(BlueprintType)
struct FBiomeDistribution
{
	GENERATED_BODY()

	// MinimalAmount that will be placed in the world for sure.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome Distribution")
	int32 MinimalAmount = 1;
	// Probability weight of this biome.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome Distribution")
	float Weight = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome Distribution")
	FName BiomeName;
	// If true, the biome will be placed in the largest vacant area.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome Distribution")
	bool PreferLargestVacantBiome = false;
	// If true, the biome will be placed in the smallest vacant area.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome Distribution")
	bool PreferSmallestVacantBiome = false;
	
};

USTRUCT(BlueprintType)
struct FBiomeGenerationSettings
{
	GENERATED_BODY()

	/**
	 * @brief Total number of regions to create.
	 *
	 * The last region is special – it will fill up whatever area remains.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 AmountRegions = 6;
	

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FBiomeDistribution> BiomeDistributions;

	/**
	 * @brief Minimum and maximum region width (Y direction).
	 *
	 * When splitting regions the random width of a region (i.e. its extent in Y)
	 * will be chosen in this range.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector2D MinMaxRegionWidth = FVector2D(10000.f, 50000.f);

	/**
	 * @brief Minimum and maximum region height (X direction).
	 *
	 * When splitting regions the random height of a region (i.e. its extent in X)
	 * will be chosen in this range.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector2D MinMaxRegionHeight = FVector2D(10000.f, 50000.f);
	
};
