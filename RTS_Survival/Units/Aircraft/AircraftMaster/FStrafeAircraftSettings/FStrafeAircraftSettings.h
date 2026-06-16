#pragma once

#include "CoreMinimal.h"

#include "FStrafeAircraftSettings.generated.h"

USTRUCT(BlueprintType)
struct FStrafeAircraftSettings
{
	GENERATED_BODY()
	FStrafeAircraftSettings() = default;

	FStrafeAircraftSettings(
		const FVector& InStrafeStartLocation,
		const FVector& InStrafeEndLocation,
		const float InStrafePointTotalLerpTime,
		const float InTotalStrafeTime,
		const FVector& InPostStrafeMoveToLocation)
		: StrafeStartLocation(InStrafeStartLocation)
		, StrafeEndLocation(InStrafeEndLocation)
		, StrafePointTotalLerpTime(InStrafePointTotalLerpTime)
		, TotalStrafeTime(InTotalStrafeTime)
		, PostStrafeMoveToLocation(InPostStrafeMoveToLocation)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector StrafeStartLocation = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector StrafeEndLocation = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float StrafePointTotalLerpTime = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TotalStrafeTime = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector PostStrafeMoveToLocation = FVector::ZeroVector;
};
