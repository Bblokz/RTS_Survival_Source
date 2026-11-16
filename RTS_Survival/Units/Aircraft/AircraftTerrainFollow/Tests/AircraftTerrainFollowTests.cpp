// Copyright (C) Bas Blokzijl - All rights reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RTS_Survival/Units/Aircraft/AircraftPath/FAircraftPath.h"
#include "RTS_Survival/Units/Aircraft/AircraftState/AircraftState.h"
#include "RTS_Survival/Units/Aircraft/AircraftTerrainFollow/AircraftTerrainFollow.h"

namespace AircraftTerrainFollowTestsPrivate
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	constexpr float StepSeconds = 1.f / 60.f;
	constexpr float StartGroundZ = 0.f;
	constexpr float StartFlightZ = 800.f;
	constexpr float CruiseSpeed = 2000.f;
	const float MinClearance = FAircraftTerrainFollowSettings().MinGroundClearance;

	struct FFlightRecord
	{
		float FinalFlightZ = 0.f;
		float HighestFlightZ = -BIG_NUMBER;
		float LowestFlightZ = BIG_NUMBER;
		float FastestVerticalSpeed = 0.f;
		float LargestSpeedChange = 0.f;
	};

	/** @brief Captures the clearance over flat ground at StartGroundZ, as an aircraft does right after take-off. */
	FAircraftTerrainFollow MakeCalibrated()
	{
		FAircraftTerrainFollow TerrainFollow;
		TerrainFollow.ResetClearance();
		TerrainFollow.OnGroundSampled(StartGroundZ, StartGroundZ, StartFlightZ, MinClearance);
		return TerrainFollow;
	}

	/** @brief Flies for Seconds over ground at GroundZ, sampling at the settings' interval like the aircraft does. */
	FFlightRecord Fly(FAircraftTerrainFollow& TerrainFollow, const FAircraftTerrainFollowSettings& Settings,
	                  const float GroundZ, const float Seconds, float FlightZ = StartFlightZ)
	{
		FFlightRecord Record;
		float PreviousSpeed = TerrainFollow.GetVerticalSpeed();
		const int32 Steps = FMath::RoundToInt(Seconds / StepSeconds);
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			if (TerrainFollow.TickSampleTimer(StepSeconds, Settings.SampleInterval))
			{
				TerrainFollow.OnGroundSampled(GroundZ, GroundZ, FlightZ, Settings.MinGroundClearance);
			}
			FlightZ += TerrainFollow.AdvanceFlightHeight(StepSeconds, FlightZ, Settings);
			const float Speed = TerrainFollow.GetVerticalSpeed();
			Record.HighestFlightZ = FMath::Max(Record.HighestFlightZ, FlightZ);
			Record.LowestFlightZ = FMath::Min(Record.LowestFlightZ, FlightZ);
			Record.FastestVerticalSpeed = FMath::Max(Record.FastestVerticalSpeed, FMath::Abs(Speed));
			Record.LargestSpeedChange = FMath::Max(Record.LargestSpeedChange, FMath::Abs(Speed - PreviousSpeed));
			PreviousSpeed = Speed;
		}
		Record.FinalFlightZ = FlightZ;
		return Record;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAircraftTerrainFollowClimbTest,
                                 "RTS.Aircraft.TerrainFollow.ClimbsSmoothlyOverHill",
                                 AircraftTerrainFollowTestsPrivate::TestFlags)

bool FAircraftTerrainFollowClimbTest::RunTest(const FString& Parameters)
{
	using namespace AircraftTerrainFollowTestsPrivate;
	const FAircraftTerrainFollowSettings Settings;
	FAircraftTerrainFollow TerrainFollow = MakeCalibrated();
	TestEqual(TEXT("Clearance is the take-off height above the ground"), TerrainFollow.GetClearance(),
	          StartFlightZ - StartGroundZ);

	constexpr float HillZ = 1500.f;
	const FFlightRecord Record = Fly(TerrainFollow, Settings, HillZ, 12.f);
	const float WantedZ = HillZ + StartFlightZ;
	TestTrue(TEXT("Ends at the same clearance above the hill"), FMath::IsNearlyEqual(Record.FinalFlightZ, WantedZ, 1.f));
	TestTrue(TEXT("Never climbs past the wanted height"), Record.HighestFlightZ <= WantedZ + KINDA_SMALL_NUMBER);
	TestTrue(TEXT("Vertical speed stays capped"),
	         Record.FastestVerticalSpeed <= Settings.MaxVerticalSpeed + KINDA_SMALL_NUMBER);
	// Acceleration-limited, plus the braking cap; the last frame stops from below two acceleration steps.
	const float AllowedSpeedChange = Settings.MaxVerticalAcceleration * StepSeconds * 2.f;
	TestTrue(FString::Printf(TEXT("No sudden jolt: largest speed change %.1f cm/s per frame"),
	                         Record.LargestSpeedChange), Record.LargestSpeedChange <= AllowedSpeedChange);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAircraftTerrainFollowValleyTest,
                                 "RTS.Aircraft.TerrainFollow.DescendsIntoValley",
                                 AircraftTerrainFollowTestsPrivate::TestFlags)

