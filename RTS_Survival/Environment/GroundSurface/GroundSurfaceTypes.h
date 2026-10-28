// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GroundSurfaceTypes.generated.h"

/**
 * @brief Kind of ground under a point of the map, baked per grid cell into UGroundSurfaceMap and read at runtime
 * through UGroundSurfaceSubsystem, for example to tint the track smoke of vehicles.
 */
UENUM(BlueprintType)
enum class EGroundSurfaceType : uint8
{
	// No baked data here: no landscape and no surface mesh, or the map has no surface map.
	None,
	Dirt,
	Grass,
	Sand,
	Rock,
	Concrete,
	Num UMETA(Hidden)
};

/** @brief How the track smoke of a vehicle looks while it drives over one kind of ground. */
USTRUCT(BlueprintType)
struct FGroundSurfaceSmokeLook
{
	GENERATED_BODY()

	// Multiplies the smoke texture in M_TrackSmoke (emissive), so values above 1 brighten it.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Smoke")
	FLinearColor SmokeTint = FLinearColor(0.88f, 0.682f, 0.55f, 1.f);

	// Scales how many smoke particles spawn; 1 is the look tuned on dirt.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Smoke", meta=(ClampMin="0", ClampMax="3"))
	float SmokeDensity = 1.f;
};
