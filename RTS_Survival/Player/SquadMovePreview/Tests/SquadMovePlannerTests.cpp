#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RTS_Survival/Player/SquadMovePreview/SquadMovePlanner.h"

namespace SquadMovePlannerTestsPrivate
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	/** Soldiers of one squad standing in a line behind the origin, facing +X. */
	void AddSquad(FSquadMovePlanRequest& Request, const int32 UnitCount, const FVector& SquadCentre)
	{
		const int32 SquadIndex = Request.SquadCount;
		++Request.SquadCount;
		for (int32 UnitIndex = 0; UnitIndex < UnitCount; ++UnitIndex)
		{
			FSquadMovePlannerUnit& Unit = Request.Units.AddDefaulted_GetRef();
			Unit.SquadIndex = SquadIndex;
			Unit.Location = SquadCentre + FVector(0.0f, static_cast<float>(UnitIndex) * 100.0f, 0.0f);
		}
	}

	/** Cover whose soldier stands at Location and looks along TowardCover. */
	FRTSCoverPoint MakeCover(
		const int64 PointId,
		const FVector& Location,
		const FVector& TowardCover,
		const ERTSCoverType CoverType)
	{
		FRTSCoverPoint CoverPoint;
		CoverPoint.PointId = PointId;
		CoverPoint.Location = Location;
		CoverPoint.CoverNormal = -TowardCover;
		CoverPoint.CoverType = CoverType;
		return CoverPoint;
	}

	FSquadMovePlanRequest MakeRequest(const FVector& Anchor)
	{
		FSquadMovePlanRequest Request;
		Request.Anchor = Anchor;
		Request.Facing = FVector::ForwardVector;
		return Request;
	}

	int32 CountType(const FSquadMovePlan& Plan, const ESquadPlannedPositionType Type)
	{
		int32 Count = 0;
		for (const FSquadUnitPlannedPosition& Position : Plan.UnitPositions)
		{
			Count += Position.Type == Type ? 1 : 0;
		}
		return Count;
	}

	bool UsesCover(const FSquadMovePlan& Plan, const int64 PointId)
	{
		for (const FSquadUnitPlannedPosition& Position : Plan.UnitPositions)
		{
			if (Position.GetIsCover() && Position.CoverPoint.PointId == PointId)
			{
				return true;
			}
		}
		return false;
	}

	float SmallestSpacing(const FSquadMovePlan& Plan)
	{
		float Smallest = TNumericLimits<float>::Max();
		for (int32 First = 0; First < Plan.UnitPositions.Num(); ++First)
		{
			for (int32 Second = First + 1; Second < Plan.UnitPositions.Num(); ++Second)
			{
				Smallest = FMath::Min(Smallest, FVector::Dist2D(
					Plan.UnitPositions[First].Location,
					Plan.UnitPositions[Second].Location));
			}
		}
		return Smallest;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSquadMovePlannerCoverTypesTest,
	"RTS.SquadMovePlanner.UsesNearbyCoverAndKeepsTypesApart",
	SquadMovePlannerTestsPrivate::TestFlags)

bool FSquadMovePlannerCoverTypesTest::RunTest(const FString& Parameters)
{
	using namespace SquadMovePlannerTestsPrivate;
	FSquadMovePlanRequest Request = MakeRequest(FVector(2000.0f, 0.0f, 0.0f));
	AddSquad(Request, 4, FVector::ZeroVector);
	Request.CoverPoints.Add(MakeCover(1, FVector(2050.0f, 100.0f, 0.0f), FVector::ForwardVector, ERTSCoverType::Crouch));
	Request.CoverPoints.Add(MakeCover(2, FVector(2050.0f, -150.0f, 0.0f), FVector::ForwardVector, ERTSCoverType::StandingLeft));
	Request.CoverPoints.Add(MakeCover(3, FVector(6000.0f, 0.0f, 0.0f), FVector::ForwardVector, ERTSCoverType::Crouch));

	FSquadMovePlan Plan;
	FSquadMovePlanner::BuildPlan(Request, Plan);
	TestEqual(TEXT("Every soldier gets a position"), Plan.UnitPositions.Num(), 4);
	TestEqual(TEXT("The crouch point near the cursor is a crouch-cover position"),
		CountType(Plan, ESquadPlannedPositionType::CrouchCover), 1);
	TestEqual(TEXT("The standing point near the cursor is a standing-cover position"),
		CountType(Plan, ESquadPlannedPositionType::StandingCover), 1);
	TestEqual(TEXT("Soldiers without cover get regular positions"),
		CountType(Plan, ESquadPlannedPositionType::RegularStanding), 2);
	TestFalse(TEXT("Cover far from the cursor is not used"), UsesCover(Plan, 3));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSquadMovePlannerSpacingTest,
	"RTS.SquadMovePlanner.KeepsPositionsUniqueAndSpaced",
	SquadMovePlannerTestsPrivate::TestFlags)

