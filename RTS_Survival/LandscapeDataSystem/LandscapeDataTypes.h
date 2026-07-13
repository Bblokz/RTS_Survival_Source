// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "LandscapeDataTypes.generated.h"

/** @brief Channels available in the runtime Landscape data texture. */
UENUM(BlueprintType)
enum class ERTSLandscapeDataChannel : uint8
{
	Scorched = 0 UMETA(DisplayName = "Scorched (R)"),
	Gravel = 1 UMETA(DisplayName = "Gravel (G)"),
	Concrete = 2 UMETA(DisplayName = "Concrete (B)"),
	ExtraSand = 3 UMETA(DisplayName = "Extra Sand (A)")
};

/** @brief Selects how a PCG point's local bounds become Landscape paint coverage. */
UENUM(BlueprintType)
enum class ERTSLandscapePointPaintMode : uint8
{
	Solid UMETA(DisplayName = "Solid Bounds"),
	Radial UMETA(DisplayName = "Radial"),
	PerlinNoise UMETA(DisplayName = "Perlin Noise"),
	RadialWithNoisyBounds UMETA(DisplayName = "Radial With Noisy Bounds")
};

/** @brief Gives designers control over the soft elliptical fade inside point bounds. */
USTRUCT(BlueprintType)
struct FRTSLandscapeRadialPointPaintSettings
{
	GENERATED_BODY()

	/** Elliptical radius that remains at full point density, where 0 is center and 1 is the bounds edge. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Radial", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float FullStrengthRadius = 0.25f;

	/** Coverage retained at the ellipse perimeter. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Radial", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float EdgeStrength = 0.0f;

	/** Values above one hold center strength longer; values below one produce an earlier fade. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Radial", meta = (ClampMin = "0.05", ClampMax = "8.0", UIMin = "0.05", UIMax = "8.0"))
	float FalloffExponent = 1.5f;
};

/** @brief Controls deterministic fractal Perlin coverage generated within point bounds. */
USTRUCT(BlueprintType)
struct FRTSLandscapePerlinPointPaintSettings
{
	GENERATED_BODY()

	/** Pattern scale within each point; higher values create smaller patches. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Perlin Noise", meta = (ClampMin = "0.01", UIMin = "0.1", UIMax = "20.0"))
	float Frequency = 3.0f;

	/** Number of progressively finer noise layers evaluated during the one-time rebuild. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Perlin Noise", meta = (ClampMin = "1", ClampMax = "6", UIMin = "1", UIMax = "6"))
	int32 Octaves = 3;

	/** Strength retained by each finer octave. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Perlin Noise", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Persistence = 0.5f;

	/** Frequency multiplier between octaves. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Perlin Noise", meta = (ClampMin = "1.0", ClampMax = "4.0", UIMin = "1.0", UIMax = "4.0"))
	float Lacunarity = 2.0f;

	/** Noise value at which painted patches begin to appear. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Perlin Noise", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Threshold = 0.5f;

	/** Width of the soft blend around the threshold; zero creates a hard split. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Perlin Noise", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float TransitionWidth = 0.2f;

	/** Base coverage retained where the noise pattern evaluates below the threshold. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Perlin Noise", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float MinimumStrength = 0.0f;
};

/** @brief Expands or contracts each point and distorts its radial perimeter organically. */
USTRUCT(BlueprintType)
struct FRTSLandscapeNoisyRadialPointPaintSettings
{
	GENERATED_BODY()

	/** Smallest seeded multiplier allowed for a point's original local bounds. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Noisy Bounds", meta = (ClampMin = "0.01", UIMin = "0.1", UIMax = "2.0"))
	float BoundsScaleMinimum = 0.8f;

	/** Largest seeded multiplier allowed for a point's original local bounds. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Noisy Bounds", meta = (ClampMin = "0.01", UIMin = "0.1", UIMax = "2.0"))
	float BoundsScaleMaximum = 1.3f;

	/** Maximum fractional distortion of the nominal radial perimeter. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Noisy Bounds", meta = (ClampMin = "0.0", ClampMax = "0.75", UIMin = "0.0", UIMax = "0.75"))
	float BoundaryNoiseAmount = 0.2f;

	/** Directional pattern scale around the perimeter; higher values make more edge variation. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Noisy Bounds", meta = (ClampMin = "0.01", UIMin = "0.1", UIMax = "20.0"))
	float BoundaryNoiseFrequency = 3.0f;

	/** Number of progressively finer layers used to distort the perimeter. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Noisy Bounds", meta = (ClampMin = "1", ClampMax = "6", UIMin = "1", UIMax = "6"))
	int32 BoundaryNoiseOctaves = 2;

	/** Strength retained by each finer perimeter-noise octave. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Noisy Bounds", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float BoundaryNoisePersistence = 0.5f;
};

/** @brief Value-only point paint parameters retained independently of graph-owned settings. */
struct FRTSLandscapeDataPointPaintConfiguration
{
	ERTSLandscapePointPaintMode PaintMode = ERTSLandscapePointPaintMode::Solid;
	FRTSLandscapeRadialPointPaintSettings RadialSettings;
	FRTSLandscapePerlinPointPaintSettings PerlinSettings;
	FRTSLandscapeNoisyRadialPointPaintSettings NoisyRadialSettings;
	FVector2D NoiseOffset = FVector2D::ZeroVector;
};

/**
 * @brief Value-only footprint copied from a PCG point before its graph-owned data expires.
 * The manager projects the oriented local XY bounds into its Landscape-aligned texture.
 */
struct FRTSLandscapeDataPointStamp
{
	FTransform Transform = FTransform::Identity;
	FVector BoundsMin = FVector::ZeroVector;
	FVector BoundsMax = FVector::ZeroVector;
	float Strength = 1.0f;
	FRTSLandscapeDataPointPaintConfiguration PaintConfiguration;
};

/** @brief Mapping values supplied to the Landscape material and CPU rasterizer. */
USTRUCT(BlueprintType)
struct FRTSLandscapeDataTextureMapping
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Landscape Data")
	FVector2D WorldMinimum = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Landscape Data")
	FVector2D WorldSize = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Landscape Data")
	FVector2D InverseWorldSize = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Landscape Data")
	FIntPoint TextureResolution = FIntPoint::ZeroValue;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Landscape Data")
	FVector2D WorldUnitsPerTexel = FVector2D::ZeroVector;

	bool IsValid() const
	{
		return WorldSize.X > UE_KINDA_SMALL_NUMBER
			&& WorldSize.Y > UE_KINDA_SMALL_NUMBER
			&& TextureResolution.X > 0
			&& TextureResolution.Y > 0;
	}
};
