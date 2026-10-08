#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverCombatScoring.h"

namespace CoverCombatScoringTestsPrivate
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	/** Cover whose soldier stands at Location and looks along TowardCover. */
	FRTSCoverPoint MakeCover(const FVector& Location, const FVector& TowardCover)
	{
		FRTSCoverPoint CoverPoint;
		CoverPoint.PointId = 1;
		CoverPoint.Location = Location;
		CoverPoint.CoverNormal = -TowardCover;
		return CoverPoint;
	}

	FRTSCombatCoverThreats MakeThreats(const FVector& PrimaryTargetLocation)
	{
		FRTSCombatCoverThreats Threats;
		Threats.PrimaryTargetLocation = PrimaryTargetLocation;
		Threats.ThreatLocations.Add(PrimaryTargetLocation);
		return Threats;
	}

	/** A location Distance away from the origin, YawDegrees to the right of +X. */
	FVector LocationAtYaw(const float YawDegrees, const float Distance = 2000.0f)
	{
		return FVector::ForwardVector.RotateAngleAxis(YawDegrees, FVector::UpVector) * Distance;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCoverCombatAimArcTest,
	"RTS.CoverFinder.Combat.AimArcBoundsUsableCover",
	CoverCombatScoringTestsPrivate::TestFlags)

bool FCoverCombatAimArcTest::RunTest(const FString& Parameters)
{
	using namespace CoverCombatScoringTestsPrivate;
	// The soldier stands at the origin and looks along +X at its cover; +Y is its right-hand side.
	const FRTSCoverPoint CoverPoint = MakeCover(FVector::ZeroVector, FVector::ForwardVector);
	const FRTSCombatCoverSettings Settings;
	TestTrue(TEXT("A target to the right has a positive yaw"),
		FMath::IsNearlyEqual(FRTSCombatCoverScoring::GetYawToLocationDegrees(CoverPoint, LocationAtYaw(40.0f)), 40.0f, 0.1f));
	TestTrue(TEXT("A target to the left has a negative yaw"),
		FMath::IsNearlyEqual(FRTSCombatCoverScoring::GetYawToLocationDegrees(CoverPoint, LocationAtYaw(-70.0f)), -70.0f, 0.1f));
	TestTrue(TEXT("A target straight behind the cover can be aimed at"),
		FRTSCombatCoverScoring::GetCanAimAt(CoverPoint, LocationAtYaw(0.0f), Settings));
	TestTrue(TEXT("The aim offsets reach 89 degrees to either side"),
		FRTSCombatCoverScoring::GetCanAimAt(CoverPoint, LocationAtYaw(89.0f), Settings) &&
		FRTSCombatCoverScoring::GetCanAimAt(CoverPoint, LocationAtYaw(-89.0f), Settings));
	TestFalse(TEXT("A target behind the soldier is outside the aim offsets"),
		FRTSCombatCoverScoring::GetCanAimAt(CoverPoint, LocationAtYaw(120.0f), Settings));
	TestFalse(TEXT("A target on top of the point has no direction"),
		FRTSCombatCoverScoring::GetCanAimAt(CoverPoint, CoverPoint.Location, Settings));

	FRTSCombatCoverSettings NarrowSettings;
	NarrowSettings.MaximumAimYawDegrees = 45.0f;
	TestFalse(TEXT("A narrower designer arc rejects a target at 60 degrees"),
		FRTSCombatCoverScoring::GetCanAimAt(CoverPoint, LocationAtYaw(60.0f), NarrowSettings));
	float UnusedScore = 0.0f;
	TestFalse(TEXT("A point that cannot aim at the target gets no score"),
		FRTSCombatCoverScoring::TryScoreCoverPoint(
			CoverPoint, FVector::ZeroVector, MakeThreats(LocationAtYaw(150.0f)), Settings, UnusedScore));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCoverCombatFacingTest,
	"RTS.CoverFinder.Combat.PrefersCoverBetweenSoldierAndEnemy",
	CoverCombatScoringTestsPrivate::TestFlags)

