// Copyright (C) Bas Blokzijl - All rights reserved.

#include "Misc/Build.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Misc/AutomationTest.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/TechWheel/TechWheelLabelPlacement.h"

namespace TechWheelLabelPlacementTestData
{
	constexpr float Tolerance = 0.01f;
	constexpr float OutlineRadius = 60.f;
	const FVector2D LabelSize(100.f, 16.f);
	const FVector2D RingCentre(1000.f, 1000.f);
	constexpr float RingRadius = 500.f;

	FTechWheelLabelRing MakeRing()
	{
		FTechWheelLabelRing Ring;
		Ring.Centre = RingCentre;
		Ring.Radius = RingRadius;
		return Ring;
	}

	FTechWheelLabelRequest MakeRequest(const FVector2D& NodeCentre)
	{
		FTechWheelLabelRequest Request;
		Request.NodeCentre = NodeCentre;
		Request.OutlineRadius = OutlineRadius;
		Request.LabelSize = LabelSize;
		return Request;
	}

	FTechWheelLabelRect MakeRect(const FVector2D& Min, const FVector2D& Max)
	{
		FTechWheelLabelRect Rect;
		Rect.Min = Min;
		Rect.Max = Max;
		return Rect;
	}

	/** No corner of the rectangle lies inside the hexagon; enough for a label that sits beside it. */
	bool GetIsClearOfHexagon(const FTechWheelLabelRect& Rect, const FVector2D& Centre, const float Radius)
	{
		const FVector2D Corners[] = {
			Rect.Min, Rect.Max, FVector2D(Rect.Min.X, Rect.Max.Y), FVector2D(Rect.Max.X, Rect.Min.Y)
		};
		for (const FVector2D& Corner : Corners)
		{
			const float HalfWidth = TechWheelLabelPlacement::GetHexagonHalfWidthAt(
				Radius, static_cast<float>(Corner.Y - Centre.Y));
			if (FMath::Abs(static_cast<float>(Corner.X - Centre.X)) < HalfWidth)
			{
				return false;
			}
		}
		return true;
	}

