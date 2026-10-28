// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

/** Everything the free-flight solve needs; the random jitter and wobble are rolled by the caller. */
struct FRailRocketArcRequest
{
	FVector RailExitLocation = FVector::ZeroVector;
	FVector RailDirection = FVector::ForwardVector;
	FVector TargetLocation = FVector::ZeroVector;
	// Positive magnitude of the world gravity in cm/s^2.
	float WorldGravity = 0.0f;
	float ArcGravityScale = 1.0f;
	float MinElevationDegrees = 0.0f;
	float MaxElevationDegrees = 0.0f;
	// Rolled elevation jitter; the solve still lands on the target with it.
	float ElevationOffsetDegrees = 0.0f;
	// The rail exit speed: a slower solved arc gets more gravity instead, so the motor never seems to brake.
	float MinLaunchSpeed = 0.0f;
	// Above this the arc is rejected and the caller falls back to the generic arced launch.
	float MaxLaunchSpeed = 0.0f;
	// Rolled rail-exit wobble; applied after the solve so it disperses the impact like a real fin-stabilised rocket.
	float ExitYawWobbleDegrees = 0.0f;
	float ExitPitchWobbleDegrees = 0.0f;
};

/** Result of a successful free-flight solve. */
struct FRailRocketFreeFlightSolution
{
	FVector LaunchVelocity = FVector::ZeroVector;
	FVector HorizontalDirection = FVector::ZeroVector;
	// Seconds until the rocket is back at the target height; drives the timed explosion.
	float FlightTime = 0.0f;
	// ProjectileGravityScale to use during the arc (world gravity * this = the gravity the solve used).
	float GravityScale = 1.0f;
	// Positive magnitude of the gravity used by the solve in cm/s^2.
	float EffectiveGravity = 0.0f;
	float ElevationDegrees = 0.0f;
};

/**
 * @brief Pure ballistic math for rail-launched rockets, kept free of any world so automation tests can prove the
 * arc that leaves the rail at a fixed elevation lands on its target. Used by AProjectile::SetupRailRocketLaunch.
 */
struct RTS_SURVIVAL_API FRailRocketArcSolver
{
	/**
	 * @brief Solves the launch speed for a fixed elevation: v^2 = g d^2 / (2 cos^2(theta) (d tan(theta) - dz)).
	 * @param HorizontalDistance Flat distance from launch to target in cm.
	 * @param HeightDelta Target height minus launch height in cm.
	 * @param ElevationDegrees Launch elevation above the horizon.
	 * @param Gravity Positive gravity magnitude in cm/s^2.
	 * @param OutLaunchSpeed Speed along the elevated launch direction.
	 * @param OutFlightTime Seconds until the target is reached: d / (v cos(theta)).
	 * @return False when no arc exists at this elevation: target above the elevation line, too close, or bad input.
	 */
	static bool SolveFixedElevationLaunch(
		const float HorizontalDistance,
		const float HeightDelta,
		const float ElevationDegrees,
		const float Gravity,
		float& OutLaunchSpeed,
		float& OutFlightTime);

	/**
	 * @brief Builds the full free-flight arc from the rail exit: clamps the rail elevation, solves the speed, keeps
	 * at least the rail speed by raising gravity (same path, faster) and applies the rolled exit wobble.
	 * @param Request Rail exit, target, gravity and designer limits with the jitter already rolled.
	 * @param OutSolution Velocity, flight time and gravity scale for the projectile movement.
	 * @return False when no arc exists or it needs more than MaxLaunchSpeed; the caller must use a fallback launch.
	 */
	static bool SolveFreeFlight(const FRailRocketArcRequest& Request, FRailRocketFreeFlightSolution& OutSolution);

	/**
	 * @brief Seconds until a ballistic shot falls back to a height, used after the wobble changed the elevation.
	 * @param VerticalSpeed Initial vertical speed in cm/s.
	 * @param HeightDelta Target height minus launch height in cm.
	 * @param Gravity Positive gravity magnitude in cm/s^2.
	 * @param OutTime Time of the descending crossing of HeightDelta.
	 * @return False when the shot never reaches that height.
	 */
	static bool GetTimeToFallToHeight(const float VerticalSpeed, const float HeightDelta, const float Gravity,
	                                  float& OutTime);
};
