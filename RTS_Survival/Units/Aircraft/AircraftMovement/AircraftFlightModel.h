// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

struct FAircraftFlightModelSettings;

/** What the aircraft wants this frame: where to aim and how fast to fly. */
struct RTS_SURVIVAL_API FAircraftFlightInput
{
	FVector Location = FVector::ZeroVector;
	// Flight direction and speed now; a (near) zero velocity flies off along CurrentRotation's yaw.
	FVector Velocity = FVector::ZeroVector;
	FRotator CurrentRotation = FRotator::ZeroRotator;
	FVector AimPoint = FVector::ZeroVector;
	float WantedSpeed = 0.f;
	// Cruise speed; the bank is the turn's sideways acceleration as a share of the hardest turn at this speed.
	float ReferenceSpeed = 0.f;
	// Speed gain toward a higher wanted speed (cm/s²).
	float Acceleration = 0.f;
	// Speed loss toward a lower wanted speed when set (cm/s²); otherwise the settings' SpeedDecreaseRate.
	TOptional<float> Deceleration;
	// Extra nose angle while terrain following moves the whole flight up or down (degrees, up positive); visual only.
	float ClimbPitch = 0.f;
	// The flight direction does not descend toward this height; only set where a dive must stay clear of the ground.
	TOptional<float> FloorZ;
	// Cruise and idle flight: climb or descend to the aim point's height at an eased vertical speed instead of
	// pointing the nose at it (which overshoots after a pull-out). Off in dives, which aim at their points.
	bool bHoldAltitude = false;
	float DeltaTime = 0.f;
};

/** The flight after one frame. */
struct RTS_SURVIVAL_API FAircraftFlightOutput
{
	FVector Velocity = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
};

/**
 * @brief Bank-to-turn flight for move, attack and idle flights. The flight direction turns toward the aim point at a
 * capped turn rate that itself builds up and eases off, so turns roll in and out instead of snapping; the speed is kept
 * through the turn (minus a small, tunable loss) and the nose and bank always follow the flight direction.
 * Pure math so it can be tested without a world; the aircraft movement component owns one per aircraft.
 */
struct RTS_SURVIVAL_API FAircraftFlightModel
{
	/** @brief Forget the turn and pitch rates, for instance after a vertical take-off or a straight run. */
	void Reset();

	/**
	 * @brief Advances the flight by one step; the caller splits long frames into short steps.
	 * @param Input Aim, wanted speed and the current flight.
	 * @param Settings Turn, pitch, bank and speed limits.
	 * @return New velocity and actor rotation.
	 */
	FAircraftFlightOutput Step(const FAircraftFlightInput& Input, const FAircraftFlightModelSettings& Settings);

	float GetYawRate() const { return M_YawRate; }
	float GetPitchRate() const { return M_PitchRate; }

	/** @return Radius of the tightest turn at this speed (cm). */
	static float GetTurnRadius(float Speed, const FAircraftFlightModelSettings& Settings);

private:
	// Current heading change of the flight direction (deg/s, positive turns right); changes at most by the turn rate
	// acceleration, which is what makes a turn roll in smoothly.
	float M_YawRate = 0.f;
	// Current climb angle change of the flight direction (deg/s, positive climbs).
	float M_PitchRate = 0.f;

	/** @return The pitch the flight direction may have at most downward, given the floor (degrees, negative). */
	static float GetLowestPitchAboveFloor(const FAircraftFlightInput& Input, float Speed,
	                                      const FAircraftFlightModelSettings& Settings);

	/** @return Pitch (degrees) that climbs or descends to the aim height at an eased vertical speed. */
	static float GetAltitudeHoldPitch(const FAircraftFlightInput& Input, float Speed,
	                                  const FAircraftFlightModelSettings& Settings);

	/**
	 * @return The rate that reaches the wanted angle without overshooting it: proportional when close, never faster
	 * than braking at the rate acceleration allows, and never above the cap.
	 */
	static float GetWantedRate(float AngleError, float Gain, float RateAcceleration, float MaxRate);

	float GetNewSpeed(const FAircraftFlightInput& Input, float CurrentSpeed,
	                  const FAircraftFlightModelSettings& Settings) const;

	/** @return The new bank: follows the turn's sideways acceleration, eased and capped in roll rate. */
	float GetNewRoll(const FAircraftFlightInput& Input, float Speed, const FAircraftFlightModelSettings& Settings) const;
};
