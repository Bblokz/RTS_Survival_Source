// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GunPitProjectileFlightWatch.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Engine/World.h"
#include "EngineUtils.h"
#include "RTS_Survival/Weapons/Projectile/Projectile.h"

namespace GunPitProjectileFlightWatchPrivate
{
	// A pooled projectile sent back to its pool jumps much further than one tick of flight.
	constexpr float MaximumTickTravelMultiplier = 3.f;
	constexpr float TickTravelSlack = 500.f;
	constexpr int32 MinimumPathSamples = 3;
}

void FGunPitProjectileFlightWatch::Start(const float WorldSeconds, const FVector& MuzzleLocation)
{
	StartSeconds = WorldSeconds;
	Muzzle = MuzzleLocation;
}

void FGunPitProjectileFlightWatch::Sample(const UWorld& World)
{
	if (not GetHasStarted() || bFlightOver)
	{
		return;
	}
	const float WorldSeconds = World.GetTimeSeconds();
	const bool bHasLaunched = LaunchSeconds >= 0.f;
	if (not bHasLaunched)
	{
		FindLaunchedProjectile(World, WorldSeconds);
		return;
	}
	if (not Projectile.IsValid())
	{
		bFlightOver = true;
		return;
	}
	FollowProjectile(WorldSeconds);
}

void FGunPitProjectileFlightWatch::FindLaunchedProjectile(const UWorld& World, const float WorldSeconds)
{
	AProjectile* Nearest = nullptr;
	float NearestDistance = MaximumLaunchDistance;
	for (TActorIterator<AProjectile> It(&World); It; ++It)
	{
		AProjectile* Candidate = *It;
		const float Distance = FVector::Dist(Candidate->GetActorLocation(), Muzzle);
		if (Candidate->GetVelocity().Size() < MinimumFlightSpeed || Distance >= NearestDistance)
		{
			continue;
		}
		Nearest = Candidate;
		NearestDistance = Distance;
	}
	if (not Nearest)
	{
		bFlightOver = WorldSeconds - StartSeconds > LaunchSearchSeconds;
		return;
	}
	Projectile = Nearest;
	LaunchSeconds = WorldSeconds;
	LastSampleSeconds = WorldSeconds;
	MaxSpeed = Nearest->GetVelocity().Size();
	Path.Add(Nearest->GetActorLocation());
}

void FGunPitProjectileFlightWatch::FollowProjectile(const float WorldSeconds)
{
	using namespace GunPitProjectileFlightWatchPrivate;
	const FVector Location = Projectile->GetActorLocation();
	const float Speed = Projectile->GetVelocity().Size();
	const float TickSeconds = WorldSeconds - LastSampleSeconds;
	const float MaximumTickTravel = MaxSpeed * TickSeconds * MaximumTickTravelMultiplier + TickTravelSlack;
	const bool bWasPooled = Speed < MinimumFlightSpeed || FVector::Dist(Location, Path.Last()) > MaximumTickTravel;
	if (bWasPooled || WorldSeconds - LaunchSeconds > MaximumFlightSeconds)
	{
		bFlightOver = true;
		return;
	}
	Path.Add(Location);
	LastSampleSeconds = WorldSeconds;
	MaxSpeed = FMath::Max(MaxSpeed, Speed);
}

bool FGunPitProjectileFlightWatch::GetFlewDirect() const
{
	const float GroundDistance = GetGroundDistance();
	return Path.Num() >= GunPitProjectileFlightWatchPrivate::MinimumPathSamples
		&& GroundDistance > 0.f
		&& GetRiseAboveChord() < GroundDistance * MaximumDirectRiseFraction;
}

float FGunPitProjectileFlightWatch::GetRiseAboveChord() const
{
	if (Path.Num() < 2)
	{
		return 0.f;
	}
	const FVector Launch = Path[0];
	const FVector Impact = Path.Last();
	const FVector Chord2D = FVector(Impact.X - Launch.X, Impact.Y - Launch.Y, 0.f);
	const float ChordLengthSquared = Chord2D.SizeSquared();
	if (ChordLengthSquared <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}
	float MaxRise = 0.f;
	for (const FVector& Point : Path)
	{
		const FVector FromLaunch2D = FVector(Point.X - Launch.X, Point.Y - Launch.Y, 0.f);
		const float Along = FMath::Clamp(FVector::DotProduct(FromLaunch2D, Chord2D) / ChordLengthSquared, 0.f, 1.f);
		const float ChordHeight = FMath::Lerp(Launch.Z, Impact.Z, Along);
		MaxRise = FMath::Max(MaxRise, Point.Z - ChordHeight);
	}
	return MaxRise;
}

float FGunPitProjectileFlightWatch::GetClosestApproach(const FVector& Point) const
{
	float Closest = TNumericLimits<float>::Max();
	for (int32 Index = 1; Index < Path.Num(); ++Index)
	{
		const FVector OnSegment = FMath::ClosestPointOnSegment(Point, Path[Index - 1], Path[Index]);
		Closest = FMath::Min(Closest, FVector::Dist(OnSegment, Point));
	}
	return Closest;
}

float FGunPitProjectileFlightWatch::GetGroundDistance() const
{
	return Path.Num() < 2 ? 0.f : FVector::Dist2D(Path[0], Path.Last());
}

float FGunPitProjectileFlightWatch::GetMeanSpeed() const
{
	const float FlightSeconds = LastSampleSeconds - LaunchSeconds;
	if (Path.Num() < 2 || FlightSeconds <= 0.f)
	{
		return 0.f;
	}
	float PathLength = 0.f;
	for (int32 Index = 1; Index < Path.Num(); ++Index)
	{
		PathLength += FVector::Dist(Path[Index - 1], Path[Index]);
	}
	return PathLength / FlightSeconds;
}

FString FGunPitProjectileFlightWatch::Describe() const
{
	return FString::Printf(
		TEXT("projectile=%s samples=%d flight_s=%.2f ground_distance=%.0f rise_above_line=%.0f mean_speed=%.0f "
			"max_speed=%.0f launch=%s impact=%s"),
		*GetNameSafe(Projectile.Get()), Path.Num(), LastSampleSeconds - LaunchSeconds, GetGroundDistance(),
		GetRiseAboveChord(), GetMeanSpeed(), MaxSpeed, Path.IsEmpty() ? TEXT("none") : *Path[0].ToCompactString(),
		Path.IsEmpty() ? TEXT("none") : *Path.Last().ToCompactString());
}

#endif