bool FSquadMovePlannerSpacingTest::RunTest(const FString& Parameters)
{
	using namespace SquadMovePlannerTestsPrivate;
	FSquadMovePlanRequest Request = MakeRequest(FVector(2000.0f, 0.0f, 0.0f));
	AddSquad(Request, 7, FVector::ZeroVector);
	// A left and a right variant of the same wall end sit almost on top of each other.
	Request.CoverPoints.Add(MakeCover(1, FVector(2000.0f, 0.0f, 0.0f), FVector::ForwardVector, ERTSCoverType::StandingLeft));
	Request.CoverPoints.Add(MakeCover(2, FVector(2000.0f, 20.0f, 0.0f), FVector::ForwardVector, ERTSCoverType::StandingRight));
	Request.CoverPoints.Add(MakeCover(3, FVector(2000.0f, 200.0f, 0.0f), FVector::ForwardVector, ERTSCoverType::Crouch));

	FSquadMovePlan Plan;
	FSquadMovePlanner::BuildPlan(Request, Plan);
	TestEqual(TEXT("Only one of two overlapping cover points is used"),
		CountType(Plan, ESquadPlannedPositionType::StandingCover), 1);
	TestTrue(TEXT("No two soldiers are planned closer than the minimum spacing"),
		SmallestSpacing(Plan) >= Request.Settings.MinimumSlotSpacing - KINDA_SMALL_NUMBER);
	TSet<int64> UsedPointIds;
	for (const FSquadUnitPlannedPosition& Position : Plan.UnitPositions)
	{
		TestTrue(TEXT("Every soldier has a real position type"), Position.Type != ESquadPlannedPositionType::None);
		if (Position.GetIsCover())
		{
			bool bAlreadyUsed = false;
			UsedPointIds.Add(Position.CoverPoint.PointId, &bAlreadyUsed);
			TestFalse(TEXT("A cover point is given to one soldier only"), bAlreadyUsed);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSquadMovePlannerChosenFacingTest,
	"RTS.SquadMovePlanner.ArrowFacingFiltersCover",
	SquadMovePlannerTestsPrivate::TestFlags)

bool FSquadMovePlannerChosenFacingTest::RunTest(const FString& Parameters)
{
	using namespace SquadMovePlannerTestsPrivate;
	FSquadMovePlanRequest Request = MakeRequest(FVector(2000.0f, 0.0f, 0.0f));
	AddSquad(Request, 2, FVector::ZeroVector);
	// Both sides of one wall: one soldier would look along +X, the other along -X.
	Request.CoverPoints.Add(MakeCover(1, FVector(1950.0f, 0.0f, 0.0f), FVector::ForwardVector, ERTSCoverType::Crouch));
	Request.CoverPoints.Add(MakeCover(2, FVector(2100.0f, 0.0f, 0.0f), -FVector::ForwardVector, ERTSCoverType::Crouch));

	Request.bFacingChosenByPlayer = true;
	Request.Facing = FVector::ForwardVector;
	FSquadMovePlan ForwardPlan;
	FSquadMovePlanner::BuildPlan(Request, ForwardPlan);
	TestTrue(TEXT("Cover that protects against the arrow's direction is used"), UsesCover(ForwardPlan, 1));
	TestFalse(TEXT("Cover on the far side of the wall is dropped"), UsesCover(ForwardPlan, 2));

	Request.Facing = -FVector::ForwardVector;
	FSquadMovePlan BackwardPlan;
	FSquadMovePlanner::BuildPlan(Request, BackwardPlan);
	TestTrue(TEXT("Turning the arrow around switches to the other side of the wall"), UsesCover(BackwardPlan, 2));
	TestFalse(TEXT("The first side is dropped once it no longer protects"), UsesCover(BackwardPlan, 1));

	Request.bFacingChosenByPlayer = false;
	Request.Facing = FVector::ForwardVector;
	FSquadMovePlan FreePlan;
	FSquadMovePlanner::BuildPlan(Request, FreePlan);
	TestTrue(TEXT("Without an arrow both sides stay available, so the cursor decides"),
		UsesCover(FreePlan, 1) && UsesCover(FreePlan, 2));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSquadMovePlannerFollowsCursorTest,
	"RTS.SquadMovePlanner.PlanFollowsTheCursor",
	SquadMovePlannerTestsPrivate::TestFlags)

bool FSquadMovePlannerFollowsCursorTest::RunTest(const FString& Parameters)
{
	using namespace SquadMovePlannerTestsPrivate;
	FSquadMovePlanRequest Request = MakeRequest(FVector(2000.0f, 0.0f, 0.0f));
	AddSquad(Request, 3, FVector::ZeroVector);
	Request.CoverPoints.Add(MakeCover(1, FVector(2000.0f, 0.0f, 0.0f), FVector::ForwardVector, ERTSCoverType::Crouch));
	Request.CoverPoints.Add(MakeCover(2, FVector(4000.0f, 0.0f, 0.0f), FVector::ForwardVector, ERTSCoverType::Crouch));

	FSquadMovePlan NearFirstPlan;
	FSquadMovePlanner::BuildPlan(Request, NearFirstPlan);
	TestTrue(TEXT("The cover under the cursor is used"), UsesCover(NearFirstPlan, 1) && not UsesCover(NearFirstPlan, 2));

	Request.Anchor = FVector(4000.0f, 0.0f, 0.0f);
	FSquadMovePlan NearSecondPlan;
	FSquadMovePlanner::BuildPlan(Request, NearSecondPlan);
	TestTrue(TEXT("Moving the cursor moves the plan to the other cover"),
		UsesCover(NearSecondPlan, 2) && not UsesCover(NearSecondPlan, 1));

	Request.Anchor = FVector(3000.0f, 3000.0f, 0.0f);
	FSquadMovePlan OpenPlan;
	FSquadMovePlanner::BuildPlan(Request, OpenPlan);
	TestEqual(TEXT("Away from all cover every soldier stands in the open"),
		CountType(OpenPlan, ESquadPlannedPositionType::RegularStanding), 3);
	for (const FSquadUnitPlannedPosition& Position : OpenPlan.UnitPositions)
	{
		TestTrue(TEXT("Open positions form up around the cursor"),
			FVector::Dist2D(Position.Location, Request.Anchor) <= Request.Settings.RegularUnitSpacing * 1.5f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSquadMovePlannerFormationTest,
	"RTS.SquadMovePlanner.OpenFormationFollowsFacing",
	SquadMovePlannerTestsPrivate::TestFlags)

bool FSquadMovePlannerFormationTest::RunTest(const FString& Parameters)
{
	using namespace SquadMovePlannerTestsPrivate;
	FSquadMovePlanRequest Request = MakeRequest(FVector(2000.0f, 500.0f, 0.0f));
	AddSquad(Request, 7, FVector::ZeroVector);
	Request.Facing = FVector::RightVector;
	Request.bFacingChosenByPlayer = true;

	FSquadMovePlan Plan;
	FSquadMovePlanner::BuildPlan(Request, Plan);
	// Seven men form a block of 4 + 3 around the cursor instead of one long line.
	const float HalfRowSpacing = Request.Settings.RegularRowSpacing * 0.5f;
	int32 FrontRowCount = 0;
	int32 BackRowCount = 0;
	FVector LocationSum = FVector::ZeroVector;
	for (const FSquadUnitPlannedPosition& Position : Plan.UnitPositions)
	{
		TestTrue(TEXT("Soldiers in the open face the chosen direction"),
			Position.Facing.Equals(FVector::RightVector, KINDA_SMALL_NUMBER));
		const float DepthBehindAnchor = FVector::DotProduct(Request.Anchor - Position.Location, Request.Facing);
		FrontRowCount += FMath::IsNearlyEqual(DepthBehindAnchor, -HalfRowSpacing, 1.0f) ? 1 : 0;
		BackRowCount += FMath::IsNearlyEqual(DepthBehindAnchor, HalfRowSpacing, 1.0f) ? 1 : 0;
		LocationSum += Position.Location;
	}
	TestEqual(TEXT("The front row is the wider one"), FrontRowCount, 4);
	TestEqual(TEXT("The rest stands in a second row"), BackRowCount, 3);
	TestTrue(TEXT("The block is centred on the cursor"),
		FVector::Dist2D(LocationSum / 7.0f, Request.Anchor) < Request.Settings.RegularRowSpacing * 0.5f);
	TestTrue(TEXT("Rows keep the open spacing"),
		SmallestSpacing(Plan) >= Request.Settings.RegularUnitSpacing - 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSquadMovePlannerSquadsTest,
	"RTS.SquadMovePlanner.KeepsSquadsSideBySideWithoutCrossing",
	SquadMovePlannerTestsPrivate::TestFlags)

bool FSquadMovePlannerSquadsTest::RunTest(const FString& Parameters)
{
	using namespace SquadMovePlannerTestsPrivate;
	FSquadMovePlanRequest Request = MakeRequest(FVector(3000.0f, 0.0f, 0.0f));
	// Facing +X makes +Y the right-hand side: squad 0 stands on the right, squad 1 on the left.
	AddSquad(Request, 3, FVector(0.0f, 800.0f, 0.0f));
	AddSquad(Request, 3, FVector(0.0f, -800.0f, 0.0f));

	FSquadMovePlan Plan;
	FSquadMovePlanner::BuildPlan(Request, Plan);
	TestEqual(TEXT("Each squad has its own anchor"), Plan.SquadAnchors.Num(), 2);
	TestTrue(TEXT("The squad that stands on the right is sent to the right"),
		Plan.SquadAnchors[0].Y > Plan.SquadAnchors[1].Y);
	// Three men stand in one row, two unit spacings wide; the gap is the free ground between the two blocks.
	TestTrue(TEXT("Neighbouring squads keep the squad gap between their blocks"),
		FMath::IsNearlyEqual(
			FVector::Dist2D(Plan.SquadAnchors[0], Plan.SquadAnchors[1]),
			Request.Settings.RegularUnitSpacing * 2.0f + Request.Settings.SquadGap,
			1.0f));
	for (int32 UnitIndex = 0; UnitIndex < Request.Units.Num(); ++UnitIndex)
	{
		const int32 SquadIndex = Request.Units[UnitIndex].SquadIndex;
		const int32 OtherSquadIndex = 1 - SquadIndex;
		TestTrue(TEXT("A soldier is planned closer to its own squad's anchor"),
			FVector::Dist2D(Plan.UnitPositions[UnitIndex].Location, Plan.SquadAnchors[SquadIndex]) <
			FVector::Dist2D(Plan.UnitPositions[UnitIndex].Location, Plan.SquadAnchors[OtherSquadIndex]));
	}
	// Within a squad the leftmost soldier gets the leftmost position.
	TestTrue(TEXT("Soldiers keep their left-to-right order"),
		Plan.UnitPositions[0].Location.Y < Plan.UnitPositions[1].Location.Y &&
		Plan.UnitPositions[1].Location.Y < Plan.UnitPositions[2].Location.Y);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSquadMovePlannerStabilityTest,
	"RTS.SquadMovePlanner.PreviousPlanKeepsItsCover",
	SquadMovePlannerTestsPrivate::TestFlags)

bool FSquadMovePlannerStabilityTest::RunTest(const FString& Parameters)
{
	using namespace SquadMovePlannerTestsPrivate;
	FSquadMovePlanRequest Request = MakeRequest(FVector(2000.0f, 0.0f, 0.0f));
	AddSquad(Request, 1, FVector::ZeroVector);
	Request.CoverPoints.Add(MakeCover(1, FVector(2000.0f, 100.0f, 0.0f), FVector::ForwardVector, ERTSCoverType::Crouch));
	Request.CoverPoints.Add(MakeCover(2, FVector(2000.0f, -130.0f, 0.0f), FVector::ForwardVector, ERTSCoverType::Crouch));

	FSquadMovePlan FreshPlan;
	FSquadMovePlanner::BuildPlan(Request, FreshPlan);
	TestTrue(TEXT("A fresh plan takes the nearest cover"), UsesCover(FreshPlan, 1));

	Request.PreviousCoverPointIds.Add(2);
	FSquadMovePlan StickyPlan;
	FSquadMovePlanner::BuildPlan(Request, StickyPlan);
	TestTrue(TEXT("Cover from the plan on screen wins against a slightly nearer point"), UsesCover(StickyPlan, 2));

	Request.CoverPoints[0].Location = FVector(2000.0f, 10.0f, 0.0f);
	FSquadMovePlan ClearlyNearerPlan;
	FSquadMovePlanner::BuildPlan(Request, ClearlyNearerPlan);
	TestTrue(TEXT("A clearly nearer point still takes over"), UsesCover(ClearlyNearerPlan, 1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSquadMovePlannerBehindCoverTest,
	"RTS.SquadMovePlanner.SoldiersWithoutCoverLineUpBehindIt",
	SquadMovePlannerTestsPrivate::TestFlags)

bool FSquadMovePlannerBehindCoverTest::RunTest(const FString& Parameters)
{
	using namespace SquadMovePlannerTestsPrivate;
	FSquadMovePlanRequest Request = MakeRequest(FVector(2000.0f, 0.0f, 0.0f));
	AddSquad(Request, 4, FVector::ZeroVector);
	const FVector CoverLocation(2000.0f, 0.0f, 0.0f);
	Request.CoverPoints.Add(MakeCover(1, CoverLocation, FVector::ForwardVector, ERTSCoverType::Crouch));

	FSquadMovePlan Plan;
	FSquadMovePlanner::BuildPlan(Request, Plan);
	TestEqual(TEXT("One soldier takes the single cover point"),
		CountType(Plan, ESquadPlannedPositionType::CrouchCover), 1);
	for (const FSquadUnitPlannedPosition& Position : Plan.UnitPositions)
	{
		if (Position.Type != ESquadPlannedPositionType::RegularStanding)
		{
			continue;
		}
		// The covered soldier looks along +X at its wall; the others must be on the -X side of him.
		TestTrue(TEXT("Soldiers without cover stand behind the covered one, not in front of the wall"),
			Position.Location.X < CoverLocation.X - Request.Settings.MinimumSlotSpacing);
	}
	return true;
}

namespace SquadMovePlannerTestsPrivate
{
	/** Squads of four standing side by side behind the origin, planned in the open at Anchor. */
	FSquadMovePlan PlanOpenFormation(const EFormation Formation, const int32 SquadCount, const FVector& Anchor)
	{
		FSquadMovePlanRequest Request = MakeRequest(Anchor);
		Request.Formation = Formation;
		for (int32 SquadIndex = 0; SquadIndex < SquadCount; ++SquadIndex)
		{
			AddSquad(Request, 4, FVector(0.0f, static_cast<float>(SquadIndex) * 1000.0f, 0.0f));
		}
		FSquadMovePlan Plan;
		FSquadMovePlanner::BuildPlan(Request, Plan);
		return Plan;
	}

	/** Number of distinct values of the anchors along Axis, which is the number of rows or columns of squads. */
	int32 CountDistinctAnchorOffsets(const FSquadMovePlan& Plan, const FVector& Axis)
	{
		TArray<float> Offsets;
		for (const FVector& SquadAnchor : Plan.SquadAnchors)
		{
			const float Offset = FVector::DotProduct(SquadAnchor, Axis);
			if (not Offsets.ContainsByPredicate([Offset](const float Known) { return FMath::IsNearlyEqual(Known, Offset, 1.0f); }))
			{
				Offsets.Add(Offset);
			}
		}
		return Offsets.Num();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSquadMovePlannerRectangleTest,
	"RTS.SquadMovePlanner.RectangleFormationIsABlockNotALine",
	SquadMovePlannerTestsPrivate::TestFlags)

bool FSquadMovePlannerRectangleTest::RunTest(const FString& Parameters)
{
	using namespace SquadMovePlannerTestsPrivate;
	const FVector Anchor(5000.0f, 0.0f, 0.0f);
	const FSquadMovePlan Plan = PlanOpenFormation(EFormation::RectangleFormation, 6, Anchor);
	TestEqual(TEXT("Six squads stand three wide"), CountDistinctAnchorOffsets(Plan, FVector::RightVector), 3);
	TestEqual(TEXT("Six squads stand two deep"), CountDistinctAnchorOffsets(Plan, FVector::ForwardVector), 2);
	FVector AnchorSum = FVector::ZeroVector;
	for (const FVector& SquadAnchor : Plan.SquadAnchors)
	{
		AnchorSum += SquadAnchor;
	}
	TestTrue(TEXT("The rectangle is centred on the cursor"), FVector::Dist2D(AnchorSum / 6.0f, Anchor) < 1.0f);
	TestEqual(TEXT("Without cover everybody stands in the open"),
		CountType(Plan, ESquadPlannedPositionType::RegularStanding), 24);
	const FSquadMovePlannerSettings Settings;
	TestTrue(TEXT("Blocks of different squads do not run into each other"),
		SmallestSpacing(Plan) >= Settings.RegularUnitSpacing - 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSquadMovePlannerFormationShapeTest,
	"RTS.SquadMovePlanner.FormationShapePlacesTheSquads",
	SquadMovePlannerTestsPrivate::TestFlags)

bool FSquadMovePlannerFormationShapeTest::RunTest(const FString& Parameters)
{
	using namespace SquadMovePlannerTestsPrivate;
	const FVector Anchor(5000.0f, 0.0f, 0.0f);

	const FSquadMovePlan SpearPlan = PlanOpenFormation(EFormation::SpearFormation, 6, Anchor);
	TestEqual(TEXT("A spear of six squads is 1 + 2 + 3 rows deep"),
		CountDistinctAnchorOffsets(SpearPlan, FVector::ForwardVector), 3);
	int32 SquadsOnCursor = 0;
	for (const FVector& SquadAnchor : SpearPlan.SquadAnchors)
	{
		SquadsOnCursor += FVector::Dist2D(SquadAnchor, Anchor) < 1.0f ? 1 : 0;
		TestTrue(TEXT("Nobody stands in front of the spear's tip"), SquadAnchor.X < Anchor.X + 1.0f);
	}
	TestEqual(TEXT("The tip of the spear is one squad on the cursor"), SquadsOnCursor, 1);

	const FSquadMovePlan ThinSpearPlan = PlanOpenFormation(EFormation::ThinSpearFormation, 9, Anchor);
	TestEqual(TEXT("A thin spear never gets wider than three squads"),
		CountDistinctAnchorOffsets(ThinSpearPlan, FVector::ForwardVector), 4);

	const FSquadMovePlan ArcPlan = PlanOpenFormation(EFormation::SemiCircleFormation, 3, Anchor);
	int32 OutwardFacingSquads = 0;
	for (int32 SquadIndex = 0; SquadIndex < ArcPlan.SquadAnchors.Num(); ++SquadIndex)
	{
		const float LateralOffset = ArcPlan.SquadAnchors[SquadIndex].Y - Anchor.Y;
		if (FMath::IsNearlyZero(LateralOffset, 1.0f))
		{
			TestTrue(TEXT("The middle squad of the arc looks straight ahead"),
				ArcPlan.SquadFacings[SquadIndex].Equals(FVector::ForwardVector, KINDA_SMALL_NUMBER));
			continue;
		}
		// Facing +X, +Y is the right-hand side: a squad on a wing looks toward its own side.
		OutwardFacingSquads += ArcPlan.SquadFacings[SquadIndex].Y * LateralOffset > 0.0f ? 1 : 0;
		TestTrue(TEXT("The wings of the arc hang back"), ArcPlan.SquadAnchors[SquadIndex].X < Anchor.X - 1.0f);
	}
	TestEqual(TEXT("Both wings of the arc face outward"), OutwardFacingSquads, 2);
	return true;
}

#endif
