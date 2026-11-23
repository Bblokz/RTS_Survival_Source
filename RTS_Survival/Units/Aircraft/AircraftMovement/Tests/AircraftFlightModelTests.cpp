// Copyright (C) Bas Blokzijl - All rights reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RTS_Survival/Units/Aircraft/AircraftMovement/AircraftFlightModel.h"
#include "RTS_Survival/Units/Aircraft/AircraftPath/FAircraftPath.h"
#include "RTS_Survival/Units/Aircraft/AircraftState/AircraftState.h"

namespace AircraftFlightModelTestsPrivate
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	constexpr float StepSeconds = 1.f / 60.f;
	constexpr float CruiseSpeed = 4000.f;
	constexpr float Acceleration = 600.f;

	struct FFlightRecord
	{
		FVector FinalLocation = FVector::ZeroVector;
		FVector FinalVelocity = FVector::ZeroVector;
		float LowestSpeed = BIG_NUMBER;
		float LowestZ = BIG_NUMBER;
		float FastestYawRate = 0.f;
		float LargestYawRateStep = 0.f;
		float LargestRoll = 0.f;
		float LargestSlipDeg = 0.f;
		bool bRollAgainstTurn = false;
	};

	/** @brief Flies the model toward a fixed aim point, integrating the location like the movement component does. */
	FFlightRecord Fly(const FAircraftFlightModelSettings& Settings, const FVector& AimPoint, const float Seconds,
	                  const TOptional<float> FloorZ = {}, const FVector StartLocation = FVector::ZeroVector)
	{
		FAircraftFlightModel Model;
		FFlightRecord Record;
		FVector Location = StartLocation;
		FVector Velocity(CruiseSpeed, 0.f, 0.f);
		FRotator Rotation = FRotator::ZeroRotator;
		float PreviousYawRate = 0.f;
		const int32 Steps = FMath::RoundToInt(Seconds / StepSeconds);
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			FAircraftFlightInput Input;
			Input.Location = Location;
			Input.Velocity = Velocity;
			Input.CurrentRotation = Rotation;
			Input.AimPoint = AimPoint;
			Input.WantedSpeed = CruiseSpeed;
			Input.ReferenceSpeed = CruiseSpeed;
			Input.Acceleration = Acceleration;
			Input.FloorZ = FloorZ;
			Input.DeltaTime = StepSeconds;
			const FAircraftFlightOutput Output = Model.Step(Input, Settings);
			Velocity = Output.Velocity;
			Rotation = Output.Rotation;
			Location += Velocity * StepSeconds;
			const float YawRate = Model.GetYawRate();
			Record.LowestSpeed = FMath::Min(Record.LowestSpeed, Velocity.Size());
			Record.LowestZ = FMath::Min(Record.LowestZ, Location.Z);
			Record.FastestYawRate = FMath::Max(Record.FastestYawRate, FMath::Abs(YawRate));
			Record.LargestYawRateStep = FMath::Max(Record.LargestYawRateStep, FMath::Abs(YawRate - PreviousYawRate));
			Record.LargestRoll = FMath::Max(Record.LargestRoll, FMath::Abs(Rotation.Roll));
			const float Slip = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(
				Rotation.Vector().GetSafeNormal2D(), Velocity.GetSafeNormal2D()), -1.f, 1.f)));
			Record.LargestSlipDeg = FMath::Max(Record.LargestSlipDeg, Slip);
			// A clear turn (not the last wobble of settling on the heading) must bank into it.
			constexpr float ClearTurnRate = 20.f;
			constexpr float ClearRoll = 5.f;
			if (FMath::Abs(YawRate) > ClearTurnRate && FMath::Abs(Rotation.Roll) > ClearRoll
				&& FMath::Sign(YawRate) != FMath::Sign(Rotation.Roll))
			{
				Record.bRollAgainstTurn = true;
			}
			PreviousYawRate = YawRate;
		}
		Record.FinalLocation = Location;
		Record.FinalVelocity = Velocity;
		return Record;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAircraftFlightModelReversalTest,
                                 "RTS.Aircraft.FlightModel.KeepsSpeedThroughReversal",
                                 AircraftFlightModelTestsPrivate::TestFlags)

