// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

struct FAircraftTerrainFollowSettings;

/**
 * @brief Terrain following of one airborne aircraft. The aircraft feeds ground samples at SampleInterval and asks every
 * frame how far its flight height moves; the clearance above the ground is captured once after each ResetClearance.
 * Pure math so it can be tested without a world.
 */
struct RTS_SURVIVAL_API FAircraftTerrainFollow
{
	/** @brief The flight height was set by something else; the next ground sample captures the clearance anew. */
	void ResetClearance();

	/** @brief The aircraft must keep its height for now (dive, bomb run): stops the vertical motion at once. */
	void Suspend();

	/** @brief Spreads the samples of many aircraft over different frames. */
	void SetSamplePhase(const float SecondsUntilFirstSample);

	/** @return Whether a ground sample is due this frame; counts the sample timer down and restarts it. */
	bool TickSampleTimer(const float DeltaTime, const float SampleInterval);

	/**
	 * @brief Takes the clearance against a known ground height instead of the ground below the aircraft; ability
	 * flights use the ground at their target, as their height was chosen for the target.
	 * @param GroundZ Ground the clearance is measured from.
	 * @param CurrentFlightZ The aircraft's current cruise height.
	 * @param MinClearance Floor of the clearance.
	 */
	void CaptureClearance(const float GroundZ, const float CurrentFlightZ, const float MinClearance);

	/**
	 * @brief The first sample after a reset captures the clearance; later samples set the height to fly at.
	 * @param GroundBelowZ Ground right below the aircraft; defines the clearance.
	 * @param HighestGroundZ Highest ground below and ahead of the aircraft; the clearance is kept above this.
	 * @param CurrentFlightZ The aircraft's current cruise height.
	 * @param MinClearance Floor of the clearance.
	 */
	void OnGroundSampled(const float GroundBelowZ, const float HighestGroundZ, const float CurrentFlightZ,
	                     const float MinClearance);

	/**
	 * @brief Eases the cruise height toward the sampled height; speed and acceleration are capped and the target is
	 * never overshot, so the aircraft never jumps up or down.
	 * @param DeltaTime Frame time.
	 * @param CurrentFlightZ The aircraft's current cruise height.
	 * @param Settings Speed, acceleration and dead-zone limits.
	 * @return How far the cruise height moves this frame (cm, up positive).
	 */
	float AdvanceFlightHeight(const float DeltaTime, const float CurrentFlightZ,
	                          const FAircraftTerrainFollowSettings& Settings);

	/**
	 * @brief Nose angle that matches the current climb, so the climb / dive animations play while following terrain.
	 * @param HorizontalSpeed Speed over the ground (cm/s).
	 * @param MaxClimbPitch Clamp (degrees).
	 * @return Pitch in degrees, positive is nose up.
	 */
	float GetClimbPitch(const float HorizontalSpeed, const float MaxClimbPitch) const;

	float GetVerticalSpeed() const { return M_VerticalSpeed; }
	bool GetHasClearance() const { return bM_HasClearance; }
	float GetClearance() const { return M_Clearance; }
	float GetDesiredFlightZ() const { return M_DesiredFlightZ; }

private:
	// Height above the ground below the aircraft at the moment its cruise height was set; kept over all terrain.
	float M_Clearance = 0.f;

	// Cruise height wanted by the last ground sample; only valid once bM_HasDesiredFlightZ.
	float M_DesiredFlightZ = 0.f;

	// Current vertical speed of the cruise height (cm/s), changed at most by the maximum acceleration.
	float M_VerticalSpeed = 0.f;

	float M_TimeUntilNextSample = 0.f;
	bool bM_HasClearance = false;
	bool bM_HasDesiredFlightZ = false;
};
