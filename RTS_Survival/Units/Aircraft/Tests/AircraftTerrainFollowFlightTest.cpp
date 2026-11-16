// Copyright (C) Bas Blokzijl - All rights reserved.

// -AircraftTerrainFollowValidate: an airborne aircraft flies over the bumpiest straight route from the anchor and must
// keep its clearance above the hills and valleys, smoothly; then an attack-ground dive into the lowest point of the
// route, which terrain following must not touch (UAircraftTestSubsystem). Run it on a map with relief.

#include "AircraftTestSubsystem.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/Aircraft/AircraftState/AircraftState.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSAircraftTerrainFollowTest, Log, All);

namespace AircraftTerrainFollowTestPrivate
{
	const TCHAR* const AircraftClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/BF109/Ger_Aircraft_BF110Airborne.Ger_Aircraft_BF110Airborne_C");
	constexpr uint8 HumanPlayer = 1;
	constexpr float SpawnHeightAboveGround = 1800.f;
	constexpr float DefaultRouteLength = 16000.f;
	constexpr float RouteSampleSpacing = 400.f;
	constexpr int32 RouteDirections = 24;
	constexpr float LandscapeTraceHalfHeight = 200000.f;
	constexpr float SettleSeconds = 1.5f;
	constexpr float RouteArrivalDistance = 1500.f;
	constexpr float RouteTimeoutMarginSeconds = 15.f;
	constexpr float DiveTimeoutSeconds = 40.f;
	// After the dive ended, the aircraft is watched this long for the recovery.
	constexpr float AfterDiveSeconds = 2.f;
	constexpr float SampleLogIntervalSeconds = 0.25f;
	// The route must climb and fall at least this much, or the map has no relief to test with.
	constexpr float MinRouteRelief = 800.f;
	// Share of the ground's height range the aircraft must climb and descend: it really followed the terrain.
	constexpr float MinFollowedShare = 0.5f;
	// Share of the clearance the aircraft must always keep above the ground, even on steep rises.
	constexpr float MinKeptClearanceShare = 0.35f;
	constexpr float VerticalRateTolerance = 1.25f;
}

void UAircraftTestSubsystem::StartAircraftTerrainFollow()
{
	using namespace AircraftTerrainFollowTestPrivate;
	FAircraftTerrainFollowFlightUnderTest& Entry = M_TerrainFlight;
	EnterPhase(EAircraftTestPhase::AircraftTerrainFollowRunning);
	const bool bHasRoute = FindBumpiestRoute(Entry);
	const float Relief = Entry.RouteHighestGroundZ - Entry.RouteLowestGroundZ;
	Check(bHasRoute, TEXT("tf_route_over_landscape"), FString::Printf(
		      TEXT("start=%s end=%s ground=%.0f..%.0f"), *Entry.RouteStart.ToCompactString(),
		      *Entry.RouteEnd.ToCompactString(), Entry.RouteLowestGroundZ, Entry.RouteHighestGroundZ));
	Check(Relief >= MinRouteRelief, TEXT("tf_route_has_relief"), FString::Printf(TEXT("relief=%.0f"), Relief));
	if (not bHasRoute)
	{
		Finish();
		return;
	}
	const FVector Direction = (Entry.RouteEnd - Entry.RouteStart).GetSafeNormal2D();
	const FVector SpawnLocation = Entry.RouteStart + FVector(0.f, 0.f, SpawnHeightAboveGround);
	Entry.Aircraft = Cast<AAircraftMaster>(SpawnUnitForPlayer(
		AircraftClassPath, FTransform(Direction.Rotation(), SpawnLocation), HumanPlayer));
	Check(Entry.Aircraft.IsValid(), TEXT("tf_aircraft_spawned"), AircraftClassPath);
	if (not Entry.Aircraft.IsValid())
	{
		Finish();
		return;
	}
	EnterTerrainFollowStep(Entry, EAircraftTerrainFollowTestStep::Settling);
}

bool UAircraftTestSubsystem::GetLandscapeZ(const FVector2D& Location2D, float& OutGroundZ) const
{
	using namespace AircraftTerrainFollowTestPrivate;
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}
	FHitResult GroundHit;
	FCollisionQueryParams Params(FName(TEXT("TerrainFollowTestGround")), false);
	if (not World->LineTraceSingleByChannel(GroundHit, FVector(Location2D, LandscapeTraceHalfHeight),
	                                        FVector(Location2D, -LandscapeTraceHalfHeight),
	                                        COLLISION_TRACE_LANDSCAPE, Params))
	{
		return false;
	}
	OutGroundZ = GroundHit.ImpactPoint.Z;
	return true;
}

