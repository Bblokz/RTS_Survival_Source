// Copyright (C) Bas Blokzijl - All rights reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RTS_Survival/Units/Aircraft/AircraftDeath/AircraftDeathMath.h"

namespace AircraftDeathMathTestsPrivate
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	constexpr float EarthGravityZ = -980.f;
	constexpr float StepSeconds = 1.f / 60.f;
	constexpr float FighterSpeed = 2600.f;
	constexpr float StartHeight = 2000.f;

	// Propeller bone locations of the game's aircraft skeletons (mesh space), as the editor reports them.
	const TArray<FVector> Bf109Engines = {FVector(276.f, -9.f, 185.f)};
	const TArray<FVector> Me410Engines = {FVector(558.f, -257.f, 196.f), FVector(558.f, 234.f, 196.f)};
	const TArray<FVector> Pe8Engines = {
		FVector(384.f, -376.f, 10.f), FVector(432.f, -166.f, -5.f), FVector(432.f, 166.f, -5.f), FVector(384.f, 376.f, 10.f)
	};
	// A tri-motor: the nose engine must never be the burning wing engine.
	const TArray<FVector> TriMotorEngines = {FVector(500.f, 0.f, 0.f), FVector(200.f, -300.f, 0.f), FVector(200.f, 300.f, 0.f)};

	FAircraftCrashFlightState MakeLevelFlight(const EAircraftDamagedSide DamagedSide)
	{
		FAircraftCrashFlightState State;
		State.Location = FVector(0.f, 0.f, StartHeight);
		State.Velocity = FVector(FighterSpeed, 0.f, 0.f);
		State.DamagedSide = DamagedSide;
		return State;
	}

	/** Flies the wreck for Seconds and reports the highest climb speed and the lowest height seen. */
	void Fly(FAircraftCrashFlightState& State, const FAircraftDeathSettings& Settings, const EAircraftFireOrigin Origin,
	         const float Seconds, float& OutHighestVelocityZ)
	{
		OutHighestVelocityZ = State.Velocity.Z;
		const int32 Steps = FMath::RoundToInt(Seconds / StepSeconds);
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			FAircraftDeathMath::StepCrashFlight(State, Settings, Origin, StepSeconds, EarthGravityZ);
			OutHighestVelocityZ = FMath::Max(OutHighestVelocityZ, State.Velocity.Z);
		}
	}

	/** @return World height of the right wing tip relative to the wreck's centre. */
	float GetRightWingTipHeight(const FAircraftCrashFlightState& State)
	{
		const FRotator Rotation = FAircraftDeathMath::GetCrashRotation(State, FRotator::ZeroRotator);
		return Rotation.RotateVector(FVector(0.f, 500.f, 0.f)).Z;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAircraftDeathFireOriginTest, "RTS.Aircraft.Death.FireOriginFollowsEngineCount",
                                 AircraftDeathMathTestsPrivate::TestFlags)

bool FAircraftDeathFireOriginTest::RunTest(const FString& Parameters)
{
	using FAircraftDeathMath::ResolveFireOrigin;
	TestEqual(TEXT("A single-engine fighter burns at the nose"),
	          ResolveFireOrigin(EAircraftFireOrigin::Automatic, 1), EAircraftFireOrigin::NoseEngine);
	TestEqual(TEXT("An aircraft without propeller bones burns at the nose"),
	          ResolveFireOrigin(EAircraftFireOrigin::Automatic, 0), EAircraftFireOrigin::NoseEngine);
	for (const int32 EngineCount : {2, 4, 6})
	{
		TestEqual(FString::Printf(TEXT("A %d-engine aircraft burns on a wing"), EngineCount),
		          ResolveFireOrigin(EAircraftFireOrigin::Automatic, EngineCount), EAircraftFireOrigin::WingEngine);
	}
	TestEqual(TEXT("A Blueprint asking for a wing fire gets it on a fighter"),
	          ResolveFireOrigin(EAircraftFireOrigin::WingEngine, 1), EAircraftFireOrigin::WingEngine);
	TestEqual(TEXT("A Blueprint asking for a nose fire gets it on a bomber"),
	          ResolveFireOrigin(EAircraftFireOrigin::NoseEngine, 4), EAircraftFireOrigin::NoseEngine);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAircraftDeathEngineChoiceTest, "RTS.Aircraft.Death.BurningEngineIsOnTheRightPart",
                                 AircraftDeathMathTestsPrivate::TestFlags)

