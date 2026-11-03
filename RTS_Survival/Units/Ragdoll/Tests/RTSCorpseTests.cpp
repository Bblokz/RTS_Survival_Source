// Copyright (C) Bas Blokzijl - All rights reserved.

// QA for battlefield corpses: a frozen crew body leaves a corpse and returns to the pool at once, Off keeps the old
// sinking, the cap removes the oldest corpse first and reuses its slot, a lowered setting trims and gives the memory
// back, and the chosen pose lies like the body did. Runs under -nullrhi in the hand-ticked ragdoll test world.
#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/StaticMesh.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "RTS_Survival/Units/Ragdoll/RTSCorpsePoseLibrary.h"
#include "RTS_Survival/Units/Ragdoll/RTSCorpseSubsystem.h"
#include "RTS_Survival/Units/Ragdoll/RTSRagdollSettings.h"
#include "RTS_Survival/Units/Ragdoll/Tests/RagdollTestWorld.h"

namespace RTSCorpseTestsPrivate
{
	using RagdollTestWorld::FRagdollTestWorld;
	using RagdollTestWorld::PrewarmSeconds;

	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	const FVector VehicleTopCentre(0.f, 0.f, 250.f);
	constexpr float VehicleSpreadRadiusCm = 100.f;
	// Freeze happens at MaxSimulationSeconds; the old sinking would take SinkSeconds more.
	constexpr float AfterFreezeSeconds = 0.25f;
	constexpr float GroundToleranceCm = 2.f;
	constexpr float HighThrowHeightCm = 2000.f;
	// A cook-off throws bodies up to about 10 m out.
	constexpr float MaxThrowDistanceCm = 1500.f;
	constexpr float MinThrowDistanceCm = 200.f;
	constexpr int32 SmallCap = 3;
	constexpr int32 CorpsesBeyondCap = 12;
	constexpr float CorpseSpacingCm = 400.f;
	constexpr float PelvisHeightCm = 20.f;
	constexpr int32 TrimCapBefore = 10;
	constexpr int32 TrimCapAfter = 2;
	// The trim runs on a one second timer.
	constexpr float TrimWaitSeconds = 1.2f;
	constexpr float AxisMatchDot = 0.99f;
	const float TestYawsDegrees[] = {0.f, 75.f, 200.f};

	/** Sets rts.Corpses for one test and puts the player's value back afterwards. */
	struct FScopedCorpseCap
	{
		IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("rts.Corpses"));
		int32 Previous = CVar ? CVar->GetInt() : 0;

		explicit FScopedCorpseCap(const int32 Cap) { Set(Cap); }
		~FScopedCorpseCap() { Set(Previous); }

