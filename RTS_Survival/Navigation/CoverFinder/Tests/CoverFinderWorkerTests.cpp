#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorldSubsystem.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorker.h"
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
