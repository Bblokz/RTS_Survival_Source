#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RTS_Survival/Player/Formation/FormationDragLineLayout.h"
#include "RTS_Survival/Player/Formation/FormationDragPath.h"

namespace FormationDragLineTestsPrivate
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	constexpr float LocationTolerance = 0.5f;
	constexpr float DirectionTolerance = 0.01f;

	/** A drag that passed through every corner, sampled every StepLength like a moving cursor would be. */
	FFormationDragPath MakeDragPath(const TArray<FVector>& Corners, const float StepLength = 50.0f)
	{
		FFormationDragPath DragPath;
		DragPath.Start(Corners[0]);
		for (int32 CornerIndex = 1; CornerIndex < Corners.Num(); ++CornerIndex)
		{
			const FVector LegStart = Corners[CornerIndex - 1];
			const FVector LegEnd = Corners[CornerIndex];
			const int32 StepCount = FMath::Max(1, FMath::RoundToInt32(FVector::Dist(LegStart, LegEnd) / StepLength));
			for (int32 StepIndex = 1; StepIndex <= StepCount; ++StepIndex)
			{
				DragPath.TryAddPoint(FMath::Lerp(LegStart, LegEnd, static_cast<float>(StepIndex) / StepCount));
			}
		}
		return DragPath;
	}

	/** A straight drag from the origin along +Y, which makes the units face +X. */
	FFormationDragPath MakeStraightDragPath(const float Length)
	{
		return MakeDragPath({FVector::ZeroVector, FVector(0.0f, Length, 0.0f)});
	}

	TArray<float> MakeEqualRadii(const int32 UnitCount, const float Radius)
	{
		TArray<float> UnitRadii;
		UnitRadii.Init(Radius, UnitCount);
		return UnitRadii;
	}

	/** Splits the radii into rows the way the formation controller does, without reordering inside a row. */
	TArray<TArray<float>> SplitIntoRows(const TArray<float>& UnitRadii, const TArray<int32>& RowUnitCounts)
	{
		TArray<TArray<float>> RowUnitRadii;
		int32 FirstUnitOfRow = 0;
		for (const int32 RowUnitCount : RowUnitCounts)
		{
			RowUnitRadii.Emplace(UnitRadii.GetData() + FirstUnitOfRow, RowUnitCount);
			FirstUnitOfRow += RowUnitCount;
		}
		return RowUnitRadii;
	}

	void LayOutUnits(
		const TArray<float>& UnitRadii,
		const FFormationDragPath& DragPath,
		TArray<int32>& OutRowUnitCounts,
		TArray<FVector>& OutPositions,
		TArray<FRotator>& OutRotations)
	{
		OutRowUnitCounts = FFormationDragLineLayout::ComputeRowUnitCounts(UnitRadii, DragPath.GetLength());
		FFormationDragLineLayout::PlaceRows(
			SplitIntoRows(UnitRadii, OutRowUnitCounts), DragPath, OutPositions, OutRotations);
	}

	bool GetIsNear(const FVector& Location, const FVector& Expected)
	{
		return Location.Equals(Expected, LocationTolerance);
	}

	bool GetFaces(const FRotator& Rotation, const FVector& ExpectedFacing)
	{
		return Rotation.Vector().Equals(ExpectedFacing, DirectionTolerance);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormationDragPathLengthTest,
	"RTS.Formation.DragLine.PathMeasuresTheDragHistory",
	FormationDragLineTestsPrivate::TestFlags)

