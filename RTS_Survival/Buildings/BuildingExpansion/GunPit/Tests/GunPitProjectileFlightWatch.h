// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;
class UWorld;

/**
 * @brief Test helper: follows a pit gun's first projectile from the muzzle to its impact, so a test can tell a fast,
 * flat direct shot from a lob. Start it at the first shot and sample it every tick until GetIsFlightOver.
 * Used by the gun pit test scenario.
 */
struct FGunPitProjectileFlightWatch
{
	// The pit's own projectile is the moving one nearest the muzzle just after the shot.
	static constexpr float MaximumLaunchDistance = 1000.f;
	static constexpr float LaunchSearchSeconds = 0.5f;
	// Pooled projectiles stop when they hit; anything slower is dormant.
	static constexpr float MinimumFlightSpeed = 300.f;
	static constexpr float MaximumFlightSeconds = 10.f;
	// A direct shot rises less than this fraction of its ground distance above the line from launch to impact; the
	// leFH's low lob rose about a quarter of it.
	static constexpr float MaximumDirectRiseFraction = 0.1f;

	void Start(const float WorldSeconds, const FVector& MuzzleLocation);
	void Sample(const UWorld& World);

	bool GetHasStarted() const { return StartSeconds >= 0.f; }
	bool GetIsFlightOver() const { return bFlightOver; }
	bool GetFlewDirect() const;
	// Nearest the projectile came to the point; max float before it launched.
	float GetClosestApproach(const FVector& Point) const;
	FString Describe() const;

	bool bChecked = false;

private:
	float StartSeconds = -1.f;
	FVector Muzzle = FVector::ZeroVector;
	TWeakObjectPtr<AActor> Projectile;
	TArray<FVector> Path;
	float LaunchSeconds = -1.f;
	float LastSampleSeconds = -1.f;
	float MaxSpeed = 0.f;
	bool bFlightOver = false;

	void FindLaunchedProjectile(const UWorld& World, const float WorldSeconds);
	void FollowProjectile(const float WorldSeconds);
	// Highest point of the path above the straight line from its first to its last point.
	float GetRiseAboveChord() const;
	float GetGroundDistance() const;
	float GetMeanSpeed() const;
};
