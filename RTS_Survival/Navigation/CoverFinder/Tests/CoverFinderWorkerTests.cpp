#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorldSubsystem.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorker.h"
#include "RTS_Survival/Units/Squads/SquadUnit/AnimSquadUnit/SquadUnitAnimInstance.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

namespace CoverFinderWorkerTestsPrivate
{
	FCoverTraceObservation MakeTrace(
		const bool bBlockingHit,
		const float Distance,
		const uint64 BlockingProviderHandle = 0)
	{
		FCoverTraceObservation Trace;
		Trace.bBlockingHit = bBlockingHit;
		Trace.Distance = Distance;
		Trace.ImpactNormal = FVector(-1.0f, 0.0f, 0.0f);
		Trace.BlockingProviderHandle = BlockingProviderHandle;
		return Trace;
	}

	FCoverProbeObservation MakeObservation(
		const bool bStandingHit,
		const bool bLeftGapOpen,
		const bool bRightGapOpen,
		const float StandingDistance = 50.0f)
	{
		FCoverProbeObservation Observation;
		Observation.ProjectedLocation = FVector(100.0f, 200.0f, 0.0f);
		FCoverDirectionalObservation& Direction = Observation.DirectionalObservations.AddDefaulted_GetRef();
		Direction.SearchDirection = FVector::ForwardVector;
		Direction.LowerTrace = MakeTrace(true, 50.0f);
		Direction.CrouchTrace = MakeTrace(true, 50.0f);
		Direction.StandingTrace = MakeTrace(bStandingHit, StandingDistance);
		Direction.LeftCoverLocation = Observation.ProjectedLocation;
		Direction.RightCoverLocation = Observation.ProjectedLocation;
		Direction.bLeftGapOpen = bLeftGapOpen;
		Direction.bRightGapOpen = bRightGapOpen;
		return Observation;
	}

	TArray<FRTSCoverPoint> Classify(const FCoverProbeObservation& Observation)
	{
		FCoverFinderSettingsSnapshot Settings;
		TArray<FCoverProbeObservation> Observations;
		Observations.Add(Observation);
		TArray<FRTSCoverPoint> Candidates;
		FCoverFinderAlgorithms::AppendClassifiedCandidates(Observations, Settings, Candidates);
		return Candidates;
	}

