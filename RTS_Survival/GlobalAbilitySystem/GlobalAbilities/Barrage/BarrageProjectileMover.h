// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "BarrageProjectileMover.generated.h"

USTRUCT(BlueprintType)
struct FBarrageProjectileMover
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ProjectileArcSpeed = 6000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ProjectileLinearSpeed = 4000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ArcStrength = 0.5f;
};