		void Set(const int32 Cap) const
		{
			if (CVar != nullptr)
			{
				CVar->Set(Cap, ECVF_SetByCode);
			}
		}
	};

	FRTSCrewBodyRequest MakeCookOff()
	{
		FRTSCrewBodyRequest Request;
		Request.TopCentre = VehicleTopCentre;
		Request.SpreadRadius = VehicleSpreadRadiusCm;
		Request.Side = ERTSCrewSide::German;
		Request.bIsCookOff = true;
		return Request;
	}

	FRTSCorpseRequest MakeCorpse(const FVector& PelvisLocation, const FQuat& PelvisRotation)
	{
		FRTSCorpseRequest Request;
		Request.Side = ERTSCrewSide::German;
		Request.Pelvis = FTransform(PelvisRotation, PelvisLocation);
		return Request;
	}

	URTSCorpseSubsystem* GetCorpses(const FRagdollTestWorld& TestWorld)
	{
		return TestWorld.World ? TestWorld.World->GetSubsystem<URTSCorpseSubsystem>() : nullptr;
	}

	/** Ticks until no thrown body simulates any more, at most until the landing grace ran out. */
	void TickUntilAllFrozen(const FRagdollTestWorld& TestWorld, const URTSRagdollSubsystem& Ragdolls)
	{
		const URTSRagdollSettings* Settings = GetDefault<URTSRagdollSettings>();
		const float LimitSeconds = Settings->MaxSimulationSeconds + Settings->LandingGraceSeconds + AfterFreezeSeconds;
		for (float Seconds = 0.f; Seconds < LimitSeconds && Ragdolls.GetSimulatingRagdollCount() > 0;
		     Seconds += RagdollTestWorld::FrameSeconds)
		{
			TestWorld.TickFor(RagdollTestWorld::FrameSeconds);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRTSCorpseFrozenCrewTest,
	"RTS.Ragdoll.Corpses.FrozenCrewLeavesCorpseAndReturnsAtOnce",
	RTSCorpseTestsPrivate::TestFlags)

bool FRTSCorpseFrozenCrewTest::RunTest(const FString& Parameters)
{
	using namespace RTSCorpseTestsPrivate;
	const FScopedCorpseCap Cap(100);
	const FRagdollTestWorld TestWorld;
	URTSRagdollSubsystem* Ragdolls = TestWorld.GetSubsystem();
	URTSCorpseSubsystem* Corpses = GetCorpses(TestWorld);
	if (not TestNotNull(TEXT("Ragdoll subsystem exists"), Ragdolls) || not TestNotNull(TEXT("Corpse subsystem exists"), Corpses))
	{
		return false;
	}
	const URTSRagdollSettings* Settings = GetDefault<URTSRagdollSettings>();
	TestWorld.TickFor(PrewarmSeconds);
	const int32 Thrown = Ragdolls->RequestCrewBodies(MakeCookOff());
	TickUntilAllFrozen(TestWorld, *Ragdolls);

	TestEqual(TEXT("Every frozen body left a corpse"), Corpses->GetCorpseCount(), Thrown);
	TestEqual(TEXT("Bodies are back in the pool before a sink could have ended"),
	          Ragdolls->GetParkedBodyCount(ERTSCrewSide::German), Settings->PoolSizePerSide);
	TestEqual(TEXT("Nothing simulates any more"), Ragdolls->GetSimulatingRagdollCount(), 0);
	for (int32 CorpseIndex = 0; CorpseIndex < Corpses->GetCorpseCount(); ++CorpseIndex)
	{
		int32 Side = INDEX_NONE;
		int32 Pose = INDEX_NONE;
		FTransform Corpse;
		if (not TestTrue(TEXT("Corpse can be read"), Corpses->GetCorpse(CorpseIndex, Side, Pose, Corpse)))
		{
			continue;
		}
		TestEqual(TEXT("A German crew body leaves a German corpse"), Side, static_cast<int32>(ERTSCrewSide::German));
		TestTrue(TEXT("Corpse lies on the ground"), FMath::Abs(Corpse.GetLocation().Z) < GroundToleranceCm);
		// Bodies wake within the spread and a cook-off throws them metres out; a corpse inside the spread was placed
		// where its body woke, not where it landed.
		const float ThrowDistance = FVector::Dist2D(Corpse.GetLocation(), VehicleTopCentre);
		TestTrue(FString::Printf(TEXT("Corpse lies where the body landed (%.0f cm out)"), ThrowDistance),
		         ThrowDistance > MinThrowDistanceCm && ThrowDistance < MaxThrowDistanceCm);
		TestTrue(TEXT("Corpse has full size"), Corpse.GetScale3D().Equals(FVector::OneVector));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRTSCorpseOffTest,
	"RTS.Ragdoll.Corpses.OffLetsBodiesSinkAsBefore",
	RTSCorpseTestsPrivate::TestFlags)

bool FRTSCorpseOffTest::RunTest(const FString& Parameters)
{
	using namespace RTSCorpseTestsPrivate;
	const FScopedCorpseCap Cap(0);
	const FRagdollTestWorld TestWorld;
	URTSRagdollSubsystem* Ragdolls = TestWorld.GetSubsystem();
	URTSCorpseSubsystem* Corpses = GetCorpses(TestWorld);
	if (not TestNotNull(TEXT("Ragdoll subsystem exists"), Ragdolls) || not TestNotNull(TEXT("Corpse subsystem exists"), Corpses))
	{
		return false;
	}
	const URTSRagdollSettings* Settings = GetDefault<URTSRagdollSettings>();
	TestWorld.TickFor(PrewarmSeconds);
	Ragdolls->RequestCrewBodies(MakeCookOff());
	TickUntilAllFrozen(TestWorld, *Ragdolls);
	TestEqual(TEXT("No corpse is left when they are off"), Corpses->GetCorpseCount(), 0);
	TestTrue(TEXT("Bodies are still sinking"),
	         Ragdolls->GetParkedBodyCount(ERTSCrewSide::German) < Settings->PoolSizePerSide);
	TestWorld.TickFor(Settings->SinkSeconds);
	TestEqual(TEXT("Sunk bodies are back in the pool"), Ragdolls->GetParkedBodyCount(ERTSCrewSide::German),
	          Settings->PoolSizePerSide);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRTSCorpseHighThrowTest,
	"RTS.Ragdoll.Corpses.HighThrowLandsBeforeItFreezes",
	RTSCorpseTestsPrivate::TestFlags)

bool FRTSCorpseHighThrowTest::RunTest(const FString& Parameters)
{
	using namespace RTSCorpseTestsPrivate;
	const FScopedCorpseCap Cap(100);
	const FRagdollTestWorld TestWorld;
	URTSRagdollSubsystem* Ragdolls = TestWorld.GetSubsystem();
	URTSCorpseSubsystem* Corpses = GetCorpses(TestWorld);
	if (not TestNotNull(TEXT("Ragdoll subsystem exists"), Ragdolls) || not TestNotNull(TEXT("Corpse subsystem exists"), Corpses))
	{
		return false;
	}
	const URTSRagdollSettings* Settings = GetDefault<URTSRagdollSettings>();
	TestWorld.TickFor(PrewarmSeconds);
	FRTSCrewBodyRequest Request = MakeCookOff();
	// Thrown from 20 m up, the bodies are still falling when the time limit comes.
	Request.TopCentre.Z = HighThrowHeightCm;
	const int32 Thrown = Ragdolls->RequestCrewBodies(Request);
	TestWorld.TickFor(Settings->MaxSimulationSeconds + AfterFreezeSeconds);
	TestEqual(TEXT("Bodies still in the air keep simulating past the time limit"),
	          Ragdolls->GetSimulatingRagdollCount(), Thrown);
	TestEqual(TEXT("No corpse is left in the air"), Corpses->GetCorpseCount(), 0);
	TickUntilAllFrozen(TestWorld, *Ragdolls);
	TestEqual(TEXT("Once down every body left a corpse"), Corpses->GetCorpseCount(), Thrown);
	for (int32 CorpseIndex = 0; CorpseIndex < Corpses->GetCorpseCount(); ++CorpseIndex)
	{
		int32 Side = INDEX_NONE;
		int32 Pose = INDEX_NONE;
		FTransform Corpse;
		Corpses->GetCorpse(CorpseIndex, Side, Pose, Corpse);
		TestTrue(FString::Printf(TEXT("The corpse lies on the ground (at %s)"), *Corpse.GetLocation().ToCompactString()),
		         FMath::Abs(Corpse.GetLocation().Z) < GroundToleranceCm);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRTSCorpseCapTest,
	"RTS.Ragdoll.Corpses.CapRemovesOldestAndReusesSlots",
	RTSCorpseTestsPrivate::TestFlags)

bool FRTSCorpseCapTest::RunTest(const FString& Parameters)
{
	using namespace RTSCorpseTestsPrivate;
	const FScopedCorpseCap Cap(SmallCap);
	const FRagdollTestWorld TestWorld;
	URTSCorpseSubsystem* Corpses = GetCorpses(TestWorld);
	if (not TestNotNull(TEXT("Corpse subsystem exists"), Corpses))
	{
		return false;
	}
	int32 MostInstances = 0;
	for (int32 CorpseNumber = 0; CorpseNumber < CorpsesBeyondCap; ++CorpseNumber)
	{
		const FQuat Rotation = FRotator(0.f, CorpseNumber * 37.f, CorpseNumber * 53.f).Quaternion();
		TestTrue(TEXT("Corpse is added"), Corpses->TryAddCorpse(
			         MakeCorpse(FVector(CorpseNumber * CorpseSpacingCm, 0.f, PelvisHeightCm), Rotation)));
		MostInstances = FMath::Max(MostInstances, Corpses->GetInstanceCount());
	}
	TestEqual(TEXT("Only the cap stays"), Corpses->GetCorpseCount(), SmallCap);
	for (int32 CorpseIndex = 0; CorpseIndex < SmallCap; ++CorpseIndex)
	{
		int32 Side = INDEX_NONE;
		int32 Pose = INDEX_NONE;
		FTransform Corpse;
		Corpses->GetCorpse(CorpseIndex, Side, Pose, Corpse);
		const float ExpectedX = (CorpsesBeyondCap - SmallCap + CorpseIndex) * CorpseSpacingCm;
		TestTrue(TEXT("The newest corpses stay, oldest first"),
		         FMath::IsNearlyEqual(Corpse.GetLocation().X, ExpectedX, GroundToleranceCm));
	}
	// Slots of removed corpses are reused or compacted, so memory never grows past twice the cap.
	TestTrue(TEXT("Instances stay bounded by the cap"), MostInstances <= 2 * SmallCap);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRTSCorpseTrimTest,
	"RTS.Ragdoll.Corpses.LoweredSettingTrimsAndFreesMemory",
	RTSCorpseTestsPrivate::TestFlags)

bool FRTSCorpseTrimTest::RunTest(const FString& Parameters)
{
	using namespace RTSCorpseTestsPrivate;
	const FScopedCorpseCap Cap(TrimCapBefore);
	const FRagdollTestWorld TestWorld;
	URTSCorpseSubsystem* Corpses = GetCorpses(TestWorld);
	if (not TestNotNull(TEXT("Corpse subsystem exists"), Corpses))
	{
		return false;
	}
	for (int32 CorpseNumber = 0; CorpseNumber < TrimCapBefore; ++CorpseNumber)
	{
		Corpses->TryAddCorpse(MakeCorpse(FVector(CorpseNumber * CorpseSpacingCm, 0.f, PelvisHeightCm),
		                                 FRotator(0.f, CorpseNumber * 41.f, 90.f).Quaternion()));
	}
	TestEqual(TEXT("All corpses fit under the first cap"), Corpses->GetCorpseCount(), TrimCapBefore);
	Cap.Set(TrimCapAfter);
	TestWorld.TickFor(TrimWaitSeconds);
	TestEqual(TEXT("A lowered setting removes the oldest corpses"), Corpses->GetCorpseCount(), TrimCapAfter);
	TestEqual(TEXT("Their instances are given back"), Corpses->GetInstanceCount(), TrimCapAfter);
	int32 Side = INDEX_NONE;
	int32 Pose = INDEX_NONE;
	FTransform Newest;
	Corpses->GetCorpse(TrimCapAfter - 1, Side, Pose, Newest);
	TestTrue(TEXT("The newest corpse is kept"), FMath::IsNearlyEqual(
		         Newest.GetLocation().X, (TrimCapBefore - 1) * CorpseSpacingCm, GroundToleranceCm));
	Cap.Set(0);
	TestFalse(TEXT("Off refuses new corpses"), Corpses->TryAddCorpse(
		          MakeCorpse(FVector(0.f, 0.f, PelvisHeightCm), FQuat::Identity)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRTSCorpsePoseMatchTest,
	"RTS.Ragdoll.Corpses.PoseLiesLikeTheBody",
	RTSCorpseTestsPrivate::TestFlags)

bool FRTSCorpsePoseMatchTest::RunTest(const FString& Parameters)
{
	using namespace RTSCorpseTestsPrivate;
	const FScopedCorpseCap Cap(100);
	const FRagdollTestWorld TestWorld;
	URTSCorpseSubsystem* Corpses = GetCorpses(TestWorld);
	URTSRagdollSettings* Settings = GetMutableDefault<URTSRagdollSettings>();
	const URTSCorpsePoseLibrary* Library = Settings->GermanCorpsePoses.LoadSynchronous();
	const URTSCorpsePoseLibrary* SovietLibrary = Settings->SovietCorpsePoses.LoadSynchronous();
	if (not TestNotNull(TEXT("Corpse subsystem exists"), Corpses) || not TestNotNull(TEXT("German poses exist"), Library)
		|| not TestNotNull(TEXT("Soviet poses exist"), SovietLibrary))
	{
		return false;
	}
	TestTrue(TEXT("German library holds poses"), Library->Poses.Num() > 0);
	TestTrue(TEXT("Soviet library holds poses"), SovietLibrary->Poses.Num() > 0);
	// Only the best match, so the test knows which pose to expect.
	const float PreviousSlack = Settings->PoseMatchSlack;
	Settings->PoseMatchSlack = 0.f;
	for (int32 PoseIndex = 0; PoseIndex < Library->Poses.Num(); ++PoseIndex)
	{
		const FRTSCorpsePose& Pose = Library->Poses[PoseIndex];
		const FQuat PoseRotation(FMatrix(Pose.PelvisAxisX, Pose.PelvisAxisY, Pose.PelvisAxisZ, FVector::ZeroVector));
		for (const float YawDegrees : TestYawsDegrees)
		{
			const FQuat BodyRotation = FQuat(FVector::UpVector, FMath::DegreesToRadians(YawDegrees)) * PoseRotation;
			Corpses->TryAddCorpse(MakeCorpse(FVector(0.f, 0.f, PelvisHeightCm), BodyRotation));
			int32 Side = INDEX_NONE;
			int32 ChosenPose = INDEX_NONE;
			FTransform Corpse;
			Corpses->GetCorpse(Corpses->GetCorpseCount() - 1, Side, ChosenPose, Corpse);
			TestEqual(TEXT("A body lying exactly like a pose gets that pose"), ChosenPose, PoseIndex);
			const FVector PlacedAxes[] = {
				Corpse.GetRotation().RotateVector(Pose.PelvisAxisX), Corpse.GetRotation().RotateVector(Pose.PelvisAxisY),
				Corpse.GetRotation().RotateVector(Pose.PelvisAxisZ)
			};
			const FVector BodyAxes[] = {
				BodyRotation.GetAxisX(), BodyRotation.GetAxisY(), BodyRotation.GetAxisZ()
			};
			for (int32 Axis = 0; Axis < UE_ARRAY_COUNT(BodyAxes); ++Axis)
			{
				TestTrue(TEXT("The corpse is turned to the body's heading"),
				         FVector::DotProduct(PlacedAxes[Axis], BodyAxes[Axis]) > AxisMatchDot);
			}
		}
	}
	Settings->PoseMatchSlack = PreviousSlack;
	return true;
}

#endif