	FRTSCoverPoint MakeCandidate(
		const FVector& Location,
		const FVector& Normal,
		const ERTSCoverType CoverType)
	{
		FRTSCoverPoint Candidate;
		Candidate.Location = Location;
		Candidate.CoverNormal = Normal;
		Candidate.CoverType = CoverType;
		return Candidate;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCoverFinderThinObstacleTest,
	"RTS.CoverFinder.Worker.ThinObstacleRing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCoverFinderThinObstacleTest::RunTest(const FString& Parameters)
{
	const FCoverFinderSettingsSnapshot Settings;
	// A tree trunk: 60 cm across, four metres tall, standing away from any grid line.
	const FVector TrunkBase(1234.0f, -567.0f, 80.0f);
	const FBox TrunkBounds(TrunkBase - FVector(30.0f, 30.0f, 0.0f), TrunkBase + FVector(30.0f, 30.0f, 400.0f));
	TArray<FCoverFocusedSample> RingSamples;
	TestTrue(TEXT("A trunk is a thin obstacle"),
		FCoverFinderAlgorithms::AppendThinObstacleSamples(TrunkBounds, Settings, RingSamples));
	TestEqual(TEXT("The ring has the configured number of probes"),
		RingSamples.Num(), Settings.ThinObstacleRingSampleCount);
	const float TrunkCornerDistance = FVector2D(30.0f, 30.0f).Size();
	for (const FCoverFocusedSample& Sample : RingSamples)
	{
		TestTrue(TEXT("Every probe looks at the middle of the trunk"), Sample.AimLocation.Equals(TrunkBase, 0.1f));
		TestTrue(TEXT("Every probe stands at the foot of the trunk"), FMath::IsNearlyEqual(Sample.Location.Z, TrunkBase.Z, 0.1f));
		TestTrue(TEXT("A soldier's capsule on the probe position clears the trunk"),
			FVector::Dist2D(Sample.Location, TrunkBase) >= TrunkCornerDistance + Settings.AgentRadius);
		TestTrue(TEXT("The trunk is inside the probe's reach"),
			FVector::Dist2D(Sample.Location, TrunkBase) < Settings.MaximumCoverSearchDistance);
	}
	int32 OpenFrameProbeCount = 0;
	for (const FCoverFocusedSample& Sample : RingSamples)
	{
		OpenFrameProbeCount += Sample.bMayFindOpenFrameCover ? 1 : 0;
	}
	TestEqual(TEXT("Only the configured number of ring probes may publish open-frame cover"),
		OpenFrameProbeCount, Settings.OpenFramePointsPerObstacle);
	TestTrue(TEXT("Those probes are spread around the ring, not bunched on one side"),
		RingSamples[0].bMayFindOpenFrameCover && not RingSamples[1].bMayFindOpenFrameCover &&
		RingSamples[3].bMayFindOpenFrameCover && RingSamples[6].bMayFindOpenFrameCover);
	TestTrue(TEXT("The probes surround the trunk"),
		FVector::DotProduct(RingSamples[0].Location - TrunkBase, RingSamples[4].Location - TrunkBase) < 0.0f);

	// The same trunk somewhere else gets the same ring: nothing depends on where the scan grid falls.
	const FVector Shift(73.0f, 41.0f, 0.0f);
	TArray<FCoverFocusedSample> ShiftedSamples;
	FCoverFinderAlgorithms::AppendThinObstacleSamples(TrunkBounds.ShiftBy(Shift), Settings, ShiftedSamples);
	TestTrue(TEXT("A trunk in another place is probed from the same relative positions"),
		ShiftedSamples.Num() == RingSamples.Num() &&
		ShiftedSamples[3].Location.Equals(RingSamples[3].Location + Shift, 0.1f));

	TArray<FCoverFocusedSample> RejectedSamples;
	const FBox WallBounds(FVector::ZeroVector, FVector(600.0f, 40.0f, 300.0f));
	TestFalse(TEXT("A wall is left to the grid"),
		FCoverFinderAlgorithms::AppendThinObstacleSamples(WallBounds, Settings, RejectedSamples));
	const FBox StumpBounds(FVector::ZeroVector, FVector(60.0f, 60.0f, 40.0f));
	TestFalse(TEXT("A stump too low for crouch cover gets no ring"),
		FCoverFinderAlgorithms::AppendThinObstacleSamples(StumpBounds, Settings, RejectedSamples));
	TestEqual(TEXT("Rejected obstacles add no probes"), RejectedSamples.Num(), 0);

	const FBox PoleBounds(FVector::ZeroVector, FVector(20.0f, 20.0f, 300.0f));
	TestEqual(TEXT("A pole shelters one soldier"),
		FCoverFinderAlgorithms::GetThinObstacleSoldierCapacity(PoleBounds, Settings), 1);
	const FBox WideTrunkBounds(FVector::ZeroVector, FVector(150.0f, 140.0f, 400.0f));
	TestEqual(TEXT("A wide trunk shelters one soldier per configured width"),
		FCoverFinderAlgorithms::GetThinObstacleSoldierCapacity(WideTrunkBounds, Settings), 2);
	FCoverFinderSettingsSnapshot CrowdedSettings;
	CrowdedSettings.ThinObstacleWidthPerSoldier = 30.0f;
	TestEqual(TEXT("A smaller width per soldier lets more share the trunk"),
		FCoverFinderAlgorithms::GetThinObstacleSoldierCapacity(WideTrunkBounds, CrowdedSettings), 5);
	TestTrue(TEXT("Every point of the ring lies inside the obstacle's cover radius"),
		FVector::Dist2D(RingSamples[0].Location, TrunkBase) <
		FCoverFinderAlgorithms::GetThinObstacleCoverRadius(TrunkBounds, Settings));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCoverFinderReaimTest,
	"RTS.CoverFinder.Worker.ReaimsSlantedHits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCoverFinderReaimTest::RunTest(const FString& Parameters)
{
	const FCoverFinderSettingsSnapshot Settings;
	const FVector SampleLocation(100.0f, 200.0f, 0.0f);
	const FVector ProbeDirection = FVector::ForwardVector;
	// The probe runs along +X and grazes a surface whose normal is turned 60 degrees away from facing it.
	FCoverTraceObservation GrazingTrace;
	GrazingTrace.bBlockingHit = true;
	GrazingTrace.Distance = 120.0f;
	GrazingTrace.ImpactNormal = (-ProbeDirection).RotateAngleAxis(60.0f, FVector::UpVector);

	FCoverFocusedSample ReaimedSample;
	TestTrue(TEXT("A grazing hit asks for a second look"), FCoverFinderAlgorithms::TryBuildReaimedSample(
		SampleLocation, ProbeDirection, GrazingTrace, Settings, ReaimedSample));
	const FVector HitLocation = SampleLocation + ProbeDirection * GrazingTrace.Distance;
	TestTrue(TEXT("The second probe looks at the spot that was hit"), ReaimedSample.AimLocation.Equals(HitLocation, 0.1f));
	const FVector ReaimDirection = (ReaimedSample.AimLocation - ReaimedSample.Location).GetSafeNormal2D();
	TestTrue(TEXT("The second probe faces the surface squarely"),
		FVector::DotProduct(ReaimDirection, -GrazingTrace.ImpactNormal) > 0.999f);
	TestTrue(TEXT("The second probe stands a capsule clear of the surface"),
		FMath::IsNearlyEqual(
			FVector::Dist2D(ReaimedSample.Location, HitLocation),
			FCoverFinderAlgorithms::GetProbeStandOffDistance(Settings),
			0.1f));

	FCoverTraceObservation SquareTrace = GrazingTrace;
	SquareTrace.ImpactNormal = -ProbeDirection;
	TestFalse(TEXT("A square hit needs no second look"), FCoverFinderAlgorithms::TryBuildReaimedSample(
		SampleLocation, ProbeDirection, SquareTrace, Settings, ReaimedSample));
	FCoverTraceObservation MissedTrace;
	TestFalse(TEXT("A miss needs no second look"), FCoverFinderAlgorithms::TryBuildReaimedSample(
		SampleLocation, ProbeDirection, MissedTrace, Settings, ReaimedSample));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCoverFinderClassificationTest,
	"RTS.CoverFinder.Worker.Classification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCoverFinderClassificationTest::RunTest(const FString& Parameters)
{
	using namespace CoverFinderWorkerTestsPrivate;
	const TArray<FRTSCoverPoint> CrouchCandidates = Classify(MakeObservation(false, false, false));
	TestEqual(TEXT("Low obstruction creates one point"), CrouchCandidates.Num(), 1);
	if (CrouchCandidates.Num() == 1)
	{
		TestEqual(TEXT("Low obstruction is crouch cover"), CrouchCandidates[0].CoverType, ERTSCoverType::Crouch);
	}

	const TArray<FRTSCoverPoint> LeftCandidates = Classify(MakeObservation(true, true, false));
	TestEqual(TEXT("High cover with only a left gap creates one point"), LeftCandidates.Num(), 1);
	if (LeftCandidates.Num() == 1)
	{
		TestEqual(TEXT("Left opening selects the left montage type"), LeftCandidates[0].CoverType, ERTSCoverType::StandingLeft);
	}

	const TArray<FRTSCoverPoint> RightCandidates = Classify(MakeObservation(true, false, true));
	TestEqual(TEXT("High cover with only a right gap creates one point"), RightCandidates.Num(), 1);
	if (RightCandidates.Num() == 1)
	{
		TestEqual(TEXT("Right opening selects the right montage type"), RightCandidates[0].CoverType, ERTSCoverType::StandingRight);
	}

	const TArray<FRTSCoverPoint> ClosedCandidates = Classify(MakeObservation(true, false, false));
	TestTrue(TEXT("High cover without a standing-width side gap is rejected"), ClosedCandidates.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCoverFinderSurfaceAndSideTest,
	"RTS.CoverFinder.Worker.SurfaceAndSides",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCoverFinderSurfaceAndSideTest::RunTest(const FString& Parameters)
{
	using namespace CoverFinderWorkerTestsPrivate;
	const TArray<FRTSCoverPoint> BothSideCandidates = Classify(MakeObservation(true, true, true));
	TestEqual(TEXT("Independent openings preserve both montage choices"), BothSideCandidates.Num(), 2);

	const TArray<FRTSCoverPoint> UnrelatedStandingHitCandidates = Classify(
		MakeObservation(true, false, false, 170.0f));
	TestEqual(TEXT("A distant unrelated standing hit does not turn low cover into high cover"),
		UnrelatedStandingHitCandidates.Num(), 1);
	if (UnrelatedStandingHitCandidates.Num() == 1)
	{
		TestEqual(TEXT("The nearby surface remains crouch cover"),
			UnrelatedStandingHitCandidates[0].CoverType,
			ERTSCoverType::Crouch);
	}

	FCoverTraceObservation FirstProviderTrace = MakeTrace(true, 50.0f, 11);
	FCoverTraceObservation SecondProviderTrace = MakeTrace(true, 50.0f, 12);
	FCoverFinderSettingsSnapshot Settings;
	TestFalse(
		TEXT("Equal-height traces from different actors are not merged into one surface"),
		FCoverFinderAlgorithms::GetIsSameSurface(FirstProviderTrace, SecondProviderTrace, Settings));
	SecondProviderTrace.BlockingProviderHandle = FirstProviderTrace.BlockingProviderHandle;
	TestTrue(
		TEXT("Equal-height traces from one actor remain one surface"),
		FCoverFinderAlgorithms::GetIsSameSurface(FirstProviderTrace, SecondProviderTrace, Settings));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCoverFinderDeduplicationTest,
	"RTS.CoverFinder.Worker.Deduplication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCoverFinderDeduplicationTest::RunTest(const FString& Parameters)
{
	using namespace CoverFinderWorkerTestsPrivate;
	FCoverFinderSettingsSnapshot Settings;
	Settings.CoverPointSpacing = 90.0f;
	TArray<FRTSCoverPoint> RawCandidates;
	RawCandidates.Add(MakeCandidate(FVector::ZeroVector, FVector::ForwardVector, ERTSCoverType::Crouch));
	RawCandidates.Add(MakeCandidate(FVector(20.0f, 0.0f, 0.0f), FVector::ForwardVector, ERTSCoverType::Crouch));
	RawCandidates.Add(MakeCandidate(FVector::ZeroVector, FVector::ForwardVector, ERTSCoverType::StandingLeft));
	RawCandidates.Add(MakeCandidate(FVector::ZeroVector, FVector::ForwardVector, ERTSCoverType::StandingRight));
	FRTSCoverPoint AuthoredCandidate = MakeCandidate(
		FVector(10.0f, 0.0f, 0.0f),
		FVector::ForwardVector,
		ERTSCoverType::Crouch);
	AuthoredCandidate.ProviderRegistrationId = 42;
	RawCandidates.Add(AuthoredCandidate);

	const TArray<FRTSCoverPoint> FinalCandidates = FCoverFinderAlgorithms::FinalizeCandidates(
		MoveTemp(RawCandidates),
		Settings);
	TestEqual(TEXT("Nearby equivalent crouch points merge while left/right choices remain"), FinalCandidates.Num(), 3);
	TestTrue(TEXT("Stable IDs are populated"), FinalCandidates[0].PointId != 0);
	bool bFoundAuthoredCrouchPoint = false;
	for (const FRTSCoverPoint& FinalCandidate : FinalCandidates)
	{
		if (FinalCandidate.CoverType == ERTSCoverType::Crouch)
		{
			bFoundAuthoredCrouchPoint = FinalCandidate.ProviderRegistrationId == 42;
		}
	}
	TestTrue(TEXT("Authored cover wins a spacing conflict with an equivalent scanned point"), bFoundAuthoredCrouchPoint);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCoverFinderAuthoredRegistrationTest,
	"RTS.CoverFinder.AuthoredRegistration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCoverFinderAuthoredRegistrationTest::RunTest(const FString& Parameters)
{
	using namespace CoverFinderWorkerTestsPrivate;
	FRTSLocalCoverPoint LocalCoverPoint;
	LocalCoverPoint.LocalTransform = FTransform(FRotator::ZeroRotator, FVector(10.0f, 0.0f, 0.0f));
	LocalCoverPoint.CoverType = ERTSCoverType::StandingLeft;
	const FTransform OwnerTransform(FRotator(0.0f, 90.0f, 0.0f), FVector(100.0f, 200.0f, 300.0f));
	const FRTSCoverPoint WorldCoverPoint = FCoverFinderAlgorithms::BuildWorldAuthoredPoint(
		OwnerTransform,
		LocalCoverPoint);
	TestTrue(TEXT("Local cover offset is transformed by the owning actor"),
		WorldCoverPoint.Location.Equals(FVector(100.0f, 210.0f, 300.0f), KINDA_SMALL_NUMBER));
	TestTrue(TEXT("Local positive X rotation axis becomes the world cover normal"),
		WorldCoverPoint.CoverNormal.Equals(FVector::YAxisVector, KINDA_SMALL_NUMBER));
	TestEqual(TEXT("Local cover type is preserved"), WorldCoverPoint.CoverType, ERTSCoverType::StandingLeft);

	URTSCoverFinderWorldSubsystem* CoverSubsystem = NewObject<URTSCoverFinderWorldSubsystem>();
	TestNotNull(TEXT("Transient cover subsystem can host authored registration data"), CoverSubsystem);
	if (not IsValid(CoverSubsystem))
	{
		return false;
	}

	TArray<FRTSCoverPoint> FirstProviderPoints;
	FirstProviderPoints.Add(MakeCandidate(FVector::ZeroVector, FVector::ForwardVector, ERTSCoverType::Crouch));
	const uint64 FirstRegistrationId = CoverSubsystem->RegisterAuthoredCoverProvider(MoveTemp(FirstProviderPoints));
	TArray<FRTSCoverPoint> SecondProviderPoints;
	SecondProviderPoints.Add(MakeCandidate(FVector(20.0f, 0.0f, 0.0f), FVector::ForwardVector, ERTSCoverType::Crouch));
	const uint64 SecondRegistrationId = CoverSubsystem->RegisterAuthoredCoverProvider(MoveTemp(SecondProviderPoints));
	TestTrue(TEXT("Each non-empty provider receives a registration ID"),
		FirstRegistrationId != 0 && SecondRegistrationId != 0);
	TestEqual(TEXT("Authored providers obey global cover spacing"), CoverSubsystem->GetCoverPointsView().Num(), 1);

	CoverSubsystem->UnregisterAuthoredCoverProvider(FirstRegistrationId);
	TestEqual(TEXT("Removing one provider republishes a nearby surviving provider"),
		CoverSubsystem->GetCoverPointsView().Num(),
		1);
	CoverSubsystem->UnregisterAuthoredCoverProvider(SecondRegistrationId);
	TestTrue(TEXT("Removing the final provider removes its cover point immediately"),
		CoverSubsystem->GetCoverPointsView().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCoverFinderSamplePlanTest,
	"RTS.CoverFinder.Worker.SamplePlan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCoverFinderSamplePlanTest::RunTest(const FString& Parameters)
{
	FCoverFinderSettingsSnapshot Settings;
	Settings.SearchGridSpacing = 100.0f;
	TArray<FBox> TileBounds;
	TileBounds.Emplace(FVector(0.0f, 0.0f, -10.0f), FVector(100.0f, 100.0f, 10.0f));
	const FBox DuplicateTileBounds = TileBounds[0];
	TileBounds.Add(DuplicateTileBounds);
	bool bWasTruncated = false;
	const TArray<FVector> SamplePlan = FCoverFinderAlgorithms::BuildSamplePlan(
		TileBounds,
		Settings,
		bWasTruncated);
	TestEqual(TEXT("Overlapping tile bounds do not duplicate grid locations"), SamplePlan.Num(), 4);
	TestFalse(TEXT("Small plans do not report truncation"), bWasTruncated);
	TestEqual(TEXT("Plan order starts at minimum X/Y"), SamplePlan[0], FVector::ZeroVector);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCoverFinderReservationTest,
	"RTS.CoverFinder.Tactical.Reservations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCoverFinderReservationTest::RunTest(const FString& Parameters)
{
	using namespace CoverFinderWorkerTestsPrivate;
	URTSCoverFinderWorldSubsystem* CoverSubsystem = NewObject<URTSCoverFinderWorldSubsystem>();
	ASquadUnit* FirstUnit = NewObject<ASquadUnit>();
	ASquadUnit* SecondUnit = NewObject<ASquadUnit>();
	TestNotNull(TEXT("Cover subsystem exists"), CoverSubsystem);
	TestNotNull(TEXT("First transient squad unit exists"), FirstUnit);
	TestNotNull(TEXT("Second transient squad unit exists"), SecondUnit);
	if (not IsValid(CoverSubsystem) || not IsValid(FirstUnit) || not IsValid(SecondUnit))
	{
		return false;
	}

	TArray<FRTSCoverPoint> Points;
	Points.Add(MakeCandidate(FVector::ZeroVector, FVector::ForwardVector, ERTSCoverType::Crouch));
	Points.Add(MakeCandidate(FVector(300.0f, 0.0f, 0.0f), FVector::ForwardVector, ERTSCoverType::Crouch));
	CoverSubsystem->RegisterAuthoredCoverProvider(MoveTemp(Points));
	FRTSCoverPoint FirstReservation;
	FRTSCoverPoint SecondReservation;
	TestTrue(
		TEXT("First unit reserves the nearest point"),
		CoverSubsystem->TryReserveBestCoverPoint(
			*FirstUnit,
			nullptr,
			FVector::ZeroVector,
			FirstReservation));
	TestTrue(
		TEXT("Second unit can reserve another point"),
		CoverSubsystem->TryReserveBestCoverPoint(
			*SecondUnit,
			nullptr,
			FVector::ZeroVector,
			SecondReservation));
	TestNotEqual(
		TEXT("Two units never receive the same physical cover slot"),
		FirstReservation.PointId,
		SecondReservation.PointId);
	CoverSubsystem->ReleaseCoverReservation(*FirstUnit, FirstReservation.PointId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCoverFinderStandingPeekOffsetTest,
	"RTS.CoverFinder.Tactical.StandingPeekOffset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCoverFinderStandingPeekOffsetTest::RunTest(const FString& Parameters)
{
	using namespace CoverFinderWorkerTestsPrivate;
	const URTSCoverFinderWorldSubsystem* CoverSubsystem = NewObject<URTSCoverFinderWorldSubsystem>();
	TestNotNull(TEXT("Cover subsystem exists"), CoverSubsystem);
	if (not IsValid(CoverSubsystem))
	{
		return false;
	}

	// The normal points from the wall to the soldier, so this soldier faces -X and its left is +Y.
	const FVector CoverNormal = FVector::ForwardVector;
	const FVector CrouchOffset = CoverSubsystem->GetStandingPeekOffset(
		MakeCandidate(FVector::ZeroVector, CoverNormal, ERTSCoverType::Crouch));
	const FVector LeftOffset = CoverSubsystem->GetStandingPeekOffset(
		MakeCandidate(FVector::ZeroVector, CoverNormal, ERTSCoverType::StandingLeft));
	const FVector RightOffset = CoverSubsystem->GetStandingPeekOffset(
		MakeCandidate(FVector::ZeroVector, CoverNormal, ERTSCoverType::StandingRight));
	TestTrue(TEXT("Crouch cover fires over the top and never steps sideways"), CrouchOffset.IsNearlyZero());
	TestTrue(TEXT("A left peek steps to the soldier's left"), LeftOffset.Y > 0.0f);
	TestTrue(TEXT("A right peek steps to the soldier's right"), RightOffset.Y < 0.0f);
	TestTrue(TEXT("Peeking moves along the wall, not through or away from it"),
		FMath::IsNearlyZero(LeftOffset.X) && FMath::IsNearlyZero(RightOffset.X));
	TestTrue(TEXT("Both sides step the same distance"), FMath::IsNearlyEqual(LeftOffset.Size(), RightOffset.Size()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCoverFinderAnimationOffsetTest,
	"RTS.CoverFinder.Animation.RootMotionOffsets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCoverFinderAnimationOffsetTest::RunTest(const FString& Parameters)
{
	using namespace CoverFinderWorkerTestsPrivate;
	const FSquadUnitCoverAnimationSets DefaultAnimationSets;
	TestTrue(
		TEXT("Standing enter clips start in front of the cover, never inside it"),
		DefaultAnimationSets.StandingLeft.EnterStartOffset.TowardCover < 0.0f &&
		DefaultAnimationSets.StandingRight.EnterStartOffset.TowardCover < 0.0f);
	TestTrue(
		TEXT("Crouch cover starts on the point until a designer tunes it"),
		FMath::IsNearlyZero(DefaultAnimationSets.Crouch.EnterStartOffset.TowardCover) &&
		FMath::IsNearlyZero(DefaultAnimationSets.Crouch.EnterStartOffset.Right));
	TestTrue(
		TEXT("The left set exposes to the soldier's left and the right set to the right"),
		DefaultAnimationSets.StandingLeft.ExposedOffset.Right < 0.0f &&
		DefaultAnimationSets.StandingRight.ExposedOffset.Right > 0.0f);

	TestTrue(
		TEXT("A soldier in a trench stands up in place: no travel into the pose and none out of it"),
		DefaultAnimationSets.Trench.EnterStartOffset.TowardCover == 0.0f &&
		DefaultAnimationSets.Trench.EnterStartOffset.Right == 0.0f &&
		DefaultAnimationSets.Trench.ExposedOffset.TowardCover == 0.0f &&
		DefaultAnimationSets.Trench.ExposedOffset.Right == 0.0f);

	// A unit without an animation instance has no authored exposure and must fall back to the caller's step.
	const ASquadUnit* SquadUnit = NewObject<ASquadUnit>();
	TestNotNull(TEXT("Transient squad unit exists"), SquadUnit);
	if (not IsValid(SquadUnit))
	{
		return false;
	}
	const FVector DefaultStep(0.0f, 84.0f, 0.0f);
	TestTrue(
		TEXT("Standing cover without an expose montage uses the default step"),
		SquadUnit->GetStandingCoverExposedWorldOffset(
			MakeCandidate(FVector::ZeroVector, FVector::ForwardVector, ERTSCoverType::StandingLeft),
			DefaultStep).Equals(DefaultStep));
	TestTrue(
		TEXT("Crouch cover never steps sideways, whatever default is offered"),
		SquadUnit->GetStandingCoverExposedWorldOffset(
			MakeCandidate(FVector::ZeroVector, FVector::ForwardVector, ERTSCoverType::Crouch),
			DefaultStep).IsNearlyZero());
	float SettledError = 0.0f;
	TestFalse(
		TEXT("A unit outside cover reports no settled capsule error"),
		SquadUnit->TryGetSettledCoverCapsuleError(SettledError));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCoverFinderWeaponIgnoreReasonTest,
	"RTS.CoverFinder.Tactical.WeaponIgnoreReasons",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCoverFinderWeaponIgnoreReasonTest::RunTest(const FString& Parameters)
{
	UWeaponState* WeaponState = NewObject<UWeaponState>();
	AActor* ProviderActor = NewObject<AActor>();
	TestNotNull(TEXT("Transient weapon state exists"), WeaponState);
	TestNotNull(TEXT("Transient provider actor exists"), ProviderActor);
	if (not IsValid(WeaponState) || not IsValid(ProviderActor))
	{
		return false;
	}

	WeaponState->RegisterActorToIgnoreForReason(
		ProviderActor,
		EWeaponIgnoredActorReason::External,
		true);
	WeaponState->RegisterActorToIgnoreForReason(
		ProviderActor,
		EWeaponIgnoredActorReason::Cover,
		true);
	WeaponState->RegisterActorToIgnoreForReason(
		ProviderActor,
		EWeaponIgnoredActorReason::Cover,
		false);
	TestTrue(
		TEXT("Removing cover ignore preserves the cargo or external reason"),
		WeaponState->GetIsActorIgnored(ProviderActor));
	WeaponState->RegisterActorToIgnoreForReason(
		ProviderActor,
		EWeaponIgnoredActorReason::External,
		false);
	TestFalse(
		TEXT("Actor is removed only after every ignore reason clears"),
		WeaponState->GetIsActorIgnored(ProviderActor));
	return true;
}

#endif
