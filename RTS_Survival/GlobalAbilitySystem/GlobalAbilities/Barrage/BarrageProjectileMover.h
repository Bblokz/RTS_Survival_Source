// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "BarrageProjectileMover.generated.h"

/**
 * Flight of a barrage shell: a long-range gun far off the map (railway artillery) whose shells come over the horizon
 * on a ballistic arc and plunge onto the target. Only the last part of the arc is flown; it starts high above the
 * camera so the shell streaks in from off screen.
 */
USTRUCT(BlueprintType)
struct FBarrageProjectileMover
{
	GENERATED_BODY()

	// Legacy: the old steered path. Not used any more; kept so the saved templates keep loading the same.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legacy")
	float ProjectileArcSpeed = 6000.0f;

	// Legacy: the old steered path. Not used any more; kept so the saved templates keep loading the same.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legacy")
	float ProjectileLinearSpeed = 4000.0f;

	// Legacy: the old steered path. Not used any more; kept so the saved templates keep loading the same.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Legacy")
	float ArcStrength = 0.5f;

	// Speed at impact, before the global projectile speed multiplier.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Descent", meta = (ClampMin = "1000.0", Units = "cm/s"))
	float ImpactSpeed = 9000.0f;

	// Angle below the horizon at impact; each shell rolls between min and max.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Descent", meta = (ClampMin = "10.0", ClampMax = "89.0"))
	float MinDescentAngleDegrees = 58.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Descent", meta = (ClampMin = "10.0", ClampMax = "89.0"))
	float MaxDescentAngleDegrees = 68.0f;

	// How long the visible part of the flight lasts; longer starts the shell higher and further away.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Descent", meta = (ClampMin = "0.2", Units = "s"))
	float FlightTimeSeconds = 1.1f;

	// Gravity on the shell on top of the arcing-shell gravity of all projectiles; higher bends the arc more.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Descent", meta = (ClampMin = "0.0"))
	float GravityMultiplier = 2.0f;

	// All shells of one barrage come from one battery; each shell deviates this much from its bearing.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Descent", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float BearingSpreadDegrees = 6.0f;
};

/** Where a barrage shell starts and how it is thrown so that it lands on its aim point. */
struct FBarrageShellFlight
{
	FVector LaunchLocation = FVector::ZeroVector;
	FVector LaunchVelocity = FVector::ZeroVector;
	// Scale on the world gravity for the projectile movement component.
	float GravityScale = 0.0f;
	float FlightTimeSeconds = 0.0f;
};

namespace BarrageShellFlight
{
	/**
	 * @brief Solves the descending part of a ballistic arc backwards from the impact.
	 * @param AimPoint Where the shell lands.
	 * @param FlightYawDegrees Horizontal direction the shell flies in.
	 * @param DescentAngleDegrees Angle below the horizon at impact.
	 * @param ImpactSpeed Speed at impact in cm/s.
	 * @param FlightTimeSeconds Time from the start of the flown part to impact.
	 * @param WorldGravityZ The world's gravity (negative is down).
	 * @param GravityScale Scale on the world gravity the shell flies with.
	 * @return Start location and velocity that land on the aim point after the flight time.
	 */
	inline FBarrageShellFlight Solve(
		const FVector& AimPoint,
		const float FlightYawDegrees,
		const float DescentAngleDegrees,
		const float ImpactSpeed,
		const float FlightTimeSeconds,
		const float WorldGravityZ,
		const float GravityScale)
	{
		const float Yaw = FMath::DegreesToRadians(FlightYawDegrees);
		const float Pitch = FMath::DegreesToRadians(DescentAngleDegrees);
		const FVector ImpactVelocity(
			FMath::Cos(Yaw) * FMath::Cos(Pitch) * ImpactSpeed,
			FMath::Sin(Yaw) * FMath::Cos(Pitch) * ImpactSpeed,
			-FMath::Sin(Pitch) * ImpactSpeed);
		const FVector Acceleration(0.0f, 0.0f, WorldGravityZ * GravityScale);
		const float Time = FMath::Max(FlightTimeSeconds, UE_KINDA_SMALL_NUMBER);

		// Run the flight backwards from the impact: p(T - t) = P - v(T) t + a t^2 / 2 and v(0) = v(T) - a T.
		FBarrageShellFlight Flight;
		Flight.LaunchLocation = AimPoint - ImpactVelocity * Time + 0.5f * Acceleration * Time * Time;
		Flight.LaunchVelocity = ImpactVelocity - Acceleration * Time;
		Flight.GravityScale = GravityScale;
		Flight.FlightTimeSeconds = Time;
		return Flight;
	}
}