bool FAircraftDeathEngineChoiceTest::RunTest(const FString& Parameters)
{
	using namespace AircraftDeathMathTestsPrivate;
	TestEqual(TEXT("The Bf 109's only propeller is its nose engine"), FAircraftDeathMath::PickNoseEngineIndex(Bf109Engines), 0);
	TestEqual(TEXT("The nose engine of a tri-motor is the one furthest forward"),
	          FAircraftDeathMath::PickNoseEngineIndex(TriMotorEngines), 0);
	TestEqual(TEXT("No engines, no nose engine"), FAircraftDeathMath::PickNoseEngineIndex({}), INDEX_NONE);
	for (int32 Seed = 0; Seed < 32; ++Seed)
	{
		const int32 TriMotorPick = FAircraftDeathMath::PickWingEngineIndex(TriMotorEngines, Seed);
		TestTrue(TEXT("A tri-motor's wing fire is never the nose engine"), TriMotorPick == 1 || TriMotorPick == 2);
		const int32 Pe8Pick = FAircraftDeathMath::PickWingEngineIndex(Pe8Engines, Seed);
		TestTrue(TEXT("A Pe-8 wing fire is one of its four engines"), Pe8Engines.IsValidIndex(Pe8Pick));
		TestEqual(TEXT("The same aircraft always burns on the same engine"), Pe8Pick,
		          FAircraftDeathMath::PickWingEngineIndex(Pe8Engines, Seed));
	}
	bool bSawLeft = false;
	bool bSawRight = false;
	for (int32 Seed = 0; Seed < 32; ++Seed)
	{
		const int32 Pick = FAircraftDeathMath::PickWingEngineIndex(Me410Engines, Seed);
		const EAircraftDamagedSide Side = FAircraftDeathMath::GetSideOfLocalPoint(Me410Engines[Pick]);
		bSawLeft |= Side == EAircraftDamagedSide::Left;
		bSawRight |= Side == EAircraftDamagedSide::Right;
	}
	TestTrue(TEXT("Me 410s burn on the left wing as well as on the right"), bSawLeft && bSawRight);
	TestEqual(TEXT("+Y is the right wing"), FAircraftDeathMath::GetSideOfLocalPoint(FVector(0.f, 300.f, 0.f)),
	          EAircraftDamagedSide::Right);
	TestEqual(TEXT("-Y is the left wing"), FAircraftDeathMath::GetSideOfLocalPoint(FVector(0.f, -300.f, 0.f)),
	          EAircraftDamagedSide::Left);
	TestEqual(TEXT("The centre line is no wing"), FAircraftDeathMath::GetSideOfLocalPoint(Bf109Engines[0]),
	          EAircraftDamagedSide::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAircraftDeathInitialVelocityTest, "RTS.Aircraft.Death.WreckKeepsTheSpeedItHad",
                                 AircraftDeathMathTestsPrivate::TestFlags)

bool FAircraftDeathInitialVelocityTest::RunTest(const FString& Parameters)
{
	constexpr float MinCrashSpeed = 900.f;
	const FVector Fast(3000.f, 400.f, -100.f);
	TestEqual(TEXT("A fast aircraft falls with exactly the velocity it had"),
	          FAircraftDeathMath::GetInitialCrashVelocity(Fast, FVector::ForwardVector, MinCrashSpeed), Fast);
	const FVector Hovering(0.f, 0.f, -50.f);
	const FVector FromHover = FAircraftDeathMath::GetInitialCrashVelocity(Hovering, FVector(0.f, 1.f, 0.f), MinCrashSpeed);
	TestTrue(TEXT("A hovering aircraft falls forward along its nose"), FromHover.Y >= MinCrashSpeed - 1.f);
	TestTrue(TEXT("A hovering aircraft keeps its sink"), FMath::IsNearlyEqual(FromHover.Z, -50.0, 0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAircraftDeathNoseFireFlightTest, "RTS.Aircraft.Death.NoseFireDivesWithItsSpeed",
                                 AircraftDeathMathTestsPrivate::TestFlags)

bool FAircraftDeathNoseFireFlightTest::RunTest(const FString& Parameters)
{
	using namespace AircraftDeathMathTestsPrivate;
	const FAircraftDeathSettings Settings;
	FAircraftCrashFlightState State = MakeLevelFlight(EAircraftDamagedSide::None);
	float HighestVelocityZ = 0.f;
	Fly(State, Settings, EAircraftFireOrigin::NoseEngine, 1.f, HighestVelocityZ);
	TestTrue(TEXT("The wreck never climbs"), HighestVelocityZ <= 1.f);
	TestTrue(TEXT("In its first second the wreck covers most of the ground its speed gives"),
	         State.Location.X >= FighterSpeed * 0.8f);
	TestTrue(TEXT("It has lost height after a second"), State.Location.Z < StartHeight - 100.f);
	float MaxRoll = 0.f;
	for (int32 Step = 0; Step < 120; ++Step)
	{
		FAircraftDeathMath::StepCrashFlight(State, Settings, EAircraftFireOrigin::NoseEngine, StepSeconds, EarthGravityZ);
		MaxRoll = FMath::Max(MaxRoll, FMath::Abs(State.RollDegrees));
	}
	TestTrue(TEXT("After three seconds the nose points down into a dive"),
	         FAircraftDeathMath::GetDiveDegrees(State.Velocity) > 30.f);
	TestTrue(TEXT("A nose fire only rocks the wreck, it does not roll it over"),
	         MaxRoll <= Settings.NoseFireWobbleDegrees + 0.01f);
	TestTrue(TEXT("The wreck keeps going in the direction it flew"), FMath::Abs(State.Location.Y) < 1.f);
	const FRotator Rotation = FAircraftDeathMath::GetCrashRotation(State, FRotator::ZeroRotator);
	TestTrue(TEXT("The nose points along the fall"),
	         FVector::DotProduct(Rotation.Vector(), State.Velocity.GetSafeNormal()) > 0.999f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAircraftDeathWingFireFlightTest, "RTS.Aircraft.Death.WingFireSpiralsTowardsTheBurningWing",
                                 AircraftDeathMathTestsPrivate::TestFlags)

bool FAircraftDeathWingFireFlightTest::RunTest(const FString& Parameters)
{
	using namespace AircraftDeathMathTestsPrivate;
	const FAircraftDeathSettings Settings;
	for (const EAircraftDamagedSide Side : {EAircraftDamagedSide::Right, EAircraftDamagedSide::Left})
	{
		const bool bIsRight = Side == EAircraftDamagedSide::Right;
		const FString SideName = bIsRight ? TEXT("right") : TEXT("left");
		FAircraftCrashFlightState State = MakeLevelFlight(Side);
		float HighestVelocityZ = 0.f;
		Fly(State, Settings, EAircraftFireOrigin::WingEngine, 0.75f, HighestVelocityZ);
		const float WingTipHeight = GetRightWingTipHeight(State);
		TestTrue(FString::Printf(TEXT("A %s wing fire drops the %s wing"), *SideName, *SideName),
		         bIsRight ? WingTipHeight < -50.f : WingTipHeight > 50.f);
		Fly(State, Settings, EAircraftFireOrigin::WingEngine, 1.5f, HighestVelocityZ);
		TestTrue(TEXT("The burning wing never lifts the wreck"), HighestVelocityZ <= 1.f);
		TestTrue(FString::Printf(TEXT("The wreck turns towards the %s wing"), *SideName),
		         bIsRight ? State.Velocity.Y > 100.f : State.Velocity.Y < -100.f);
		TestTrue(TEXT("The wreck rolls fast once the wing has lost its lift"),
		         FMath::Abs(State.RollRateDegreesPerSecond) > Settings.WingFireRollDegreesPerSecond * 0.5f);
		TestTrue(TEXT("It still falls with the speed it had"), State.Velocity.Size() > FighterSpeed * 0.75f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAircraftDeathDiveLimitTest, "RTS.Aircraft.Death.DiveSettlesAtTheMaximumAngle",
                                 AircraftDeathMathTestsPrivate::TestFlags)

bool FAircraftDeathDiveLimitTest::RunTest(const FString& Parameters)
{
	using namespace AircraftDeathMathTestsPrivate;
	FAircraftDeathSettings Settings;
	// Without gravity only the nose drop steers the wreck; it must stop at the configured dive.
	Settings.GravityScale = 0.f;
	Settings.DragPerSecond = 0.f;
	FAircraftCrashFlightState State = MakeLevelFlight(EAircraftDamagedSide::None);
	float HighestVelocityZ = 0.f;
	Fly(State, Settings, EAircraftFireOrigin::NoseEngine, 10.f, HighestVelocityZ);
	TestTrue(TEXT("The dive does not pass the maximum"),
	         FAircraftDeathMath::GetDiveDegrees(State.Velocity) <= Settings.MaxDiveDegrees + 0.5f);
	TestTrue(TEXT("The dive reaches the maximum"),
	         FAircraftDeathMath::GetDiveDegrees(State.Velocity) >= Settings.MaxDiveDegrees - 0.5f);
	TestTrue(TEXT("Without drag and gravity the speed stays what it was"),
	         FMath::IsNearlyEqual(State.Velocity.Size(), FighterSpeed, 1.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAircraftDeathCollectionNameTest, "RTS.Aircraft.Death.CrashCollectionNameFollowsMeshes",
                                 AircraftDeathMathTestsPrivate::TestFlags)

bool FAircraftDeathCollectionNameTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("The static frame names the collection of a frame-and-rotor aircraft"),
	          FAircraftDeathMath::GetCrashCollectionAssetName({TEXT("SM_PE2_Frame")}, {TEXT("SK_PE2_Rotors")}),
	          FString(TEXT("GC_Crash_SM_PE2_Frame")));
	TestEqual(TEXT("A skeletal-only aircraft is named after its mesh"),
	          FAircraftDeathMath::GetCrashCollectionAssetName({}, {TEXT("Bf109_WingAnims")}),
	          FString(TEXT("GC_Crash_Bf109_WingAnims")));
	TestEqual(TEXT("The order of the meshes does not change the name"),
	          FAircraftDeathMath::GetCrashCollectionAssetName({TEXT("SM_B"), TEXT("SM_A")}, {}),
	          FAircraftDeathMath::GetCrashCollectionAssetName({TEXT("SM_A"), TEXT("SM_B")}, {}));
	TestTrue(TEXT("No meshes, no collection"), FAircraftDeathMath::GetCrashCollectionAssetName({}, {}).IsEmpty());
	TestEqual(TEXT("Object path of a collection"),
	          FAircraftDeathMath::GetCrashCollectionObjectPath(TEXT("GC_Crash_SM_PE2_Frame")),
	          FString(TEXT("/Game/RTS_Survival/Blueprints/Aircraft/CrashCollections/GC_Crash_SM_PE2_Frame.GC_Crash_SM_PE2_Frame")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