bool FAircraftTerrainFollowValleyTest::RunTest(const FString& Parameters)
{
	using namespace AircraftTerrainFollowTestsPrivate;
	const FAircraftTerrainFollowSettings Settings;
	FAircraftTerrainFollow TerrainFollow = MakeCalibrated();
	constexpr float ValleyZ = -1200.f;
	const FFlightRecord Record = Fly(TerrainFollow, Settings, ValleyZ, 12.f);
	const float WantedZ = ValleyZ + StartFlightZ;
	TestTrue(TEXT("Ends at the same clearance above the valley"), FMath::IsNearlyEqual(Record.FinalFlightZ, WantedZ, 1.f));
	TestTrue(TEXT("Never dives below the wanted height"), Record.LowestFlightZ >= WantedZ - KINDA_SMALL_NUMBER);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAircraftTerrainFollowDeadZoneTest,
                                 "RTS.Aircraft.TerrainFollow.IgnoresSmallBumps",
                                 AircraftTerrainFollowTestsPrivate::TestFlags)

bool FAircraftTerrainFollowDeadZoneTest::RunTest(const FString& Parameters)
{
	using namespace AircraftTerrainFollowTestsPrivate;
	const FAircraftTerrainFollowSettings Settings;
	FAircraftTerrainFollow TerrainFollow = MakeCalibrated();
	const float BumpZ = Settings.HeightDeadZone * 0.5f;
	const FFlightRecord Record = Fly(TerrainFollow, Settings, BumpZ, 3.f);
	TestEqual(TEXT("A level aircraft stays level over a small bump"), Record.FinalFlightZ, StartFlightZ);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAircraftTerrainFollowSuspendResetTest,
                                 "RTS.Aircraft.TerrainFollow.SuspendAndResetClearance",
                                 AircraftTerrainFollowTestsPrivate::TestFlags)

bool FAircraftTerrainFollowSuspendResetTest::RunTest(const FString& Parameters)
{
	using namespace AircraftTerrainFollowTestsPrivate;
	const FAircraftTerrainFollowSettings Settings;
	FAircraftTerrainFollow TerrainFollow = MakeCalibrated();
	(void)Fly(TerrainFollow, Settings, 2000.f, 1.f);
	TestTrue(TEXT("Climbing before the dive"), TerrainFollow.GetVerticalSpeed() > 0.f);
	TerrainFollow.Suspend();
	TestEqual(TEXT("A dive stops the terrain climb at once"), TerrainFollow.GetVerticalSpeed(), 0.f);

	TerrainFollow.ResetClearance();
	TestFalse(TEXT("A new cruise height drops the old clearance"), TerrainFollow.GetHasClearance());
	TestEqual(TEXT("Without a clearance the height does not move"),
	          TerrainFollow.AdvanceFlightHeight(StepSeconds, 3000.f, Settings), 0.f);
	TestTrue(TEXT("The next frame samples at once"), TerrainFollow.TickSampleTimer(StepSeconds, Settings.SampleInterval));
	constexpr float NewGroundZ = 500.f;
	constexpr float NewFlightZ = 1700.f;
	TerrainFollow.OnGroundSampled(NewGroundZ, NewGroundZ + 300.f, NewFlightZ, MinClearance);
	TestEqual(TEXT("The clearance is taken from the ground right below"), TerrainFollow.GetClearance(),
	          NewFlightZ - NewGroundZ);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAircraftTerrainFollowClearanceFloorTest,
                                 "RTS.Aircraft.TerrainFollow.ClearanceFloorAndTargetCapture",
                                 AircraftTerrainFollowTestsPrivate::TestFlags)

bool FAircraftTerrainFollowClearanceFloorTest::RunTest(const FString& Parameters)
{
	using namespace AircraftTerrainFollowTestsPrivate;
	FAircraftTerrainFollow SpawnedBelowRidge;
	SpawnedBelowRidge.ResetClearance();
	constexpr float RidgeZ = 1000.f;
	SpawnedBelowRidge.OnGroundSampled(RidgeZ, RidgeZ, RidgeZ + 50.f, MinClearance);
	TestEqual(TEXT("A low start does not make the aircraft skim the ground"), SpawnedBelowRidge.GetClearance(),
	          MinClearance);

	// An ability flight spawned at a fixed height keeps that height above its target, not above the map edge.
	FAircraftTerrainFollow AbilityFlight;
	AbilityFlight.ResetClearance();
	constexpr float TargetGroundZ = -400.f;
	constexpr float SpawnFlightZ = 6000.f;
	AbilityFlight.CaptureClearance(TargetGroundZ, SpawnFlightZ, MinClearance);
	TestEqual(TEXT("Clearance is measured from the target's ground"), AbilityFlight.GetClearance(),
	          SpawnFlightZ - TargetGroundZ);
	AbilityFlight.OnGroundSampled(2500.f, 2500.f, SpawnFlightZ, MinClearance);
	TestEqual(TEXT("A later sample at the map edge keeps that clearance"), AbilityFlight.GetClearance(),
	          SpawnFlightZ - TargetGroundZ);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAircraftTerrainFollowLookAheadTest,
                                 "RTS.Aircraft.TerrainFollow.ClimbsForHighestGroundAhead",
                                 AircraftTerrainFollowTestsPrivate::TestFlags)

