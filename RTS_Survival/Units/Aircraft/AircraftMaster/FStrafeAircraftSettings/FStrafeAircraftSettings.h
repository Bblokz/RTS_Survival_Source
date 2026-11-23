#pragma once

#include "CoreMinimal.h"
#include "FStrafeAircraftSettings.generated.h"

namespace AircraftStrafeHeights
{
	// Strafing aircraft come in and cruise at least this high above the strafe's ground before they dive, whatever
	// world height they were spawned at, so the dive has room and the pull-out stays well above the ground (cm).
	inline constexpr float MinCruiseClearanceAboveTarget = 2600.f;
}

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

	// On: every attack run goes over the strafe target until TotalStrafeTime ends. Off: after the first run the
	// aircraft hunts nearby enemies on its own (aggressive stance) for the rest of the time.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bRepeatRunsOverTarget = false;
};
