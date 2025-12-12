#pragma once

#include "CoreMinimal.h"

#include "PresetRegionSettings.generated.h"

// These types are used for regions with predefined bounds as in the FRTSLandscapeRegionPresets
// These are non vacant regions with a purpose predefined before generating the landscape.
UENUM(BlueprintType)
enum class ERTSPresetRegion : uint8
{
	PresetRegion_None,
	PresetRegion_PlayerStart,
	PresetRegion_EnemyStart,
};

USTRUCT(BlueprintType)
struct FRTSPresetRegion
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector2D MinMaxRegionWidth = FVector2D(50000.f, 50000.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector2D MinMaxRegionHeight = FVector2D(50000.f, 50000.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ERTSPresetRegion RegionType = ERTSPresetRegion::PresetRegion_None;
};

USTRUCT(BlueprintType)
struct FRTSPresetRegionMinimalDistances
{
	GENERATED_BODY()
	// Minimal distance to itself and other types of preset regions.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<ERTSPresetRegion, float> MinDistances;
};

USTRUCT(BlueprintType)
struct FRTSLandscapeRegionPresets
{
	GENERATED_BODY()

	// For each type of preset region, this defines the minimal distance to itself, and to other types of preset regions.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<ERTSPresetRegion, FRTSPresetRegionMinimalDistances> RegionMinDistances;

	// For each  type of preset region, this is the tag we give to the component once generated.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<ERTSPresetRegion, FName> RegionTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FRTSPresetRegion> PresetRegions;

	// Stepsize for our virtual grid (default: 1000). This determines the size of a grid cell that is used in backtracking when
	// placing region presets.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Stepsize = 1000.f;
};