bool FAircraftFlightModelReversalTest::RunTest(const FString& Parameters)
{
	using namespace AircraftFlightModelTestsPrivate;
	const FAircraftFlightModelSettings Settings;
	// Far behind and a little to the right: a full reversal.
	const FFlightRecord Record = Fly(Settings, FVector(-50000.f, 500.f, 0.f), 8.f);
	const float WantedLowestSpeed = CruiseSpeed * (1.f - Settings.TurnSpeedLoss) - 1.f;
	TestTrue(FString::Printf(TEXT("Keeps its speed through the reversal: lowest %.0f, wanted >= %.0f"),
	                         Record.LowestSpeed, WantedLowestSpeed), Record.LowestSpeed >= WantedLowestSpeed);
	TestTrue(TEXT("Ends flying toward the aim point"), Record.FinalVelocity.GetSafeNormal().X < -0.99f);
	TestTrue(FString::Printf(TEXT("Turn rate stays capped: fastest %.1f"), Record.FastestYawRate),
	         Record.FastestYawRate <= Settings.MaxTurnRate + KINDA_SMALL_NUMBER);
	const float AllowedRateStep = Settings.TurnRateAcceleration * StepSeconds + KINDA_SMALL_NUMBER;
	TestTrue(FString::Printf(TEXT("Turn rolls in, no snaps: largest rate step %.2f, allowed %.2f"),
	                         Record.LargestYawRateStep, AllowedRateStep), Record.LargestYawRateStep <= AllowedRateStep);
	TestTrue(TEXT("Banks into the turn, never against it"), not Record.bRollAgainstTurn);
	TestTrue(TEXT("Bank stays within the maximum"), Record.LargestRoll <= Settings.MaxBankAngle + KINDA_SMALL_NUMBER);
	TestTrue(FString::Printf(TEXT("Nose follows the flight: largest slip %.2f"), Record.LargestSlipDeg),
	         Record.LargestSlipDeg < 0.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAircraftFlightModelStraightTest,
                                 "RTS.Aircraft.FlightModel.StraightFlightStaysLevel",
                                 AircraftFlightModelTestsPrivate::TestFlags)

bool FAircraftFlightModelStraightTest::RunTest(const FString& Parameters)
{
	using namespace AircraftFlightModelTestsPrivate;
	const FAircraftFlightModelSettings Settings;
	const FFlightRecord Record = Fly(Settings, FVector(100000.f, 0.f, 0.f), 3.f);
	TestTrue(TEXT("No turn"), Record.FastestYawRate < KINDA_SMALL_NUMBER);
	TestTrue(TEXT("Wings level"), Record.LargestRoll < KINDA_SMALL_NUMBER);
	TestTrue(TEXT("Full speed"), FMath::IsNearlyEqual(Record.FinalVelocity.Size(), CruiseSpeed, 1.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAircraftFlightModelFloorTest,
                                 "RTS.Aircraft.FlightModel.DiveFlattensAboveFloor",
                                 AircraftFlightModelTestsPrivate::TestFlags)

bool FAircraftFlightModelFloorTest::RunTest(const FString& Parameters)
{
	using namespace AircraftFlightModelTestsPrivate;
	const FAircraftFlightModelSettings Settings;
	constexpr float StartZ = 3000.f;
	constexpr float FloorZ = 1000.f;
	// Aimed steeply into the ground far below the floor.
	const FFlightRecord Record = Fly(Settings, FVector(6000.f, 0.f, -8000.f), 4.f, FloorZ, FVector(0.f, 0.f, StartZ));
	TestTrue(FString::Printf(TEXT("Never below the floor: lowest %.1f"), Record.LowestZ), Record.LowestZ >= FloorZ - 1.f);
	TestTrue(TEXT("Did dive toward it"), Record.LowestZ < StartZ - 1000.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAircraftFlightModelPathFollowingTest,
                                 "RTS.Aircraft.FlightModel.PathLookAheadAndCornerPassing",
                                 AircraftFlightModelTestsPrivate::TestFlags)

bool FAircraftFlightModelPathFollowingTest::RunTest(const FString& Parameters)
{
	FAircraftPath Path;
	Path.PathPoints.Add({FVector(0.f, 0.f, 0.f)});
	Path.PathPoints.Add({FVector(1000.f, 0.f, 0.f)});
	Path.PathPoints.Add({FVector(1000.f, 1000.f, 0.f)});
	Path.CurrentPathIndex = 0;

	const FVector OnFirstLeg = Path.GetLookAheadPoint(FVector(200.f, 50.f, 0.f), 300.f);
	TestTrue(TEXT("Look-ahead runs along the leg from the projection"), OnFirstLeg.Equals(FVector(500.f, 0.f, 0.f), 0.1f));
	const FVector AroundCorner = Path.GetLookAheadPoint(FVector(900.f, 0.f, 0.f), 300.f);
	TestTrue(TEXT("Look-ahead continues round the corner"), AroundCorner.Equals(FVector(1000.f, 200.f, 0.f), 0.1f));
	const FVector PastEnd = Path.GetLookAheadPoint(FVector(950.f, 0.f, 0.f), 3000.f);
	TestTrue(TEXT("Past the last point it continues along the last leg"), PastEnd.Y > 1000.f
	         && FMath::IsNearlyEqual(PastEnd.X, 1000.f, 0.1f));

	TestFalse(TEXT("Not passed while still on the leg"), Path.GetShouldAdvancePastTargetPoint(FVector(500.f, 0.f, 0.f), 10.f));
	TestTrue(TEXT("Passed when the corner was cut onto the next leg"),
	         Path.GetShouldAdvancePastTargetPoint(FVector(950.f, 150.f, 0.f), 10.f));

	FAircraftPathPoint Floored{FVector(1000.f, 0.f, 0.f), 0.f, EAirPathPointType::AttackDive};
	Floored.FloorZ = 123.f;
	Path.PathPoints[1] = Floored;
	TestTrue(TEXT("Dive segment carries its floor"), Path.GetFloorZOfCurrentSegment().IsSet()
	         && FMath::IsNearlyEqual(Path.GetFloorZOfCurrentSegment().GetValue(), 123.f));
	Path.CurrentPathIndex = 1;
	TestFalse(TEXT("A regular segment has no floor"), Path.GetFloorZOfCurrentSegment().IsSet());
	return true;
}

#endif