bool UAircraftTestSubsystem::FindBumpiestRoute(FAircraftTerrainFollowFlightUnderTest& Entry) const
{
	using namespace AircraftTerrainFollowTestPrivate;
	float RouteLength = DefaultRouteLength;
	FParse::Value(FCommandLine::Get(), TEXT("AircraftTerrainRouteLength="), RouteLength);
	const FVector2D Start(M_Anchor);
	const int32 SampleCount = FMath::Max(2, FMath::RoundToInt(RouteLength / RouteSampleSpacing));
	float BestRelief = -1.f;
	for (int32 DirectionIndex = 0; DirectionIndex < RouteDirections; ++DirectionIndex)
	{
		const float Angle = 2.f * PI * static_cast<float>(DirectionIndex) / static_cast<float>(RouteDirections);
		const FVector2D Direction(FMath::Cos(Angle), FMath::Sin(Angle));
		float LowestZ = BIG_NUMBER;
		float HighestZ = -BIG_NUMBER;
		bool bAllOnLandscape = true;
		for (int32 SampleIndex = 0; SampleIndex <= SampleCount && bAllOnLandscape; ++SampleIndex)
		{
			float GroundZ = 0.f;
			const float Distance = RouteLength * static_cast<float>(SampleIndex) / static_cast<float>(SampleCount);
			bAllOnLandscape = GetLandscapeZ(Start + Direction * Distance, GroundZ);
			LowestZ = FMath::Min(LowestZ, GroundZ);
			HighestZ = FMath::Max(HighestZ, GroundZ);
		}
		if (not bAllOnLandscape || HighestZ - LowestZ <= BestRelief)
		{
			continue;
		}
		BestRelief = HighestZ - LowestZ;
		float StartGroundZ = 0.f;
		(void)GetLandscapeZ(Start, StartGroundZ);
		Entry.RouteStart = FVector(Start, StartGroundZ);
		Entry.RouteEnd = FVector(Start + Direction * RouteLength, StartGroundZ);
		Entry.RouteLowestGroundZ = LowestZ;
		Entry.RouteHighestGroundZ = HighestZ;
	}
	return BestRelief >= 0.f;
}

void UAircraftTestSubsystem::TickAircraftTerrainFollow()
{
	FAircraftTerrainFollowFlightUnderTest& Entry = M_TerrainFlight;
	if (not Entry.Aircraft.IsValid())
	{
		Check(false, TEXT("tf_aircraft_alive"), TEXT("destroyed during the test"));
		Finish();
		return;
	}
	switch (Entry.Step)
	{
	case EAircraftTerrainFollowTestStep::Settling:
		TickTerrainFollowSettling(Entry);
		return;
	case EAircraftTerrainFollowTestStep::FlyingRoute:
		TickTerrainFollowFlyingRoute(Entry);
		return;
	case EAircraftTerrainFollowTestStep::AttackDiving:
		TickTerrainFollowAttackDiving(Entry);
		return;
	case EAircraftTerrainFollowTestStep::Done:
		Finish();
		return;
	}
}

void UAircraftTestSubsystem::TickTerrainFollowSettling(FAircraftTerrainFollowFlightUnderTest& Entry)
{
	using namespace AircraftTerrainFollowTestPrivate;
	if (GetNow() - Entry.StepStartSeconds < SettleSeconds)
	{
		return;
	}
	AAircraftMaster& Aircraft = *Entry.Aircraft;
	const FAircraftTerrainFollow& TerrainFollow = Aircraft.GetTerrainFollow();
	Check(Aircraft.GetLandedState() == EAircraftLandingState::Airborne, TEXT("tf_spawned_airborne"),
	      FString::Printf(TEXT("state=%d"), static_cast<int32>(Aircraft.GetLandedState())));
	Check(TerrainFollow.GetHasClearance(), TEXT("tf_clearance_captured"),
	      FString::Printf(TEXT("clearance=%.1f"), TerrainFollow.GetClearance()));
	Entry.Clearance = TerrainFollow.GetClearance();
	const ECommandQueueError Error = Aircraft.MoveToLocation(Entry.RouteEnd, true, FRotator::ZeroRotator);
	Check(Error == ECommandQueueError::NoError, TEXT("tf_route_move_order_accepted"),
	      FString::Printf(TEXT("error=%d"), static_cast<int32>(Error)));
	Entry.LastZ = Aircraft.GetActorLocation().Z;
	EnterTerrainFollowStep(Entry, EAircraftTerrainFollowTestStep::FlyingRoute);
}

