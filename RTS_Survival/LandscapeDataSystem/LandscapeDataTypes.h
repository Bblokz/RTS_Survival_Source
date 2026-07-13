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