	float GetCoveredArea(const FTechWheelLabelRect& Rect, const TArray<FTechWheelLabelObstacle>& Obstacles)
	{
		float CoveredArea = 0.f;
		for (const FTechWheelLabelObstacle& Obstacle : Obstacles)
		{
			CoveredArea += TechWheelLabelPlacement::GetOverlapArea(Rect, Obstacle.Rect);
		}
		return CoveredArea;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTechWheelLabelOverlapTest, "RTS.WorldCampaign.UI.TechWheel.LabelOverlapArea",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTechWheelLabelOverlapTest::RunTest(const FString& Parameters)
{
	using namespace TechWheelLabelPlacementTestData;
	const FTechWheelLabelRect First = MakeRect(FVector2D(0.f, 0.f), FVector2D(10.f, 10.f));
	TestEqual(TEXT("A shared corner of 4 by 6"), TechWheelLabelPlacement::GetOverlapArea(
		          First, MakeRect(FVector2D(6.f, 4.f), FVector2D(20.f, 20.f))), 24.f, Tolerance);
	TestEqual(TEXT("Touching edges share nothing"), TechWheelLabelPlacement::GetOverlapArea(
		          First, MakeRect(FVector2D(10.f, 0.f), FVector2D(20.f, 10.f))), 0.f, Tolerance);
	TestEqual(TEXT("Apart"), TechWheelLabelPlacement::GetOverlapArea(
		          First, MakeRect(FVector2D(30.f, 30.f), FVector2D(40.f, 40.f))), 0.f, Tolerance);
	TestEqual(TEXT("One inside the other"), TechWheelLabelPlacement::GetOverlapArea(
		          First, MakeRect(FVector2D(2.f, 2.f), FVector2D(4.f, 5.f))), 6.f, Tolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTechWheelLabelRingTest, "RTS.WorldCampaign.UI.TechWheel.LabelOutsideRing",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTechWheelLabelRingTest::RunTest(const FString& Parameters)
{
	using namespace TechWheelLabelPlacementTestData;
	const FTechWheelLabelRing Ring = MakeRing();
	const FVector2D HalfLabel = LabelSize * 0.5;
	TestEqual(TEXT("At the centre nothing is outside"), TechWheelLabelPlacement::GetAreaOutsideRing(
		          FTechWheelLabelRect::MakeCentred(RingCentre, HalfLabel), Ring), 0.f, Tolerance);
	TestEqual(TEXT("Far away everything is outside"), TechWheelLabelPlacement::GetAreaOutsideRing(
		          FTechWheelLabelRect::MakeCentred(RingCentre + FVector2D(RingRadius * 3.f, 0.f), HalfLabel), Ring),
	          static_cast<float>(LabelSize.X * LabelSize.Y), Tolerance);
	const float AcrossTheLine = TechWheelLabelPlacement::GetAreaOutsideRing(
		FTechWheelLabelRect::MakeCentred(RingCentre + FVector2D(RingRadius, 0.f), HalfLabel), Ring);
	TestTrue(TEXT("Across the line a part is outside"),
	         AcrossTheLine > 0.f && AcrossTheLine < static_cast<float>(LabelSize.X * LabelSize.Y));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTechWheelLabelHexagonTest, "RTS.WorldCampaign.UI.TechWheel.LabelHexagon",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTechWheelLabelHexagonTest::RunTest(const FString& Parameters)
{
	using namespace TechWheelLabelPlacementTestData;
	constexpr float FlatEdgeScale = 0.8660254f;
	TestEqual(TEXT("Corner to corner at the centre line"),
	          TechWheelLabelPlacement::GetHexagonHalfWidthAt(OutlineRadius, 0.f), OutlineRadius, Tolerance);
	TestEqual(TEXT("Half as wide at the flat edge"), TechWheelLabelPlacement::GetHexagonHalfWidthAt(
		          OutlineRadius, -OutlineRadius * FlatEdgeScale + Tolerance), OutlineRadius * 0.5f, 0.1f);
	TestEqual(TEXT("Nothing past the flat edge"),
	          TechWheelLabelPlacement::GetHexagonHalfWidthAt(OutlineRadius, OutlineRadius), 0.f, Tolerance);

	TArray<FTechWheelLabelObstacle> Slices;
	TechWheelLabelPlacement::AppendHexagonObstacles(RingCentre, OutlineRadius, Slices);
	TestTrue(TEXT("A hexagon gives slices"), Slices.Num() > 0);
	if (Slices.Num() == 0)
	{
		return true;
	}
	TestEqual(TEXT("The stack starts at the top edge"), static_cast<float>(Slices[0].Rect.Min.Y),
	          static_cast<float>(RingCentre.Y) - OutlineRadius * FlatEdgeScale, Tolerance);
	TestEqual(TEXT("The stack ends at the bottom edge"), static_cast<float>(Slices.Last().Rect.Max.Y),
	          static_cast<float>(RingCentre.Y) + OutlineRadius * FlatEdgeScale, Tolerance);
	// The bounding box's corner is empty: a label there is not charged.
	const FTechWheelLabelRect BoxCorner = MakeRect(
		RingCentre + FVector2D(OutlineRadius * 0.9f, OutlineRadius * 0.7f),
		RingCentre + FVector2D(OutlineRadius, OutlineRadius * FlatEdgeScale));
	TestEqual(TEXT("The box corner is free"), GetCoveredArea(BoxCorner, Slices), 0.f, Tolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTechWheelLabelSidesTest, "RTS.WorldCampaign.UI.TechWheel.LabelSides",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTechWheelLabelSidesTest::RunTest(const FString& Parameters)
{
	using namespace TechWheelLabelPlacementTestData;
	const FTechWheelLabelRequest Request = MakeRequest(RingCentre);
	for (int32 SideIndex = 0; SideIndex < static_cast<int32>(ETechWheelLabelSide::Count); ++SideIndex)
	{
		const FTechWheelLabelRect Rect = TechWheelLabelPlacement::GetCandidateRect(
			Request, static_cast<ETechWheelLabelSide>(SideIndex));
		TestTrue(*FString::Printf(TEXT("Side %d keeps the label's size"), SideIndex),
		         Rect.GetSize().Equals(LabelSize, Tolerance));
		TestTrue(*FString::Printf(TEXT("Side %d is clear of the node's own outline"), SideIndex),
		         GetIsClearOfHexagon(Rect, RingCentre, OutlineRadius));
	}

	const FTechWheelLabelRect Below = TechWheelLabelPlacement::GetCandidateRect(Request, ETechWheelLabelSide::Below);
	TestEqual(TEXT("Below is centred under the node"), static_cast<float>(Below.GetCentre().X),
	          static_cast<float>(RingCentre.X), Tolerance);
	TestTrue(TEXT("Below is under the node"), Below.Min.Y > RingCentre.Y);
	const FTechWheelLabelRect Left = TechWheelLabelPlacement::GetCandidateRect(Request, ETechWheelLabelSide::Left);
	TestTrue(TEXT("Left is left of the outline"), Left.Max.X < RingCentre.X - OutlineRadius);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTechWheelLabelPlaceTest, "RTS.WorldCampaign.UI.TechWheel.PlaceLabels",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTechWheelLabelPlaceTest::RunTest(const FString& Parameters)
{
	using namespace TechWheelLabelPlacementTestData;
	const FTechWheelLabelRing Ring = MakeRing();
	TArray<FTechWheelLabelPlacementResult> Placements;

	// A node with nothing around it keeps its name below.
	TArray<FTechWheelLabelRequest> Requests = {MakeRequest(RingCentre)};
	TArray<FTechWheelLabelObstacle> Obstacles;
	TechWheelLabelPlacement::AppendHexagonObstacles(RingCentre, OutlineRadius, Obstacles);
	TechWheelLabelPlacement::PlaceLabels(Requests, Obstacles, Ring, Placements);
	TestEqual(TEXT("One placement per request"), Placements.Num(), 1);
	if (Placements.Num() != 1)
	{
		return true;
	}
	TestTrue(TEXT("Free below stays below"), Placements[0].Side == ETechWheelLabelSide::Below);

	// Another node right under it takes the place below: the name moves and covers nothing.
	const FVector2D NodeBelowCentre = RingCentre + FVector2D(0.f, OutlineRadius * 2.f);
	TechWheelLabelPlacement::AppendHexagonObstacles(NodeBelowCentre, OutlineRadius, Obstacles);
	TechWheelLabelPlacement::PlaceLabels(Requests, Obstacles, Ring, Placements);
	TestTrue(TEXT("A node below pushes the name away"), Placements[0].Side != ETechWheelLabelSide::Below);
	TestEqual(TEXT("The moved name covers no node"), GetCoveredArea(Placements[0].Rect, Obstacles), 0.f, Tolerance);

	// A node on the bottom of the ring: below would fall off the wheel.
	const FVector2D BottomNodeCentre = RingCentre + FVector2D(0.f, RingRadius - OutlineRadius);
	Requests = {MakeRequest(BottomNodeCentre)};
	Obstacles.Reset();
	TechWheelLabelPlacement::AppendHexagonObstacles(BottomNodeCentre, OutlineRadius, Obstacles);
	TechWheelLabelPlacement::PlaceLabels(Requests, Obstacles, Ring, Placements);
	TestTrue(TEXT("Not below at the bottom of the ring"), Placements[0].Side != ETechWheelLabelSide::Below);
	TestEqual(TEXT("The name stays on the wheel"),
	          TechWheelLabelPlacement::GetAreaOutsideRing(Placements[0].Rect, Ring), 0.f, Tolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTechWheelLabelNeighboursTest, "RTS.WorldCampaign.UI.TechWheel.LabelsOfNeighbours",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTechWheelLabelNeighboursTest::RunTest(const FString& Parameters)
{
	using namespace TechWheelLabelPlacementTestData;
	// Two nodes so close that their names cannot both sit below: the two rectangles below would overlap.
	constexpr float HalfNodeDistance = 40.f;
	const FVector2D FirstCentre = RingCentre + FVector2D(-HalfNodeDistance, 0.f);
	const FVector2D SecondCentre = RingCentre + FVector2D(HalfNodeDistance, 0.f);
	const TArray<FTechWheelLabelRequest> Requests = {MakeRequest(FirstCentre), MakeRequest(SecondCentre)};
	TestTrue(TEXT("The two names below would overlap"), TechWheelLabelPlacement::GetOverlapArea(
		         TechWheelLabelPlacement::GetCandidateRect(Requests[0], ETechWheelLabelSide::Below),
		         TechWheelLabelPlacement::GetCandidateRect(Requests[1], ETechWheelLabelSide::Below)) > 0.f);

	TArray<FTechWheelLabelObstacle> Obstacles;
	TechWheelLabelPlacement::AppendHexagonObstacles(FirstCentre, OutlineRadius, Obstacles);
	TechWheelLabelPlacement::AppendHexagonObstacles(SecondCentre, OutlineRadius, Obstacles);
	TArray<FTechWheelLabelPlacementResult> Placements;
	TechWheelLabelPlacement::PlaceLabels(Requests, Obstacles, MakeRing(), Placements);
	TestEqual(TEXT("Two placements"), Placements.Num(), 2);
	if (Placements.Num() != 2)
	{
		return true;
	}
	TestEqual(TEXT("The names do not overlap"),
	          TechWheelLabelPlacement::GetOverlapArea(Placements[0].Rect, Placements[1].Rect), 0.f, Tolerance);
	TestEqual(TEXT("The first name covers no node"), GetCoveredArea(Placements[0].Rect, Obstacles), 0.f, Tolerance);
	TestEqual(TEXT("The second name covers no node"), GetCoveredArea(Placements[1].Rect, Obstacles), 0.f, Tolerance);
	return true;
}

#endif