void UAircraftTestSubsystem::RecordTerrainFollowSample(FAircraftTerrainFollowFlightUnderTest& Entry,
                                                       const TCHAR* StepName)
{
	using namespace AircraftTerrainFollowTestPrivate;
	const AAircraftMaster& Aircraft = *Entry.Aircraft;
	const FVector Location = Aircraft.GetActorLocation();
	float GroundZ = 0.f;
	const bool bHasGround = GetLandscapeZ(FVector2D(Location), GroundZ);
	const float AboveGround = bHasGround ? Location.Z - GroundZ : -1.f;
	if (bHasGround)
	{
		Entry.LowestAboveGround = FMath::Min(Entry.LowestAboveGround, AboveGround);
		Entry.HighestAboveGround = FMath::Max(Entry.HighestAboveGround, AboveGround);
	}
	if (GetNow() < Entry.NextSampleLogSeconds)
	{
		return;
	}
	Entry.NextSampleLogSeconds = GetNow() + SampleLogIntervalSeconds;
	const FAircraftTerrainFollow& TerrainFollow = Aircraft.GetTerrainFollow();
	UE_LOG(LogRTSAircraftTerrainFollowTest, Display,
	       TEXT("RTS_AIRCRAFT_TEST TF_SAMPLE step=%s t=%.2f x=%.0f y=%.0f ground=%.1f z=%.1f above=%.1f "
		       "wanted=%.1f vspeed=%.1f pitch=%.1f dive=%d"),
	       StepName, GetNow() - Entry.StepStartSeconds, Location.X, Location.Y, GroundZ, Location.Z, AboveGround,
	       TerrainFollow.GetDesiredFlightZ(), TerrainFollow.GetVerticalSpeed(), Aircraft.GetActorRotation().Pitch,
	       Aircraft.GetIsFlyingAttackDive() ? 1 : 0);
}

void UAircraftTestSubsystem::TickTerrainFollowFlyingRoute(FAircraftTerrainFollowFlightUnderTest& Entry)
{
	using namespace AircraftTerrainFollowTestPrivate;
	AAircraftMaster& Aircraft = *Entry.Aircraft;
	const UWorld* World = GetWorld();
	const float DeltaSeconds = IsValid(World) ? World->GetDeltaSeconds() : 0.f;
	const float Z = Aircraft.GetActorLocation().Z;
	if (DeltaSeconds > KINDA_SMALL_NUMBER)
	{
		Entry.FastestVerticalRate = FMath::Max(Entry.FastestVerticalRate, FMath::Abs(Z - Entry.LastZ) / DeltaSeconds);
	}
	Entry.LastZ = Z;
	Entry.LowestAircraftZ = FMath::Min(Entry.LowestAircraftZ, Z);
	Entry.HighestAircraftZ = FMath::Max(Entry.HighestAircraftZ, Z);
	RecordTerrainFollowSample(Entry, TEXT("route"));

	const float RouteLength = FVector::Dist2D(Entry.RouteStart, Entry.RouteEnd);
	const FAircraftTerrainFollowSettings Settings;
	const float Timeout = RouteLength / FMath::Max(Aircraft.GetVelocity().Size2D(), 500.f) + RouteTimeoutMarginSeconds;
	const float Waited = GetNow() - Entry.StepStartSeconds;
	const bool bArrived = FVector::Dist2D(Aircraft.GetActorLocation(), Entry.RouteEnd) <= RouteArrivalDistance;
	if (not bArrived && Waited < Timeout)
	{
		return;
	}
	const float Relief = Entry.RouteHighestGroundZ - Entry.RouteLowestGroundZ;
	const float Followed = Entry.HighestAircraftZ - Entry.LowestAircraftZ;
	Check(bArrived, TEXT("tf_route_flown"), FString::Printf(TEXT("after=%.1fs"), Waited));
	Check(Followed >= Relief * MinFollowedShare, TEXT("tf_followed_terrain"),
	      FString::Printf(TEXT("aircraft_z_range=%.0f ground_relief=%.0f"), Followed, Relief));
	Check(Entry.LowestAboveGround >= Entry.Clearance * MinKeptClearanceShare, TEXT("tf_kept_clear_of_ground"),
	      FString::Printf(TEXT("above_ground=%.0f..%.0f clearance=%.0f"), Entry.LowestAboveGround,
	                      Entry.HighestAboveGround, Entry.Clearance));
	Check(Entry.FastestVerticalRate <= Settings.MaxVerticalSpeed * VerticalRateTolerance, TEXT("tf_smooth_height"),
	      FString::Printf(TEXT("fastest=%.0f cm/s cap=%.0f"), Entry.FastestVerticalRate, Settings.MaxVerticalSpeed));

	// Attack the lowest ground of the route: the dive must keep the attack path's own heights.
	float LowestRouteZ = BIG_NUMBER;
	const int32 SampleCount = FMath::Max(2, FMath::RoundToInt(RouteLength / RouteSampleSpacing));
	for (int32 SampleIndex = 0; SampleIndex <= SampleCount; ++SampleIndex)
	{
		const FVector2D Point = FMath::Lerp(FVector2D(Entry.RouteStart), FVector2D(Entry.RouteEnd),
		                                    static_cast<float>(SampleIndex) / static_cast<float>(SampleCount));
		float GroundZ = 0.f;
		if (GetLandscapeZ(Point, GroundZ) && GroundZ < LowestRouteZ)
		{
			LowestRouteZ = GroundZ;
			Entry.AttackPoint = FVector(Point, GroundZ);
		}
	}
	const ECommandQueueError Error = Aircraft.AttackGround(Entry.AttackPoint, true);
	Check(Error == ECommandQueueError::NoError, TEXT("tf_attack_order_accepted"),
	      FString::Printf(TEXT("error=%d at=%s"), static_cast<int32>(Error), *Entry.AttackPoint.ToCompactString()));
	EnterTerrainFollowStep(Entry, EAircraftTerrainFollowTestStep::AttackDiving);
}

