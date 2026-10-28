// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "RailRocketArcSolver.h"

namespace RailRocketArcSolverPrivate
{
	// Below this flat distance the target is straight above or below the rail exit: no arc to aim.
	constexpr float MinHorizontalDistanceCm = 1.0f;
	// Near-vertical elevations make the horizontal speed vanish.
	constexpr float MinCosElevation = 0.01f;
	// The target must sit at least this far below the line along the launch elevation.
	constexpr float MinClearanceBelowElevationLineCm = 1.0f;
	constexpr float MinSpeedForRatio = 1.0f;

	/**
	 * @brief The arc leaves along the rail's own elevation so the rocket curves over without a visible kink; only a
	 * rail outside the designer limits is clamped, then the rolled jitter is added within the limits.
	 */
	float GetArcElevationDegrees(const FVector& RailDirection, const FRailRocketArcRequest& Request)
	{
		const float RailSinElevation = FMath::Clamp(static_cast<float>(RailDirection.Z), -1.0f, 1.0f);
		const float RailElevationDegrees = FMath::RadiansToDegrees(FMath::Asin(RailSinElevation));
		const float LowestElevation = FMath::Min(Request.MinElevationDegrees, Request.MaxElevationDegrees);
		const float HighestElevation = FMath::Max(Request.MinElevationDegrees, Request.MaxElevationDegrees);
		const float ClampedRailElevation = FMath::Clamp(RailElevationDegrees, LowestElevation, HighestElevation);
		return FMath::Clamp(ClampedRailElevation + Request.ElevationOffsetDegrees, LowestElevation, HighestElevation);
	}
}

bool FRailRocketArcSolver::SolveFixedElevationLaunch(
	const float HorizontalDistance,
	const float HeightDelta,
	const float ElevationDegrees,
	const float Gravity,
	float& OutLaunchSpeed,
	float& OutFlightTime)
{
	using namespace RailRocketArcSolverPrivate;
	if (HorizontalDistance < MinHorizontalDistanceCm || Gravity <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float ElevationRadians = FMath::DegreesToRadians(ElevationDegrees);
	const float CosElevation = FMath::Cos(ElevationRadians);
	if (CosElevation < MinCosElevation)
	{
		return false;
	}

	// d tan(theta) - dz: how far the target lies below the straight line along the launch elevation.
	const float ClearanceBelowElevationLine = HorizontalDistance * FMath::Tan(ElevationRadians) - HeightDelta;
	if (ClearanceBelowElevationLine < MinClearanceBelowElevationLineCm)
	{
		return false;
	}

	const float SpeedSquared = (Gravity * HorizontalDistance * HorizontalDistance)
		/ (2.0f * CosElevation * CosElevation * ClearanceBelowElevationLine);
	OutLaunchSpeed = FMath::Sqrt(SpeedSquared);
	if (OutLaunchSpeed <= KINDA_SMALL_NUMBER)
	{
		return false;
	}
	OutFlightTime = HorizontalDistance / (OutLaunchSpeed * CosElevation);
	return true;
}

bool FRailRocketArcSolver::SolveFreeFlight(const FRailRocketArcRequest& Request,
                                           FRailRocketFreeFlightSolution& OutSolution)
{
	using namespace RailRocketArcSolverPrivate;
	const FVector RailDirection = Request.RailDirection.GetSafeNormal();
	if (RailDirection.IsNearlyZero() || Request.WorldGravity <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FVector ExitToTarget = Request.TargetLocation - Request.RailExitLocation;
	const FVector FlatExitToTarget(ExitToTarget.X, ExitToTarget.Y, 0.0f);
	const float HorizontalDistance = static_cast<float>(FlatExitToTarget.Size());
	const FVector HorizontalDirection = FlatExitToTarget.GetSafeNormal();
	if (HorizontalDirection.IsNearlyZero())
	{
		return false;
	}

	const float ElevationDegrees = GetArcElevationDegrees(RailDirection, Request);
	float Gravity = Request.WorldGravity * FMath::Max(Request.ArcGravityScale, KINDA_SMALL_NUMBER);
	float LaunchSpeed = 0.0f;
	float FlightTime = 0.0f;
	const float HeightDelta = static_cast<float>(ExitToTarget.Z);
	if (not SolveFixedElevationLaunch(HorizontalDistance, HeightDelta, ElevationDegrees, Gravity, LaunchSpeed,
	                                  FlightTime))
	{
		return false;
	}
	if (Request.MaxLaunchSpeed > 0.0f && LaunchSpeed > Request.MaxLaunchSpeed)
	{
		return false;
	}
	if (LaunchSpeed < Request.MinLaunchSpeed)
	{
		// At a fixed elevation v^2 scales with g, so more gravity keeps the exact same path at the rail speed.
		const float SpeedRatio = Request.MinLaunchSpeed / FMath::Max(LaunchSpeed, MinSpeedForRatio);
		Gravity *= SpeedRatio * SpeedRatio;
		LaunchSpeed = Request.MinLaunchSpeed;
		FlightTime /= SpeedRatio;
	}

	const float WobbledElevationRadians = FMath::DegreesToRadians(ElevationDegrees + Request.ExitPitchWobbleDegrees);
	const FVector WobbledHorizontalDirection =
		FRotator(0.0f, Request.ExitYawWobbleDegrees, 0.0f).RotateVector(HorizontalDirection);
	const FVector LaunchVelocity = LaunchSpeed * (FMath::Cos(WobbledElevationRadians) * WobbledHorizontalDirection
		+ FMath::Sin(WobbledElevationRadians) * FVector::UpVector);

	// Pitch wobble changes when the rocket comes down; time the explosion on the wobbled arc to avoid air bursts.
	float WobbledFlightTime = FlightTime;
	if (not FMath::IsNearlyZero(Request.ExitPitchWobbleDegrees)
		&& not GetTimeToFallToHeight(static_cast<float>(LaunchVelocity.Z), HeightDelta, Gravity, WobbledFlightTime))
	{
		WobbledFlightTime = FlightTime;
	}

	OutSolution.LaunchVelocity = LaunchVelocity;
	OutSolution.HorizontalDirection = WobbledHorizontalDirection;
	OutSolution.FlightTime = WobbledFlightTime;
	OutSolution.EffectiveGravity = Gravity;
	OutSolution.GravityScale = Gravity / Request.WorldGravity;
	OutSolution.ElevationDegrees = ElevationDegrees;
	return true;
}

bool FRailRocketArcSolver::GetTimeToFallToHeight(const float VerticalSpeed, const float HeightDelta,
                                                 const float Gravity, float& OutTime)
{
	if (Gravity <= KINDA_SMALL_NUMBER)
	{
		return false;
	}
	const float Discriminant = VerticalSpeed * VerticalSpeed - 2.0f * Gravity * HeightDelta;
	if (Discriminant < 0.0f)
	{
		return false;
	}
	const float DescendingTime = (VerticalSpeed + FMath::Sqrt(Discriminant)) / Gravity;
	if (DescendingTime <= KINDA_SMALL_NUMBER)
	{
		return false;
	}
	OutTime = DescendingTime;
	return true;
}