bool FFormationDragPathLengthTest::RunTest(const FString& Parameters)
{
	using namespace FormationDragLineTestsPrivate;
	FFormationDragPath DragPath;
	TestFalse(TEXT("A path that was never started is not started"), DragPath.GetIsStarted());
	TestFalse(TEXT("A path that was never started takes no points"), DragPath.TryAddPoint(FVector(500.0f, 0.0f, 0.0f)));

	DragPath.Start(FVector::ZeroVector);
	TestTrue(TEXT("Start begins the path"), DragPath.GetIsStarted());
	TestFalse(TEXT("Cursor jitter does not extend the path"), DragPath.TryAddPoint(FVector(10.0f, 5.0f, 0.0f)));
	TestEqual(TEXT("Jitter adds no length"), DragPath.GetLength(), 0.0f);
	TestTrue(TEXT("A real movement extends the path"), DragPath.TryAddPoint(FVector(0.0f, 300.0f, 0.0f)));
	TestTrue(TEXT("A second leg extends the path"), DragPath.TryAddPoint(FVector(400.0f, 300.0f, 0.0f)));

	TestEqual(TEXT("The length is the sum of both legs"), DragPath.GetLength(), 700.0f, LocationTolerance);
	TestTrue(TEXT("A distance on the first leg lies on the first leg"),
		GetIsNear(DragPath.GetLocationAtDistance(150.0f), FVector(0.0f, 150.0f, 0.0f)));
	TestTrue(TEXT("A distance past the corner lies on the second leg"),
		GetIsNear(DragPath.GetLocationAtDistance(500.0f), FVector(200.0f, 300.0f, 0.0f)));
	TestTrue(TEXT("A negative distance clamps to the start"),
		GetIsNear(DragPath.GetLocationAtDistance(-50.0f), FVector::ZeroVector));
	TestTrue(TEXT("A distance past the end clamps to the end"),
		GetIsNear(DragPath.GetLocationAtDistance(5000.0f), FVector(400.0f, 300.0f, 0.0f)));

	DragPath.Reset();
	TestFalse(TEXT("Reset ends the path"), DragPath.GetIsStarted());
	TestEqual(TEXT("A reset path has no length"), DragPath.GetLength(), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormationDragPathFacingTest,
	"RTS.Formation.DragLine.FacingIsLeftOfTheDrag",
	FormationDragLineTestsPrivate::TestFlags)

bool FFormationDragPathFacingTest::RunTest(const FString& Parameters)
{
	using namespace FormationDragLineTestsPrivate;
	const FFormationDragPath DragRight = MakeStraightDragPath(1000.0f);
	TestTrue(TEXT("Dragging along +Y faces the units along +X"),
		DragRight.GetFacingAtDistance(500.0f).Equals(FVector::ForwardVector, DirectionTolerance));
	TestTrue(TEXT("The overall facing of a straight drag is its local facing"),
		DragRight.GetOverallFacing().Equals(FVector::ForwardVector, DirectionTolerance));
	TestTrue(TEXT("The drag direction is the right-hand side of the facing"),
		DragRight.GetOverallFacing().Rotation().RotateVector(FVector::RightVector).Equals(
			FVector::RightVector, DirectionTolerance));
	TestTrue(TEXT("The facing at the very start of the line is defined"),
		DragRight.GetFacingAtDistance(0.0f).Equals(FVector::ForwardVector, DirectionTolerance));

	const FFormationDragPath DragLeft = MakeDragPath({FVector::ZeroVector, FVector(0.0f, -1000.0f, 0.0f)});
	TestTrue(TEXT("Dragging the other way turns the units around"),
		DragLeft.GetFacingAtDistance(500.0f).Equals(-FVector::ForwardVector, DirectionTolerance));

	const FFormationDragPath CornerDrag = MakeDragPath(
		{FVector::ZeroVector, FVector(0.0f, 1000.0f, 0.0f), FVector(1000.0f, 1000.0f, 0.0f)});
	TestTrue(TEXT("Units on the first leg of a bent drag face along that leg's left"),
		CornerDrag.GetFacingAtDistance(300.0f).Equals(FVector::ForwardVector, DirectionTolerance));
	TestTrue(TEXT("Units on the second leg face along the second leg's left"),
		CornerDrag.GetFacingAtDistance(1700.0f).Equals(-FVector::RightVector, DirectionTolerance));

	FFormationDragPath SinglePoint;
	SinglePoint.Start(FVector(100.0f, 100.0f, 0.0f));
	TestTrue(TEXT("A path without a direction still returns a unit facing"),
		SinglePoint.GetFacingAtDistance(0.0f).IsNormalized());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormationDragPathPointCapTest,
	"RTS.Formation.DragLine.VeryLongDragKeepsBoundedPoints",
	FormationDragLineTestsPrivate::TestFlags)

bool FFormationDragPathPointCapTest::RunTest(const FString& Parameters)
{
	using namespace FormationDragLineTestsPrivate;
	constexpr float DragLength = 50000.0f;
	constexpr int32 MaximumExpectedPoints = 128;
	// The end of the path may trail the cursor by the thinned-out point spacing.
	constexpr float AllowedEndLagFraction = 0.03f;

	const FFormationDragPath DragPath = MakeStraightDragPath(DragLength);
	TestTrue(TEXT("The point count stays bounded"), DragPath.GetPoints().Num() < MaximumExpectedPoints);
	TestTrue(TEXT("The path keeps its start"), GetIsNear(DragPath.GetStartLocation(), FVector::ZeroVector));
	TestTrue(TEXT("The path is never longer than the drag"), DragPath.GetLength() <= DragLength + LocationTolerance);
	TestTrue(TEXT("The path keeps close to the full length of the drag"),
		DragPath.GetLength() >= DragLength * (1.0f - AllowedEndLagFraction));
	TestTrue(TEXT("Thinning out a straight drag keeps it straight"),
		GetIsNear(DragPath.GetLocationAtDistance(DragPath.GetLength() * 0.5f),
		          FVector(0.0f, DragPath.GetLength() * 0.5f, 0.0f)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormationDragLineSingleRowTest,
	"RTS.Formation.DragLine.LongLineSpreadsOneRowEndToEnd",
	FormationDragLineTestsPrivate::TestFlags)

bool FFormationDragLineSingleRowTest::RunTest(const FString& Parameters)
{
	using namespace FormationDragLineTestsPrivate;
	const FFormationDragPath DragPath = MakeStraightDragPath(900.0f);
	TArray<int32> RowUnitCounts;
	TArray<FVector> Positions;
	TArray<FRotator> Rotations;
	LayOutUnits(MakeEqualRadii(4, 100.0f), DragPath, RowUnitCounts, Positions, Rotations);

	TestEqual(TEXT("Four small units fit in one row on a long line"), RowUnitCounts, TArray<int32>{4});
	if (not TestEqual(TEXT("Every unit gets a slot"), Positions.Num(), 4) ||
		not TestEqual(TEXT("Every slot gets a facing"), Rotations.Num(), 4))
	{
		return false;
	}
	TestTrue(TEXT("The first unit stands on the start of the line"), GetIsNear(Positions[0], FVector::ZeroVector));
	TestTrue(TEXT("The units are spread evenly"), GetIsNear(Positions[1], FVector(0.0f, 300.0f, 0.0f)));
	TestTrue(TEXT("The units are spread evenly"), GetIsNear(Positions[2], FVector(0.0f, 600.0f, 0.0f)));
	TestTrue(TEXT("The last unit stands on the end of the line"), GetIsNear(Positions[3], FVector(0.0f, 900.0f, 0.0f)));
	for (const FRotator& Rotation : Rotations)
	{
		TestTrue(TEXT("Every unit faces to the left of the drag"), GetFaces(Rotation, FVector::ForwardVector));
	}

	// A longer drag scales the same formation up.
	const FFormationDragPath LongerDragPath = MakeStraightDragPath(1800.0f);
	LayOutUnits(MakeEqualRadii(4, 100.0f), LongerDragPath, RowUnitCounts, Positions, Rotations);
	TestEqual(TEXT("A longer line still is one row"), RowUnitCounts, TArray<int32>{4});
	TestTrue(TEXT("A line twice as long doubles the spacing"), GetIsNear(Positions[1], FVector(0.0f, 600.0f, 0.0f)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormationDragLineExtraRowsTest,
	"RTS.Formation.DragLine.ShortLineStacksRowsBehind",
	FormationDragLineTestsPrivate::TestFlags)

bool FFormationDragLineExtraRowsTest::RunTest(const FString& Parameters)
{
	using namespace FormationDragLineTestsPrivate;
	// Three units of radius 100 span 200 shoulder to shoulder, a fourth would need 300.
	const FFormationDragPath DragPath = MakeStraightDragPath(250.0f);
	TArray<int32> RowUnitCounts;
	TArray<FVector> Positions;
	TArray<FRotator> Rotations;
	LayOutUnits(MakeEqualRadii(5, 100.0f), DragPath, RowUnitCounts, Positions, Rotations);

	TestEqual(TEXT("The line holds three units, the rest forms a second row"), RowUnitCounts, TArray<int32>{3, 2});
	if (not TestEqual(TEXT("Every unit gets a slot"), Positions.Num(), 5))
	{
		return false;
	}
	TestTrue(TEXT("The front row starts on the start of the line"), GetIsNear(Positions[0], FVector::ZeroVector));
	TestTrue(TEXT("The front row is stretched over the line"), GetIsNear(Positions[1], FVector(0.0f, 125.0f, 0.0f)));
	TestTrue(TEXT("The front row ends on the end of the line"), GetIsNear(Positions[2], FVector(0.0f, 250.0f, 0.0f)));

	// Rows are (100 + 100) / 1.5 apart, like in the rectangle formation.
	constexpr float ExpectedRowDepth = 133.333f;
	TestTrue(TEXT("The second row stands behind the line, centred, with the front row's spacing"),
		GetIsNear(Positions[3], FVector(-ExpectedRowDepth, 62.5f, 0.0f)));
	TestTrue(TEXT("The second row stands behind the line, centred, with the front row's spacing"),
		GetIsNear(Positions[4], FVector(-ExpectedRowDepth, 187.5f, 0.0f)));
	TestTrue(TEXT("The second row faces the same way as the front row"), GetFaces(Rotations[4], FVector::ForwardVector));

	// A line shorter than a single unit leaves one unit per row: a column behind the middle of the line.
	const FFormationDragPath ShortDragPath = MakeStraightDragPath(200.0f);
	LayOutUnits(MakeEqualRadii(3, 300.0f), ShortDragPath, RowUnitCounts, Positions, Rotations);
	TestEqual(TEXT("Units wider than the line each get their own row"), RowUnitCounts, TArray<int32>{1, 1, 1});
	TestTrue(TEXT("A lone unit stands on the middle of the line"), GetIsNear(Positions[0], FVector(0.0f, 100.0f, 0.0f)));
	TestTrue(TEXT("The column continues behind the line"), GetIsNear(Positions[1], FVector(-400.0f, 100.0f, 0.0f)));
	TestTrue(TEXT("The column continues behind the line"), GetIsNear(Positions[2], FVector(-800.0f, 100.0f, 0.0f)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormationDragLineMixedSizesTest,
	"RTS.Formation.DragLine.MixedSizesNeverOverlap",
	FormationDragLineTestsPrivate::TestFlags)

bool FFormationDragLineMixedSizesTest::RunTest(const FString& Parameters)
{
	using namespace FormationDragLineTestsPrivate;
	const TArray<float> UnitRadii = {300.0f, 100.0f, 100.0f, 200.0f, 100.0f, 300.0f, 150.0f, 400.0f, 100.0f};
	const TArray<float> LineLengths = {200.0f, 450.0f, 700.0f, 1200.0f, 4000.0f};
	for (const float LineLength : LineLengths)
	{
		const FFormationDragPath DragPath = MakeStraightDragPath(LineLength);
		TArray<int32> RowUnitCounts;
		TArray<FVector> Positions;
		TArray<FRotator> Rotations;
		LayOutUnits(UnitRadii, DragPath, RowUnitCounts, Positions, Rotations);

		int32 PlacedUnitCount = 0;
		for (const int32 RowUnitCount : RowUnitCounts)
		{
			TestTrue(TEXT("No row is empty"), RowUnitCount > 0);
			PlacedUnitCount += RowUnitCount;
		}
		TestEqual(TEXT("The rows hold every unit exactly once"), PlacedUnitCount, UnitRadii.Num());
		if (not TestEqual(TEXT("Every unit gets a slot"), Positions.Num(), UnitRadii.Num()))
		{
			return false;
		}

		int32 FirstUnitOfRow = 0;
		float PreviousRowDepth = -1.0f;
		for (const int32 RowUnitCount : RowUnitCounts)
		{
			for (int32 UnitInRow = 1; UnitInRow < RowUnitCount; ++UnitInRow)
			{
				const int32 UnitIndex = FirstUnitOfRow + UnitInRow;
				const float NeededSpacing = FFormationDragLineLayout::GetNeighbourSpacing(
					UnitRadii[UnitIndex - 1], UnitRadii[UnitIndex]);
				TestTrue(TEXT("Neighbours in a row keep at least their shoulder-to-shoulder spacing"),
					FVector::Dist2D(Positions[UnitIndex - 1], Positions[UnitIndex]) >= NeededSpacing - LocationTolerance);
			}
			for (int32 UnitInRow = 0; UnitInRow < RowUnitCount; ++UnitInRow)
			{
				const float DistanceAlongLine = Positions[FirstUnitOfRow + UnitInRow].Y;
				TestTrue(TEXT("No unit is placed past the ends of the line"),
					DistanceAlongLine >= -LocationTolerance && DistanceAlongLine <= LineLength + LocationTolerance);
			}
			const float RowDepth = -Positions[FirstUnitOfRow].X;
			TestTrue(TEXT("Every row stands further behind the line than the row before it"), RowDepth > PreviousRowDepth);
			PreviousRowDepth = RowDepth;
			FirstUnitOfRow += RowUnitCount;
		}
	}

	TArray<FVector> NoPositions;
	TArray<FRotator> NoRotations;
	TestEqual(TEXT("No units make no rows"),
		FFormationDragLineLayout::ComputeRowUnitCounts({}, 500.0f).Num(), 0);
	FFormationDragLineLayout::PlaceRows({}, MakeStraightDragPath(500.0f), NoPositions, NoRotations);
	TestEqual(TEXT("No rows make no slots"), NoPositions.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFormationDragLineBentLineTest,
	"RTS.Formation.DragLine.BentLineTurnsUnitsWithIt",
	FormationDragLineTestsPrivate::TestFlags)

bool FFormationDragLineBentLineTest::RunTest(const FString& Parameters)
{
	using namespace FormationDragLineTestsPrivate;
	const FFormationDragPath DragPath = MakeDragPath(
		{FVector::ZeroVector, FVector(0.0f, 1000.0f, 0.0f), FVector(1000.0f, 1000.0f, 0.0f)});
	TArray<int32> RowUnitCounts;
	TArray<FVector> Positions;
	TArray<FRotator> Rotations;
	LayOutUnits(MakeEqualRadii(5, 100.0f), DragPath, RowUnitCounts, Positions, Rotations);

	TestEqual(TEXT("The bent line is long enough for one row"), RowUnitCounts, TArray<int32>{5});
	if (not TestEqual(TEXT("Every unit gets a slot"), Positions.Num(), 5))
	{
		return false;
	}
	TestTrue(TEXT("Units follow the first leg"), GetIsNear(Positions[1], FVector(0.0f, 500.0f, 0.0f)));
	TestTrue(TEXT("A unit stands on the corner"), GetIsNear(Positions[2], FVector(0.0f, 1000.0f, 0.0f)));
	TestTrue(TEXT("Units follow the second leg"), GetIsNear(Positions[3], FVector(500.0f, 1000.0f, 0.0f)));
	TestTrue(TEXT("The last unit stands on the end of the bent line"),
		GetIsNear(Positions[4], FVector(1000.0f, 1000.0f, 0.0f)));

	TestTrue(TEXT("Units on the first leg face left of that leg"), GetFaces(Rotations[1], FVector::ForwardVector));
	TestTrue(TEXT("Units on the second leg face left of that leg"), GetFaces(Rotations[3], -FVector::RightVector));
	const FVector CornerFacing = (FVector::ForwardVector - FVector::RightVector).GetSafeNormal();
	TestTrue(TEXT("The unit on the corner faces between both legs"), GetFaces(Rotations[2], CornerFacing));
	return true;
}

#endif
