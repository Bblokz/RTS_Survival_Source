// Copyright (C) Bas Blokzijl - All rights reserved.

#include "Misc/Build.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Misc/AutomationTest.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/MapTags/WorldMapTagsMath.h"

namespace WorldMapTagsMathTestData
{
	constexpr float Tolerance = 0.01f;
	constexpr float DistancePerTurn = 5000.f;
	const FVector2D TagSize(120.f, 24.f);
	constexpr float TagGap = 6.f;
	constexpr float TopBarHeight = 55.f;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldMapTagsRemainingPathTest, "RTS.WorldCampaign.UI.MapTags.RemainingPath",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldMapTagsRemainingPathTest::RunTest(const FString& Parameters)
{
	using namespace WorldMapTagsMathTestData;
	// Z differences are ignored, like the division's own movement budget.
	const TArray<FVector> PathPoints = {FVector(0.f, 0.f, 0.f), FVector(3000.f, 0.f, 500.f), FVector(3000.f, 4000.f, 0.f)};
	const FVector PieceLocation(1000.f, 0.f, 900.f);

	TestEqual(TEXT("From the piece through the remaining points"),
	          WorldMapTagsMath::GetRemainingPathLengthXY(PieceLocation, PathPoints, 1), 6000.f, Tolerance);
	TestEqual(TEXT("Only the last leg left"),
	          WorldMapTagsMath::GetRemainingPathLengthXY(FVector(3000.f, 1000.f, 0.f), PathPoints, 2), 3000.f, Tolerance);
	TestEqual(TEXT("A used-up order has no length"),
	          WorldMapTagsMath::GetRemainingPathLengthXY(PieceLocation, PathPoints, PathPoints.Num()), 0.f, Tolerance);
	TestEqual(TEXT("A negative index has no length"),
	          WorldMapTagsMath::GetRemainingPathLengthXY(PieceLocation, PathPoints, INDEX_NONE), 0.f, Tolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldMapTagsTurnsAwayTest, "RTS.WorldCampaign.UI.MapTags.TurnsAway",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldMapTagsTurnsAwayTest::RunTest(const FString& Parameters)
{
	using namespace WorldMapTagsMathTestData;
	TestEqual(TEXT("Nothing left"), WorldMapTagsMath::GetTurnsAway(0.f, DistancePerTurn), 0);
	TestEqual(TEXT("A short hop still takes a turn"), WorldMapTagsMath::GetTurnsAway(10.f, DistancePerTurn), 1);
	TestEqual(TEXT("Exactly one turn"), WorldMapTagsMath::GetTurnsAway(DistancePerTurn, DistancePerTurn), 1);
	TestEqual(TEXT("Rounded up"), WorldMapTagsMath::GetTurnsAway(DistancePerTurn + 1.f, DistancePerTurn), 2);
	TestEqual(TEXT("Three and a half turns"), WorldMapTagsMath::GetTurnsAway(17500.f, DistancePerTurn), 4);
	TestEqual(TEXT("A piece that cannot move"), WorldMapTagsMath::GetTurnsAway(1000.f, 0.f), static_cast<int32>(INDEX_NONE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldMapTagsClipSegmentTest, "RTS.WorldCampaign.UI.MapTags.ClipSegment",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldMapTagsClipSegmentTest::RunTest(const FString& Parameters)
{
	using namespace WorldMapTagsMathTestData;
	const FVector2D RectMin(0.f, 0.f);
	const FVector2D RectMax(100.f, 100.f);

	FVector2D Start(50.f, 50.f);
	FVector2D End(80.f, 60.f);
	TestTrue(TEXT("Inside stays"), WorldMapTagsMath::ClipSegmentToRect(Start, End, RectMin, RectMax));
	TestTrue(TEXT("Inside start unchanged"), Start.Equals(FVector2D(50.f, 50.f), Tolerance));
	TestTrue(TEXT("Inside end unchanged"), End.Equals(FVector2D(80.f, 60.f), Tolerance));

	Start = FVector2D(50.f, 50.f);
	End = FVector2D(100050.f, 50.f);
	TestTrue(TEXT("A far target is cut"), WorldMapTagsMath::ClipSegmentToRect(Start, End, RectMin, RectMax));
	TestTrue(TEXT("Cut at the right edge"), End.Equals(FVector2D(100.f, 50.f), Tolerance));

	Start = FVector2D(-50.f, -50.f);
	End = FVector2D(150.f, 150.f);
	TestTrue(TEXT("Crossing diagonal"), WorldMapTagsMath::ClipSegmentToRect(Start, End, RectMin, RectMax));
	TestTrue(TEXT("Diagonal enters at the corner"), Start.Equals(RectMin, Tolerance));
	TestTrue(TEXT("Diagonal leaves at the corner"), End.Equals(RectMax, Tolerance));

	Start = FVector2D(-50.f, 200.f);
	End = FVector2D(-10.f, 300.f);
	TestFalse(TEXT("Wholly outside"), WorldMapTagsMath::ClipSegmentToRect(Start, End, RectMin, RectMax));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldMapTagsDashTest, "RTS.WorldCampaign.UI.MapTags.Dashes",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldMapTagsDashTest::RunTest(const FString& Parameters)
{
	using namespace WorldMapTagsMathTestData;
	TArray<FWorldMapDashSegment> Dashes;
	WorldMapTagsMath::BuildDashSegments(FVector2D(0.f, 0.f), FVector2D(30.f, 0.f), 8.f, 6.f, 100, Dashes);
	// Dashes start at 0, 14 and 28; the last is cut to end on the line's end.
	TestEqual(TEXT("Three dashes"), Dashes.Num(), 3);
	if (Dashes.Num() == 3)
	{
		TestTrue(TEXT("First dash"), Dashes[0].End.Equals(FVector2D(8.f, 0.f), Tolerance));
		TestTrue(TEXT("Second dash start"), Dashes[1].Start.Equals(FVector2D(14.f, 0.f), Tolerance));
		TestTrue(TEXT("Last dash ends on the line end"), Dashes[2].End.Equals(FVector2D(30.f, 0.f), Tolerance));
	}

	WorldMapTagsMath::BuildDashSegments(FVector2D::ZeroVector, FVector2D(0.f, 100000.f), 8.f, 6.f, 50, Dashes);
	TestEqual(TEXT("Capped"), Dashes.Num(), 50);

	WorldMapTagsMath::BuildDashSegments(FVector2D::ZeroVector, FVector2D::ZeroVector, 8.f, 6.f, 50, Dashes);
	TestEqual(TEXT("A point has no dashes"), Dashes.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldMapTagsLabelPointTest, "RTS.WorldCampaign.UI.MapTags.CountryLabelPoint",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldMapTagsLabelPointTest::RunTest(const FString& Parameters)
{
	using namespace WorldMapTagsMathTestData;
	FVector LabelPoint;
	TestFalse(TEXT("No anchors, no label"), WorldMapTagsMath::TryGetCountryLabelPoint({}, LabelPoint));
	TestTrue(TEXT("Mean of the anchors"), WorldMapTagsMath::TryGetCountryLabelPoint(
		         {FVector(0.f, 0.f, 0.f), FVector(400.f, 0.f, 30.f), FVector(200.f, 600.f, 0.f)}, LabelPoint));
	TestTrue(TEXT("Mean value"), LabelPoint.Equals(FVector(200.f, 200.f, 10.f), Tolerance));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldMapTagsTopBarTest, "RTS.WorldCampaign.UI.MapTags.TopBar",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldMapTagsTopBarTest::RunTest(const FString& Parameters)
{
	using namespace WorldMapTagsMathTestData;
	// A 24 high tag centred on 62 starts at 50, inside a 55 high bar.
	TestFalse(TEXT("Half under the bar"), WorldMapTagsMath::GetIsTagClearOfTopBar(FVector2D(880.f, 62.f), TagSize,
	                                                                              TopBarHeight));
	TestFalse(TEXT("Wholly under the bar"), WorldMapTagsMath::GetIsTagClearOfTopBar(FVector2D(880.f, 20.f), TagSize,
	                                                                                TopBarHeight));
	TestTrue(TEXT("Touching the bar's lower edge"), WorldMapTagsMath::GetIsTagClearOfTopBar(
		         FVector2D(880.f, 67.f), TagSize, TopBarHeight));
	TestTrue(TEXT("Well below the bar"), WorldMapTagsMath::GetIsTagClearOfTopBar(FVector2D(880.f, 500.f), TagSize,
	                                                                             TopBarHeight));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldMapTagsMarkerTest, "RTS.WorldCampaign.UI.MapTags.ClearOfMarker",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldMapTagsMarkerTest::RunTest(const FString& Parameters)
{
	using namespace WorldMapTagsMathTestData;
	// The marker's box runs from 915 to 995 across and from 516 to 584 down.
	const FVector2D MarkerCenter(955.f, 550.f);
	const FVector2D MarkerHalfSize(40.f, 34.f);

	const FVector2D Lifted = WorldMapTagsMath::GetTagCenterClearOfMarker(FVector2D(975.f, 516.f), TagSize, MarkerCenter,
	                                                                     MarkerHalfSize, TagGap);
	// Tag bottom ends the gap above the marker's top: 516 - 6 - 12.
	TestTrue(TEXT("A tag on the marker is lifted above it"), Lifted.Equals(FVector2D(975.f, 498.f), Tolerance));

	const FVector2D Beside(1300.f, 516.f);
	TestTrue(TEXT("A tag beside the marker stays"), WorldMapTagsMath::GetTagCenterClearOfMarker(
		         Beside, TagSize, MarkerCenter, MarkerHalfSize, TagGap).Equals(Beside, Tolerance));

	const FVector2D Above(975.f, 480.f);
	TestTrue(TEXT("A tag already clear above the marker stays"), WorldMapTagsMath::GetTagCenterClearOfMarker(
		         Above, TagSize, MarkerCenter, MarkerHalfSize, TagGap).Equals(Above, Tolerance));

	TestTrue(TEXT("A lifted tag is clear of the marker"), WorldMapTagsMath::GetTagCenterClearOfMarker(
		         Lifted, TagSize, MarkerCenter, MarkerHalfSize, TagGap).Equals(Lifted, Tolerance));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldMapTagsOverlapTest, "RTS.WorldCampaign.UI.MapTags.ResolveOverlaps",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldMapTagsOverlapTest::RunTest(const FString& Parameters)
{
	using namespace WorldMapTagsMathTestData;
	TestTrue(TEXT("Crossing tags overlap"), WorldMapTagsMath::GetDoTagsOverlap(
		         FVector2D(100.f, 100.f), TagSize, FVector2D(150.f, 110.f), TagSize, TagGap));
	TestFalse(TEXT("Tags a gap apart do not overlap"), WorldMapTagsMath::GetDoTagsOverlap(
		          FVector2D(100.f, 100.f), TagSize, FVector2D(100.f, 100.f + TagSize.Y + TagGap), TagSize, TagGap));
	TestFalse(TEXT("Tags side by side do not overlap"), WorldMapTagsMath::GetDoTagsOverlap(
		          FVector2D(100.f, 100.f), TagSize, FVector2D(400.f, 100.f), TagSize, TagGap));

	TArray<FWorldMapTagPlacement> Tags;
	const auto AddTag = [&Tags](const FVector2D& Center, const bool bIsVisible)
	{
		FWorldMapTagPlacement Tag;
		Tag.Center = Center;
		Tag.Size = TagSize;
		Tag.bIsVisible = bIsVisible;
		Tags.Add(Tag);
	};
	// 0 wins, 1 is nudged below it, 2 has no room left, 3 is under the bar, 5 would be nudged into the bar,
	// 6 is hidden already and so does not push 7.
	AddTag(FVector2D(100.f, 100.f), true);
	AddTag(FVector2D(150.f, 110.f), true);
	AddTag(FVector2D(140.f, 126.f), true);
	AddTag(FVector2D(500.f, 60.f), true);
	AddTag(FVector2D(800.f, 80.f), true);
	AddTag(FVector2D(800.f, 75.f), true);
	AddTag(FVector2D(1200.f, 300.f), false);
	AddTag(FVector2D(1200.f, 300.f), true);
	WorldMapTagsMath::ResolveTagOverlaps(Tags, TagGap, TopBarHeight);

	TestTrue(TEXT("The first tag stays where it is"),
	         Tags[0].bIsVisible && Tags[0].Center.Equals(FVector2D(100.f, 100.f), Tolerance));
	// Below the first tag by both half heights and the gap: 100 + 24 + 6.
	TestTrue(TEXT("The second tag is nudged below the first"),
	         Tags[1].bIsVisible && Tags[1].Center.Equals(FVector2D(150.f, 130.f), Tolerance));
	TestFalse(TEXT("A tag with no free spot is hidden"), Tags[2].bIsVisible);
	TestFalse(TEXT("A tag under the bar is hidden"), Tags[3].bIsVisible);
	TestTrue(TEXT("A tag below the bar stays"), Tags[4].bIsVisible);
	TestFalse(TEXT("A tag nudged into the bar is hidden"), Tags[5].bIsVisible);
	TestFalse(TEXT("A hidden tag stays hidden"), Tags[6].bIsVisible);
	TestTrue(TEXT("A hidden tag pushes nobody"),
	         Tags[7].bIsVisible && Tags[7].Center.Equals(FVector2D(1200.f, 300.f), Tolerance));
	return true;
}

#endif
