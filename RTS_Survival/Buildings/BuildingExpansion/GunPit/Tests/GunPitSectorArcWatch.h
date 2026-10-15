// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * @brief Test helper for a pit whose gun only traverses a fixed arc: ordered onto a target outside the arc, the gun
 * must stop at the arc's edge and hold its fire. Sample it every tick while that order stands, then read it.
 * Used by the gun pit test scenario (-At53KSectorPitValidate).
 */
struct FGunPitSectorArcWatch
{
	// The gun stops this close to the limit (it eases onto it at the turret's rotation speed).
	static constexpr float LimitToleranceDegrees = 2.f;
	// How long the gun is kept on the target outside the arc; the 53-K needs under 2 s to reach the limit.
	static constexpr float HoldSeconds = 8.f;

	/**
	 * @brief Records where the gun points, against the pit's forward, and whether a shot went off.
	 * @param GunYawFromPitForward Gun yaw minus the pit actor's yaw (degrees, -180..180).
	 * @param bShotFired True when the pit fired since the last sample.
	 */
	void Sample(const float GunYawFromPitForward, const bool bShotFired);

	bool GetHoldsAtLimit(const float SectorHalfDegrees) const;
	bool GetStaysInsideArc(const float SectorHalfDegrees) const;
	bool GetHeldFire() const { return ShotsOutsideArc == 0; }
	FString Describe() const;

private:
	float LastGunYawFromPitForward = 0.f;
	float LargestGunYawFromPitForward = 0.f;
	int32 ShotsOutsideArc = 0;
	int32 Samples = 0;
};
