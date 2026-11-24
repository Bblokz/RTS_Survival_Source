// Copyright (C) Bas Blokzijl - All rights reserved.

#include "AircraftTerrainFollow.h"

#include "RTS_Survival/Units/Aircraft/AircraftState/AircraftState.h"

namespace AircraftTerrainFollowConstants
{
	// A moving aircraft keeps easing until it is this close to the sampled height (cm).
	constexpr float SettleTolerance = 1.f;
	// Below this vertical speed the cruise height counts as level, and the dead zone applies again (cm/s).
	constexpr float LevelVerticalSpeed = 1.f;
	// Slow or hovering aircraft would otherwise point their nose almost straight up for a small climb (cm/s).
	constexpr float MinHorizontalSpeedForPitch = 500.f;

	/**
	 * @return The fastest speed from which slowing down by MaxAcceleration every frame still stops within
	 * RemainingDistance; the frame-exact form of sqrt(2 a d), so the climb ends without a last-frame snap to zero.
	 */
	float GetBrakingSpeed(const float RemainingDistance, const float MaxAcceleration, const float DeltaTime)
	{
		const float SpeedStepPerFrame = MaxAcceleration * DeltaTime;
		return SpeedStepPerFrame * (FMath::Sqrt(2.f * RemainingDistance / (SpeedStepPerFrame * DeltaTime) + 0.25f)
			- 0.5f);
	}
}

void FAircraftTerrainFollow::ResetClearance()
{
	bM_HasClearance = false;
	bM_HasDesiredFlightZ = false;
	M_VerticalSpeed = 0.f;
	// Capture the new clearance on the next frame the aircraft may follow the terrain.
	M_TimeUntilNextSample = 0.f;
}

void FAircraftTerrainFollow::Suspend()
{
	M_VerticalSpeed = 0.f;
}

void FAircraftTerrainFollow::SetSamplePhase(const float SecondsUntilFirstSample)
{
	M_TimeUntilNextSample = FMath::Max(0.f, SecondsUntilFirstSample);
}

bool FAircraftTerrainFollow::TickSampleTimer(const float DeltaTime, const float SampleInterval)
{
	M_TimeUntilNextSample -= DeltaTime;
	if (M_TimeUntilNextSample > 0.f)
	{
		return false;
	}
	M_TimeUntilNextSample = FMath::Max(SampleInterval, KINDA_SMALL_NUMBER);
	return true;
}

void FAircraftTerrainFollow::CaptureClearance(const float GroundZ, const float CurrentFlightZ,
                                              const float MinClearance)
{
	M_Clearance = FMath::Max(MinClearance, CurrentFlightZ - GroundZ);
	bM_HasClearance = true;
	// Stay at the current height until the first sample says what the ground under the aircraft asks for.
	M_DesiredFlightZ = CurrentFlightZ;
	bM_HasDesiredFlightZ = true;
}

void FAircraftTerrainFollow::OnGroundSampled(const float GroundBelowZ, const float HighestGroundZ,
                                             const float CurrentFlightZ, const float MinClearance)
{
	if (not bM_HasClearance)
	{
		CaptureClearance(GroundBelowZ, CurrentFlightZ, MinClearance);
		return;
	}
	M_DesiredFlightZ = FMath::Max(GroundBelowZ, HighestGroundZ) + M_Clearance;
	bM_HasDesiredFlightZ = true;
}

float FAircraftTerrainFollow::AdvanceFlightHeight(const float DeltaTime, const float CurrentFlightZ,
                                                  const FAircraftTerrainFollowSettings& Settings)
{
	using namespace AircraftTerrainFollowConstants;
	if (not bM_HasDesiredFlightZ || DeltaTime <= 0.f)
	{
		M_VerticalSpeed = 0.f;
		return 0.f;
	}

	const float HeightError = M_DesiredFlightZ - CurrentFlightZ;
	const float AbsHeightError = FMath::Abs(HeightError);
	const float MaxAcceleration = FMath::Max(Settings.MaxVerticalAcceleration, KINDA_SMALL_NUMBER);
	const bool bIsLevel = FMath::Abs(M_VerticalSpeed) < LevelVerticalSpeed;
	// The dead zone only keeps a level aircraft level; a climbing one finishes its climb.
	const float StartMoveDistance = bIsLevel ? Settings.HeightDeadZone : SettleTolerance;

	// The fastest speed from which the aircraft can still brake to a stop at the sampled height.
	const float BrakingSpeed = GetBrakingSpeed(AbsHeightError, MaxAcceleration, DeltaTime);
	float WantedSpeed = 0.f;
	if (AbsHeightError > StartMoveDistance)
	{
		WantedSpeed = FMath::Sign(HeightError) * FMath::Min(Settings.MaxVerticalSpeed, BrakingSpeed);
	}
	M_VerticalSpeed = FMath::FInterpConstantTo(M_VerticalSpeed, WantedSpeed, DeltaTime, MaxAcceleration);
	if (M_VerticalSpeed * HeightError > 0.f)
	{
		// Never faster than the braking speed while closing in, so the speed reaches zero together with the height.
		M_VerticalSpeed = FMath::Sign(M_VerticalSpeed) * FMath::Min(FMath::Abs(M_VerticalSpeed), BrakingSpeed);
	}

	const float HeightStep = M_VerticalSpeed * DeltaTime;
	const bool bIsMovingTowardTarget = HeightStep * HeightError > 0.f;
	if (bIsMovingTowardTarget && FMath::Abs(HeightStep) >= AbsHeightError)
	{
		// Arrives this frame: land exactly on the sampled height.
		M_VerticalSpeed = 0.f;
		return HeightError;
	}
	return HeightStep;
}

float FAircraftTerrainFollow::GetClimbPitch(const float HorizontalSpeed, const float MaxClimbPitch) const
{
	using namespace AircraftTerrainFollowConstants;
	const float GroundSpeed = FMath::Max(HorizontalSpeed, MinHorizontalSpeedForPitch);
	const float PitchDegrees = FMath::RadiansToDegrees(FMath::Atan2(M_VerticalSpeed, GroundSpeed));
	return FMath::Clamp(PitchDegrees, -MaxClimbPitch, MaxClimbPitch);
}
