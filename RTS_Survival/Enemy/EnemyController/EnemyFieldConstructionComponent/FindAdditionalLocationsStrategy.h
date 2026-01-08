// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "FindAdditionalLocationsStrategy.generated.h"

UENUM(BlueprintType)
enum class EAdditionalLocationsStrategy : uint8
{
	None, // stop like before; this order is completed.
	ContinueOnClosestRoad, // compute the average location of the squad controllers that are still in the order and find the closest road.
	ContinueOnAreaBetweenCurrentAndEnemyLocation // compute the average location of the squad controllers that are still in the order and find a location between that and the enemy location.
};

USTRUCT(BlueprintType)
struct FFindAddtionalLocationsStrategy
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAdditionalLocationsStrategy Strategy = EAdditionalLocationsStrategy::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector EnemyLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 AmountOfLocationsToFind = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float PointDensityScaler = 1.f;
};
