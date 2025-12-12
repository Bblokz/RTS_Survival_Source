#pragma once

#include "CoreMinimal.h"

#include "GeneratedRotationBounds.generated.h"

USTRUCT(BlueprintType)
struct FGeneratedRotationBounds
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generated Rotation Bounds")
	FRotator MinRotationXY = FRotator::ZeroRotator;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generated Rotation Bounds")
	FRotator MaxRotationXY = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generated Rotation Bounds")
	FRotator MinRotationZ = FRotator::ZeroRotator;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generated Rotation Bounds")
	FRotator MaxRotationZ = FRotator::ZeroRotator;
};