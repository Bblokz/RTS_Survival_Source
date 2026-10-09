#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorker.h"
#include "RTS_Survival/Player/SquadMovePreview/SquadMovePlanner.h"

namespace CoverFinderProneTestsPrivate
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	FCoverTraceObservation MakeHit(const float Distance, const FVector& ImpactNormal = FVector(-1.0f, 0.0f, 0.0f))
	{
		FCoverTraceObservation Trace;
		Trace.bBlockingHit = true;
		Trace.Distance = Distance;
		Trace.ImpactNormal = ImpactNormal;
		return Trace;
	}

	/** One probe direction that hit something at knee height and nothing else. */
	FCoverDirectionalObservation MakeLowObstacle(const float Distance)
	{
		FCoverDirectionalObservation Direction;
		Direction.SearchDirection = FVector::ForwardVector;
		Direction.LowerTrace = MakeHit(Distance);
		Direction.ProneCoverLocation = FVector(10.0f, 20.0f, 0.0f);
		Direction.bHasProneLyingSpace = true;
		return Direction;
	}

	TArray<FRTSCoverPoint> Classify(const FCoverDirectionalObservation& Direction)
	{
		FCoverProbeObservation Observation;
		Observation.ProjectedLocation = FVector(100.0f, 200.0f, 0.0f);
		Observation.DirectionalObservations.Add(Direction);
		TArray<FCoverProbeObservation> Observations;
		Observations.Add(Observation);
		TArray<FRTSCoverPoint> Candidates;
		FCoverFinderAlgorithms::AppendClassifiedCandidates(Observations, FCoverFinderSettingsSnapshot(), Candidates);
		return Candidates;
	}

	FRTSCoverPoint MakePoint(const FVector& Location, const ERTSCoverType CoverType, const int64 PointId = 0)
	{
		FRTSCoverPoint CoverPoint;
		CoverPoint.Location = Location;
		CoverPoint.CoverNormal = FVector(-1.0f, 0.0f, 0.0f);
		CoverPoint.CoverType = CoverType;
		CoverPoint.PointId = PointId;
		return CoverPoint;
	}

	int32 CountProne(const TArray<FRTSCoverPoint>& CoverPoints)
	{
		int32 Count = 0;
		for (const FRTSCoverPoint& CoverPoint : CoverPoints)
		{
			Count += CoverPoint.CoverType == ERTSCoverType::Prone ? 1 : 0;
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCoverFinderProneEvidenceTest,
	"RTS.CoverFinder.Prone.LowObstacleIsProneCover",
	CoverFinderProneTestsPrivate::TestFlags)

bool FCoverFinderProneEvidenceTest::RunTest(const FString& Parameters)
{
	using namespace CoverFinderProneTestsPrivate;
	const FCoverFinderSettingsSnapshot Settings;

	const TArray<FRTSCoverPoint> LowObstacle = Classify(MakeLowObstacle(90.0f));
	TestEqual(TEXT("Something at knee height with nothing above it gives one point"), LowObstacle.Num(), 1);
	if (LowObstacle.Num() == 1)
	{
		TestEqual(TEXT("That point is prone cover"), LowObstacle[0].CoverType, ERTSCoverType::Prone);
		TestTrue(TEXT("The soldier lies where the game thread found room, not where the probe stood"),
			LowObstacle[0].Location.Equals(FVector(10.0f, 20.0f, 0.0f)));
		TestTrue(TEXT("The cover normal points from the obstacle back to the soldier"),
			FVector::DotProduct(LowObstacle[0].CoverNormal, FVector::ForwardVector) < -0.9f);
	}

	FCoverDirectionalObservation NoRoom = MakeLowObstacle(90.0f);
	NoRoom.bHasProneLyingSpace = false;
	TestEqual(TEXT("Without room to lie down there is no prone point"), Classify(NoRoom).Num(), 0);

	FCoverDirectionalObservation TooTall = MakeLowObstacle(90.0f);
	TooTall.ProneFireOverTrace = MakeHit(95.0f);
	TestFalse(TEXT("An obstacle the soldier cannot fire over is no prone cover"),
		FCoverFinderAlgorithms::GetIsProneCoverEvidence(TooTall, Settings));

	FCoverDirectionalObservation Hillside = MakeLowObstacle(90.0f);
	Hillside.ProneFireOverTrace = MakeHit(90.0f + RTSCoverFinderConstants::ProneCoverClearDepth - 10.0f);
	TestFalse(TEXT("Ground that keeps rising behind the face is a hillside, not a bump"),
		FCoverFinderAlgorithms::GetIsProneCoverEvidence(Hillside, Settings));

	FCoverDirectionalObservation BumpWithHillBeyond = MakeLowObstacle(40.0f);
	BumpWithHillBeyond.ProneFireOverTrace = MakeHit(40.0f + RTSCoverFinderConstants::ProneCoverClearDepth + 5.0f);
	TestTrue(TEXT("Higher ground far enough behind the bump does not spoil it"),
		FCoverFinderAlgorithms::GetIsProneCoverEvidence(BumpWithHillBeyond, Settings));

	FCoverDirectionalObservation GentleRise = MakeLowObstacle(90.0f);
	GentleRise.LowerTrace.ImpactNormal = FVector(-0.17f, 0.0f, 0.98f);
	TestFalse(TEXT("A gentle rise in the ground is nothing to lie behind"),
		FCoverFinderAlgorithms::GetIsProneCoverEvidence(GentleRise, Settings));

	FCoverDirectionalObservation SteepBump = MakeLowObstacle(90.0f);
	SteepBump.LowerTrace.ImpactNormal = FVector(-0.64f, 0.0f, 0.77f);
	TestTrue(TEXT("A steep little bank is"), FCoverFinderAlgorithms::GetIsProneCoverEvidence(SteepBump, Settings));

	FCoverFinderSettingsSnapshot ProneOff;
	ProneOff.bFindProneCover = false;
	TestFalse(TEXT("The designer can switch prone cover off"),
		FCoverFinderAlgorithms::GetIsProneCoverEvidence(MakeLowObstacle(90.0f), ProneOff));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCoverFinderProneKeepsRegularCoverTest,
	"RTS.CoverFinder.Prone.RegularCoverIsNotReplaced",
	CoverFinderProneTestsPrivate::TestFlags)

bool FCoverFinderProneKeepsRegularCoverTest::RunTest(const FString& Parameters)
{
	using namespace CoverFinderProneTestsPrivate;
	// A wall up to crouch height: lower and crouch probe hit the same surface.
	FCoverDirectionalObservation CrouchWall = MakeLowObstacle(60.0f);
	CrouchWall.CrouchTrace = MakeHit(60.0f);
	const TArray<FRTSCoverPoint> CrouchCandidates = Classify(CrouchWall);
	TestEqual(TEXT("A crouch-high wall still gives exactly one point"), CrouchCandidates.Num(), 1);
	if (CrouchCandidates.Num() == 1)
	{
		TestEqual(TEXT("And it is still crouch cover"), CrouchCandidates[0].CoverType, ERTSCoverType::Crouch);
	}

	// An open frame found by the game thread wins over the prone rule in the same direction.
	FCoverDirectionalObservation Hedgehog = MakeLowObstacle(60.0f);
	Hedgehog.bOpenFrameCover = true;
	Hedgehog.OpenFrameCoverLocation = FVector(5.0f, 5.0f, 0.0f);
	const TArray<FRTSCoverPoint> FrameCandidates = Classify(Hedgehog);
	TestEqual(TEXT("An open frame gives one point"), FrameCandidates.Num(), 1);
	if (FrameCandidates.Num() == 1)
	{
		TestEqual(TEXT("And that stays crouch cover"), FrameCandidates[0].CoverType, ERTSCoverType::Crouch);
	}

	// Something tall right behind the low face, such as an overhang: not a place to lie and fire from.
	FCoverDirectionalObservation Overhang = MakeLowObstacle(60.0f);
	Overhang.CrouchTrace = MakeHit(170.0f);
	Overhang.CrouchTrace.BlockingProviderHandle = 7;
	Overhang.LowerTrace.BlockingProviderHandle = 3;
	TestEqual(TEXT("A tall object just behind the low one gives nothing"), Classify(Overhang).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCoverFinderProneSpacingTest,
	"RTS.CoverFinder.Prone.PointsAreSparse",
	CoverFinderProneTestsPrivate::TestFlags)

bool FCoverFinderProneSpacingTest::RunTest(const FString& Parameters)
{
	using namespace CoverFinderProneTestsPrivate;
	FCoverFinderSettingsSnapshot Settings;
	Settings.ProneCoverPointSpacing = 300.0f;

	// A ridge that gives a prone candidate every metre, facing two ways at each spot.
	TArray<FRTSCoverPoint> RawCandidates;
	for (int32 Step = 0; Step < 10; ++Step)
	{
		const FVector Location(0.0f, static_cast<float>(Step) * 100.0f, 0.0f);
		RawCandidates.Add(MakePoint(Location, ERTSCoverType::Prone));
		FRTSCoverPoint OtherWay = MakePoint(Location, ERTSCoverType::Prone);
		OtherWay.CoverNormal = FVector(1.0f, 0.0f, 0.0f);
		RawCandidates.Add(OtherWay);
	}
	// A crouch point on the ridge, and a prone candidate right on top of it.
	RawCandidates.Add(MakePoint(FVector(2000.0f, 0.0f, 0.0f), ERTSCoverType::Crouch));
	RawCandidates.Add(MakePoint(FVector(2030.0f, 0.0f, 0.0f), ERTSCoverType::Prone));

	const TArray<FRTSCoverPoint> FinalPoints = FCoverFinderAlgorithms::FinalizeCandidates(MoveTemp(RawCandidates), Settings);
	TestEqual(TEXT("Nine metres of ridge keep three prone points, each more than three metres from the next"),
		CountProne(FinalPoints), 3);
	TestEqual(TEXT("The crouch point is kept and gets no prone point on top of it"),
		FinalPoints.Num() - CountProne(FinalPoints), 1);
	TSet<int64> PointIds;
	for (const FRTSCoverPoint& FinalPoint : FinalPoints)
	{
		TestTrue(TEXT("Every published point has an ID"), FinalPoint.PointId != 0);
		PointIds.Add(FinalPoint.PointId);
	}
	TestEqual(TEXT("IDs are unique"), PointIds.Num(), FinalPoints.Num());

	// An authored prone point is the designer's decision and is never thinned out.
	TArray<FRTSCoverPoint> WithAuthored;
	WithAuthored.Add(MakePoint(FVector::ZeroVector, ERTSCoverType::Prone));
	FRTSCoverPoint Authored = MakePoint(FVector(100.0f, 0.0f, 0.0f), ERTSCoverType::Prone);
	Authored.ProviderRegistrationId = 5;
	WithAuthored.Add(Authored);
	TestTrue(TEXT("An authored prone point survives next to a found one"),
		FCoverFinderAlgorithms::FinalizeCandidates(MoveTemp(WithAuthored), Settings).ContainsByPredicate(
			[](const FRTSCoverPoint& CoverPoint) { return CoverPoint.ProviderRegistrationId == 5; }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCoverFinderProneStabilityTest,
	"RTS.CoverFinder.Prone.PublishedPointsKeepTheirPlace",
	CoverFinderProneTestsPrivate::TestFlags)

bool FCoverFinderProneStabilityTest::RunTest(const FString& Parameters)
{
	using namespace CoverFinderProneTestsPrivate;
	FCoverFinderSettingsSnapshot Settings;
	Settings.ProneCoverPointSpacing = 300.0f;
	const auto MakeRidge = [](const int32 FirstStep)
	{
		TArray<FRTSCoverPoint> Candidates;
		for (int32 Step = FirstStep; Step < 10; ++Step)
		{
			Candidates.Add(MakePoint(FVector(0.0f, static_cast<float>(Step) * 100.0f, 0.0f), ERTSCoverType::Prone));
		}
		return Candidates;
	};
	const auto GetProneIds = [](const TArray<FRTSCoverPoint>& CoverPoints)
	{
		TSet<int64> PointIds;
		for (const FRTSCoverPoint& CoverPoint : CoverPoints)
		{
			PointIds.Add(CoverPoint.PointId);
		}
		return PointIds;
	};
	const TSet<int64> FirstScanIds = GetProneIds(FCoverFinderAlgorithms::FinalizeCandidates(MakeRidge(1), Settings));

	// The next scan finds one more candidate at the start of the ridge. Thinning from scratch would now keep
	// that one and shift every point after it.
	const TSet<int64> FromScratchIds = GetProneIds(FCoverFinderAlgorithms::FinalizeCandidates(MakeRidge(0), Settings));
	TestFalse(TEXT("Without a preference a new candidate reshuffles the points"),
		FromScratchIds.Includes(FirstScanIds));
	const TSet<int64> SecondScanIds = GetProneIds(
		FCoverFinderAlgorithms::FinalizeCandidates(MakeRidge(0), Settings, &FirstScanIds));
	TestTrue(TEXT("With the published points preferred, every one of them is published again"),
		SecondScanIds.Includes(FirstScanIds));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCoverFinderProneCompanionTest,
	"RTS.CoverFinder.Prone.CompanionsBesideOtherCover",
	CoverFinderProneTestsPrivate::TestFlags)

bool FCoverFinderProneCompanionTest::RunTest(const FString& Parameters)
{
	using namespace CoverFinderProneTestsPrivate;
	constexpr int32 SourceCount = 2000;
	FCoverFinderSettingsSnapshot Settings;
	Settings.ProneCompanionChancePercent = 20;

	int32 CompanionCount = 0;
	for (int32 SourceIndex = 0; SourceIndex < SourceCount; ++SourceIndex)
	{
		// IDs as spread out as real ones are.
		const int64 PointId = static_cast<int64>(GetTypeHash(SourceIndex)) * 7919 + SourceIndex;
		const FRTSCoverPoint Source = MakePoint(FVector(1000.0f, 0.0f, 0.0f), ERTSCoverType::Crouch, PointId);
		FRTSCoverPoint Companion;
		if (not FCoverFinderAlgorithms::TryBuildProneCompanion(Source, Settings, Companion))
		{
			continue;
		}
		++CompanionCount;
		FRTSCoverPoint Repeated;
		FCoverFinderAlgorithms::TryBuildProneCompanion(Source, Settings, Repeated);
		TestTrue(TEXT("The same point gets the same companion on every scan"),
			Repeated.Location.Equals(Companion.Location) && Repeated.CoverNormal.Equals(Companion.CoverNormal));
		TestEqual(TEXT("A companion is prone cover"), Companion.CoverType, ERTSCoverType::Prone);
		const float SidewaysDistance = FMath::Abs(Companion.Location.Y - Source.Location.Y);
		TestTrue(TEXT("It lies the configured distance to the side"),
			FMath::IsNearlyEqual(SidewaysDistance, Settings.ProneCompanionOffset, 1.0f));
		TestTrue(TEXT("It faces roughly the way its neighbour does"),
			FVector::DotProduct(Companion.CoverNormal, Source.CoverNormal) > 0.95f);
	}
	const float CompanionShare = static_cast<float>(CompanionCount) / static_cast<float>(SourceCount);
	TestTrue(TEXT("About the configured share of points gets a companion"),
		CompanionShare > 0.14f && CompanionShare < 0.26f);

	FRTSCoverPoint Unused;
	Settings.ProneCompanionChancePercent = 100;
	FRTSCoverPoint StandingLeft = MakePoint(FVector::ZeroVector, ERTSCoverType::StandingLeft, 12345);
	FRTSCoverPoint StandingRight = MakePoint(FVector::ZeroVector, ERTSCoverType::StandingRight, 12345);
	FRTSCoverPoint LeftCompanion;
	FRTSCoverPoint RightCompanion;
	TestTrue(TEXT("Standing cover gets companions too"),
		FCoverFinderAlgorithms::TryBuildProneCompanion(StandingLeft, Settings, LeftCompanion) &&
		FCoverFinderAlgorithms::TryBuildProneCompanion(StandingRight, Settings, RightCompanion));
	// Facing -normal = +X, so the soldier's right is +Y. A left-peek point steps out to -Y; its wall is at +Y.
	TestTrue(TEXT("The companion of a left-peek point lies on the wall side, to its right"), LeftCompanion.Location.Y > 0.0f);
	TestTrue(TEXT("The companion of a right-peek point lies to its left"), RightCompanion.Location.Y < 0.0f);

	TestFalse(TEXT("Prone points get no companions of their own"),
		FCoverFinderAlgorithms::TryBuildProneCompanion(MakePoint(FVector::ZeroVector, ERTSCoverType::Prone, 1), Settings, Unused));
	TestFalse(TEXT("Neither do trench firing steps"),
		FCoverFinderAlgorithms::TryBuildProneCompanion(
			MakePoint(FVector::ZeroVector, ERTSCoverType::TrenchStandUp, 1), Settings, Unused));
	Settings.ProneCompanionChancePercent = 0;
	TestFalse(TEXT("A chance of zero adds none"),
		FCoverFinderAlgorithms::TryBuildProneCompanion(MakePoint(FVector::ZeroVector, ERTSCoverType::Crouch, 1), Settings, Unused));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCoverFinderProneLowObstacleRingTest,
	"RTS.CoverFinder.Prone.LowObjectsAreRinged",
	CoverFinderProneTestsPrivate::TestFlags)

bool FCoverFinderProneLowObstacleRingTest::RunTest(const FString& Parameters)
{
	FCoverFinderSettingsSnapshot Settings;
	// A crate 50 cm tall and 80 cm across: too low to crouch behind.
	const FBox CrateBounds(FVector(-40.0f, -40.0f, 0.0f), FVector(40.0f, 40.0f, 50.0f));
	TArray<FCoverFocusedSample> RingSamples;
	TestTrue(TEXT("A low crate gets a ring of probes"),
		FCoverFinderAlgorithms::AppendThinObstacleSamples(CrateBounds, Settings, RingSamples));
	TestFalse(TEXT("None of them may publish open-frame crouch cover behind something that low"),
		RingSamples.ContainsByPredicate([](const FCoverFocusedSample& Sample) { return Sample.bMayFindOpenFrameCover; }));

	const FBox KerbBounds(FVector(-40.0f, -40.0f, 0.0f), FVector(40.0f, 40.0f, 20.0f));
	TestFalse(TEXT("Something below knee height gets none"),
		FCoverFinderAlgorithms::GetIsThinObstacle(KerbBounds, Settings));

	Settings.bFindProneCover = false;
	TestFalse(TEXT("With prone cover off the crate is skipped as before"),
		FCoverFinderAlgorithms::GetIsThinObstacle(CrateBounds, Settings));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCoverFinderPronePlannerTest,
	"RTS.CoverFinder.Prone.SquadMovePreviewUsesProneCover",
	CoverFinderProneTestsPrivate::TestFlags)

bool FCoverFinderPronePlannerTest::RunTest(const FString& Parameters)
{
	using namespace CoverFinderProneTestsPrivate;
	FSquadMovePlanRequest Request;
	Request.Anchor = FVector(2000.0f, 0.0f, 0.0f);
	Request.Facing = FVector::ForwardVector;
	Request.SquadCount = 1;
	for (int32 UnitIndex = 0; UnitIndex < 3; ++UnitIndex)
	{
		FSquadMovePlannerUnit& Unit = Request.Units.AddDefaulted_GetRef();
		Unit.SquadIndex = 0;
		Unit.Location = FVector(0.0f, static_cast<float>(UnitIndex) * 100.0f, 0.0f);
	}
	Request.CoverPoints.Add(MakePoint(FVector(2050.0f, 120.0f, 0.0f), ERTSCoverType::Prone, 11));
	Request.CoverPoints.Add(MakePoint(FVector(2050.0f, -150.0f, 0.0f), ERTSCoverType::Crouch, 12));

	FSquadMovePlan Plan;
	FSquadMovePlanner::BuildPlan(Request, Plan);
	int32 ProneCount = 0;
	int32 CrouchCount = 0;
	for (const FSquadUnitPlannedPosition& Position : Plan.UnitPositions)
	{
		ProneCount += Position.Type == ESquadPlannedPositionType::ProneCover ? 1 : 0;
		CrouchCount += Position.Type == ESquadPlannedPositionType::CrouchCover ? 1 : 0;
		if (Position.Type == ESquadPlannedPositionType::ProneCover)
		{
			TestTrue(TEXT("A prone position counts as cover"), Position.GetIsCover());
			TestEqual(TEXT("And carries its prone cover point"), Position.CoverPoint.CoverType, ERTSCoverType::Prone);
		}
	}
	TestEqual(TEXT("Every soldier gets a position"), Plan.UnitPositions.Num(), 3);
	TestEqual(TEXT("The prone point near the cursor is planned as prone cover"), ProneCount, 1);
	TestEqual(TEXT("The crouch point beside it stays crouch cover"), CrouchCount, 1);
	return true;
}

#endif
