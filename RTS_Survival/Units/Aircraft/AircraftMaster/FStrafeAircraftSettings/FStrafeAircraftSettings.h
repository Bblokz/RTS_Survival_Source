#pragma once

#include "CoreMinimal.h"

struct FStrafeAircraftSettings
{
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

	FVector StrafeStartLocation = FVector::ZeroVector;
	FVector StrafeEndLocation = FVector::ZeroVector;
	float StrafePointTotalLerpTime = 0.f;
	float TotalStrafeTime = 0.f;
	FVector PostStrafeMoveToLocation = FVector::ZeroVector;
};