bool FCoverCombatFacingTest::RunTest(const FString& Parameters)
{
	using namespace CoverCombatScoringTestsPrivate;
	const FRTSCombatCoverSettings Settings;
	const FVector UnitLocation = FVector::ZeroVector;
	const FRTSCombatCoverThreats Threats = MakeThreats(FVector(3000.0f, 0.0f, 0.0f));
	// Right next to the soldier, but the enemy is 80 degrees off to the side of this wall.
	const FRTSCoverPoint NearSidewaysCover = MakeCover(
		FVector(0.0f, 100.0f, 0.0f),
		FVector::ForwardVector.RotateAngleAxis(80.0f, FVector::UpVector));
	// A few steps away, with the wall squarely between the soldier and the enemy.
	const FRTSCoverPoint FacingCover = MakeCover(FVector(0.0f, -400.0f, 0.0f), FVector::ForwardVector);

	float NearSidewaysScore = 0.0f;
	float FacingScore = 0.0f;
	TestTrue(TEXT("The sideways cover is still usable"), FRTSCombatCoverScoring::TryScoreCoverPoint(
		NearSidewaysCover, UnitLocation, Threats, Settings, NearSidewaysScore));
	TestTrue(TEXT("The facing cover is usable"), FRTSCombatCoverScoring::TryScoreCoverPoint(
		FacingCover, UnitLocation, Threats, Settings, FacingScore));
	TestTrue(TEXT("Cover that faces the enemy beats nearer cover that does not"), FacingScore > NearSidewaysScore);

	const FRTSCoverPoint FarFacingCover = MakeCover(FVector(0.0f, -850.0f, 0.0f), FVector::ForwardVector);
	float FarFacingScore = 0.0f;
	FRTSCombatCoverScoring::TryScoreCoverPoint(FarFacingCover, UnitLocation, Threats, Settings, FarFacingScore);
	TestTrue(TEXT("Of two equally good walls the nearer one wins"), FacingScore > FarFacingScore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCoverCombatThreatsTest,
	"RTS.CoverFinder.Combat.CountsEveryEngagedEnemy",
	CoverCombatScoringTestsPrivate::TestFlags)

bool FCoverCombatThreatsTest::RunTest(const FString& Parameters)
{
	using namespace CoverCombatScoringTestsPrivate;
	const FRTSCombatCoverSettings Settings;
	const FRTSCoverPoint CoverPoint = MakeCover(FVector::ZeroVector, FVector::ForwardVector);
	FRTSCombatCoverThreats Threats = MakeThreats(LocationAtYaw(10.0f));
	TestEqual(TEXT("One enemy behind the cover is fully blocked"),
		FRTSCombatCoverScoring::GetProtectedThreatFraction(CoverPoint, Threats, Settings), 1.0f);

	Threats.ThreatLocations.Add(LocationAtYaw(85.0f));
	TestEqual(TEXT("A second enemy on the flank halves the protection"),
		FRTSCombatCoverScoring::GetProtectedThreatFraction(CoverPoint, Threats, Settings), 0.5f);

	// A wall turned halfway toward the second enemy has both within its protected angle.
	const FRTSCoverPoint AngledCover = MakeCover(
		FVector::ZeroVector,
		FVector::ForwardVector.RotateAngleAxis(45.0f, FVector::UpVector));
	TestEqual(TEXT("Cover angled between two enemies blocks both"),
		FRTSCombatCoverScoring::GetProtectedThreatFraction(AngledCover, Threats, Settings), 1.0f);
	float StraightScore = 0.0f;
	float AngledScore = 0.0f;
	FRTSCombatCoverScoring::TryScoreCoverPoint(CoverPoint, FVector::ZeroVector, Threats, Settings, StraightScore);
	FRTSCombatCoverScoring::TryScoreCoverPoint(AngledCover, FVector::ZeroVector, Threats, Settings, AngledScore);
	TestTrue(TEXT("Blocking both enemies outscores squarely facing only the target"), AngledScore > StraightScore);

	FRTSCombatCoverSettings StrictSettings;
	StrictSettings.ProtectedHalfAngleDegrees = 30.0f;
	TestEqual(TEXT("A stricter designer angle no longer counts the angled wall as blocking either enemy"),
		FRTSCombatCoverScoring::GetProtectedThreatFraction(AngledCover, Threats, StrictSettings), 0.0f);
	const FRTSCombatCoverThreats NoThreats;
	TestEqual(TEXT("Without threats nothing is protected against"),
		FRTSCombatCoverScoring::GetProtectedThreatFraction(CoverPoint, NoThreats, Settings), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCoverCombatRepositionTest,
	"RTS.CoverFinder.Combat.RepositionsOnlyWhenFlanked",
	CoverCombatScoringTestsPrivate::TestFlags)

bool FCoverCombatRepositionTest::RunTest(const FString& Parameters)
{
	using namespace CoverCombatScoringTestsPrivate;
	const FRTSCombatCoverSettings Settings;
	// The soldier sits in OccupiedCover; OtherCover is the next wall along, turned 70 degrees to the right.
	const FRTSCoverPoint OccupiedCover = MakeCover(FVector::ZeroVector, FVector::ForwardVector);
	const FRTSCoverPoint OtherCover = MakeCover(
		FVector(0.0f, 350.0f, 0.0f),
		FVector::ForwardVector.RotateAngleAxis(70.0f, FVector::UpVector));
	const auto GetIsMoveWorthIt = [&](const FVector& EnemyLocation)
	{
		const FRTSCombatCoverThreats Threats = MakeThreats(EnemyLocation);
		float OccupiedScore = 0.0f;
		float OtherScore = 0.0f;
		const bool bOccupiedUsable = FRTSCombatCoverScoring::TryScoreCoverPoint(
			OccupiedCover, OccupiedCover.Location, Threats, Settings, OccupiedScore);
		const bool bOtherUsable = FRTSCombatCoverScoring::TryScoreCoverPoint(
			OtherCover, OccupiedCover.Location, Threats, Settings, OtherScore);
		return bOccupiedUsable && bOtherUsable &&
			FRTSCombatCoverScoring::GetIsWorthRepositioning(OccupiedScore, OtherScore, Settings);
	};
	TestFalse(TEXT("With the enemy in front, the soldier stays"), GetIsMoveWorthIt(LocationAtYaw(0.0f, 3000.0f)));
	TestFalse(TEXT("An enemy drifting a little to the side is no reason to move"),
		GetIsMoveWorthIt(LocationAtYaw(35.0f, 3000.0f)));
	TestTrue(TEXT("An enemy that has moved around to the flank makes the other wall worth the walk"),
		GetIsMoveWorthIt(LocationAtYaw(75.0f, 3000.0f)));

	TestFalse(TEXT("An equal score is never worth a walk"),
		FRTSCombatCoverScoring::GetIsWorthRepositioning(1.0f, 1.0f, Settings));
	FRTSCombatCoverSettings RestlessSettings;
	RestlessSettings.MinimumScoreGain = 0.0f;
	TestTrue(TEXT("The designer can remove the threshold"),
		FRTSCombatCoverScoring::GetIsWorthRepositioning(1.0f, 1.0f, RestlessSettings));

	// A long walk eats the gain: the same wall much further away is not worth it for the same flanking enemy.
	FRTSCoverPoint DistantOtherCover = OtherCover;
	DistantOtherCover.Location = FVector(0.0f, 900.0f, 0.0f);
	const FRTSCombatCoverThreats FlankThreats = MakeThreats(LocationAtYaw(75.0f, 3000.0f));
	float NearOtherScore = 0.0f;
	float DistantOtherScore = 0.0f;
	FRTSCombatCoverScoring::TryScoreCoverPoint(OtherCover, FVector::ZeroVector, FlankThreats, Settings, NearOtherScore);
	FRTSCombatCoverScoring::TryScoreCoverPoint(
		DistantOtherCover, FVector::ZeroVector, FlankThreats, Settings, DistantOtherScore);
	TestTrue(TEXT("Walking further under fire costs score"), DistantOtherScore < NearOtherScore);
	return true;
}

#endif