void UAircraftTestSubsystem::TickTerrainFollowAttackDiving(FAircraftTerrainFollowFlightUnderTest& Entry)
{
	using namespace AircraftTerrainFollowTestPrivate;
	const AAircraftMaster& Aircraft = *Entry.Aircraft;
	RecordTerrainFollowSample(Entry, TEXT("attack"));
	if (Aircraft.GetIsFlyingAttackDive())
	{
		++Entry.DiveFrames;
		if (not FMath::IsNearlyZero(Aircraft.GetTerrainFollow().GetVerticalSpeed()))
		{
			++Entry.DiveFramesWithTerrainMotion;
		}
		Entry.LastDiveSeconds = GetNow();
	}
	const float Waited = GetNow() - Entry.StepStartSeconds;
	const bool bDiveOver = Entry.DiveFrames > 0 && not Aircraft.GetIsFlyingAttackDive()
		&& GetNow() - Entry.LastDiveSeconds >= AfterDiveSeconds;
	if (not bDiveOver && Waited < DiveTimeoutSeconds)
	{
		return;
	}
	Check(Entry.DiveFrames > 0, TEXT("tf_attack_dive_flown"), FString::Printf(TEXT("dive_frames=%d"), Entry.DiveFrames));
	Check(Entry.DiveFramesWithTerrainMotion == 0, TEXT("tf_dive_untouched_by_terrain_follow"),
	      FString::Printf(TEXT("frames_with_terrain_motion=%d of %d"), Entry.DiveFramesWithTerrainMotion,
	                      Entry.DiveFrames));
	EnterTerrainFollowStep(Entry, EAircraftTerrainFollowTestStep::Done);
}

void UAircraftTestSubsystem::EnterTerrainFollowStep(FAircraftTerrainFollowFlightUnderTest& Entry,
                                                    const EAircraftTerrainFollowTestStep NewStep)
{
	Entry.Step = NewStep;
	Entry.StepStartSeconds = GetNow();
	Entry.NextSampleLogSeconds = Entry.StepStartSeconds;
	UE_LOG(LogRTSAircraftTerrainFollowTest, Display, TEXT("RTS_AIRCRAFT_TEST TF_STEP %d at=%.1f"),
	       static_cast<int32>(NewStep), Entry.StepStartSeconds);
}

#endif