bool FAircraftTerrainFollowLookAheadTest::RunTest(const FString& Parameters)
{
	using namespace AircraftTerrainFollowTestsPrivate;
	FAircraftTerrainFollow TerrainFollow = MakeCalibrated();
	constexpr float HillAheadZ = 900.f;
	TerrainFollow.OnGroundSampled(StartGroundZ, HillAheadZ, StartFlightZ, MinClearance);
	TestEqual(TEXT("Wanted height clears the hill ahead"), TerrainFollow.GetDesiredFlightZ(), HillAheadZ + StartFlightZ);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAircraftTerrainFollowPitchTest,
                                 "RTS.Aircraft.TerrainFollow.ClimbPitch",
                                 AircraftTerrainFollowTestsPrivate::TestFlags)

bool FAircraftTerrainFollowPitchTest::RunTest(const FString& Parameters)
{
	using namespace AircraftTerrainFollowTestsPrivate;
	const FAircraftTerrainFollowSettings Settings;
	FAircraftTerrainFollow TerrainFollow = MakeCalibrated();
	TestEqual(TEXT("Level flight has no pitch"), TerrainFollow.GetClimbPitch(CruiseSpeed, Settings.MaxClimbPitch), 0.f);
	(void)Fly(TerrainFollow, Settings, 3000.f, 3.f);
	const float ClimbPitch = TerrainFollow.GetClimbPitch(CruiseSpeed, Settings.MaxClimbPitch);
	TestTrue(TEXT("Nose up while climbing"), ClimbPitch > 0.f);
	TestTrue(TEXT("Pitch stays clamped"), ClimbPitch <= Settings.MaxClimbPitch);
	TestTrue(TEXT("Hovering aircraft do not point straight up"),
	         TerrainFollow.GetClimbPitch(0.f, Settings.MaxClimbPitch) <= Settings.MaxClimbPitch);

	FAircraftTerrainFollow Descending = MakeCalibrated();
	(void)Fly(Descending, Settings, -3000.f, 3.f);
	TestTrue(TEXT("Nose down while descending"), Descending.GetClimbPitch(CruiseSpeed, Settings.MaxClimbPitch) < 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAircraftPathShiftHeightTest,
                                 "RTS.Aircraft.TerrainFollow.PathShiftKeepsShape",
                                 AircraftTerrainFollowTestsPrivate::TestFlags)

bool FAircraftPathShiftHeightTest::RunTest(const FString& Parameters)
{
	FAircraftPath Path;
	Path.PathPoints.Add({FVector(0.f, 0.f, 800.f), 0.0, EAirPathPointType::Regular});
	Path.PathPoints.Add({FVector(3000.f, 0.f, 800.f), 0.0, EAirPathPointType::Regular});
	Path.PathPoints.Add({FVector(5000.f, 0.f, 300.f), 0.0, EAirPathPointType::AttackDive});
	Path.PathPoints.Add({FVector(6000.f, 0.f, 800.f), 0.0, EAirPathPointType::GetOutOfDive});

	const FVector DiveDropBefore = Path.PathPoints[2].Location - Path.PathPoints[1].Location;
	constexpr float Shift = 420.f;
	Path.ShiftHeight(Shift);
	TestEqual(TEXT("Every point moves with the cruise height"), Path.PathPoints[0].Location.Z, 800.0 + Shift);
	TestEqual(TEXT("The dive keeps its depth"), Path.PathPoints[2].Location - Path.PathPoints[1].Location,
	          DiveDropBefore);

	Path.CurrentPathIndex = 0;
	TestFalse(TEXT("Approach is not the aiming part"), Path.GetIsFlyingDiveSegment());
	Path.CurrentPathIndex = 1;
	TestTrue(TEXT("Flying into the dive aims"), Path.GetIsFlyingDiveSegment());
	Path.CurrentPathIndex = 2;
	TestTrue(TEXT("Flying out of the dive drops bombs"), Path.GetIsFlyingDiveSegment());
	Path.CurrentPathIndex = 3;
	TestFalse(TEXT("A finished path has no next point"), Path.GetIsFlyingDiveSegment());
	return true;
}

#endif
