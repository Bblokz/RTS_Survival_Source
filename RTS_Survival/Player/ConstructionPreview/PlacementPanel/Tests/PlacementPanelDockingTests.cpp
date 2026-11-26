// Copyright (C) Bas Blokzijl - All rights reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RTS_Survival/Player/ConstructionPreview/PlacementPanel/PlacementPanelTypes.h"

namespace PlacementPanelDockingTests
{
	// The panel's measured size with four stat rows and three key hints is about 260 x 190 units; test larger.
	const FVector2D PanelSize(300.f, 220.f);
	// Same bands as PlacementPanelLayout in W_PlacementPanel.cpp.
	const FMargin SafeInsets(16.f, 16.f, 320.f, 312.f);
	constexpr float Gap = 28.f;
	// ChooseDock's fallback margin to the screen edge (PlacementPanelDockingPrivate).
	constexpr float ScreenEdgeMargin = 8.f;

	FPlacementPanelDockRequest MakeRequest(const FBox2D& AvoidRect, const FVector2D& ViewportSize,
	                                       const EPlacementPanelDock PreferredDock = EPlacementPanelDock::Right)
	{
		FPlacementPanelDockRequest Request;
		Request.AvoidRect = AvoidRect;
		Request.PanelSize = PanelSize;
		Request.ViewportSize = ViewportSize;
		Request.SafeInsets = SafeInsets;
		Request.Gap = Gap;
		Request.PreferredDock = PreferredDock;
		return Request;
	}

	/** @return Whether some side of the rectangle has room for the panel inside the screen edge margin. */
	bool GetHasRoomBesideRect(const FBox2D& AvoidRect, const FVector2D& ViewportSize, const FMargin& Insets)
	{
		const float RoomRight = ViewportSize.X - Insets.Right - (AvoidRect.Max.X + Gap);
		const float RoomLeft = AvoidRect.Min.X - Gap - Insets.Left;
		const float RoomBelow = ViewportSize.Y - Insets.Bottom - (AvoidRect.Max.Y + Gap);
		const float RoomAbove = AvoidRect.Min.Y - Gap - Insets.Top;
		return RoomRight >= PanelSize.X || RoomLeft >= PanelSize.X || RoomBelow >= PanelSize.Y
			|| RoomAbove >= PanelSize.Y;
	}

	bool GetIsInsideScreen(const FBox2D& PanelRect, const FVector2D& ViewportSize)
	{
		constexpr float Tolerance = 0.01f;
		return PanelRect.Min.X >= ScreenEdgeMargin - Tolerance && PanelRect.Min.Y >= ScreenEdgeMargin - Tolerance
			&& PanelRect.Max.X <= ViewportSize.X - ScreenEdgeMargin + Tolerance
			&& PanelRect.Max.Y <= ViewportSize.Y - ScreenEdgeMargin + Tolerance;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlacementPanelNeverCoversBuildingTest,
                                 "RTS.PlacementPanel.Docking.NeverCoversBuildingWhenThereIsRoom",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlacementPanelNeverCoversBuildingTest::RunTest(const FString& Parameters)
{
	using namespace PlacementPanelDockingTests;
	// Widget space is 1080 units tall on every landscape screen (the DPI curve scales on the shortest side).
	const TArray<FVector2D> Viewports = {FVector2D(1920.f, 1080.f), FVector2D(2560.f, 1080.f), FVector2D(1440.f, 1080.f)};
	const TArray<FVector2D> BuildingSizes = {
		FVector2D(40.f), FVector2D(160.f, 90.f), FVector2D(320.f), FVector2D(700.f, 420.f), FVector2D(1100.f, 700.f)
	};
	constexpr float CentreStep = 37.f;
	int32 CheckedCount = 0;
	int32 UnavoidableCount = 0;
	int32 FailureCount = 0;
	for (const FVector2D& Viewport : Viewports)
	{
		for (const FVector2D& BuildingSize : BuildingSizes)
		{
			for (float CentreX = 0.f; CentreX <= Viewport.X; CentreX += CentreStep)
			{
				for (float CentreY = 0.f; CentreY <= Viewport.Y; CentreY += CentreStep)
				{
					const FVector2D Centre(CentreX, CentreY);
					const FBox2D AvoidRect(Centre - BuildingSize * 0.5f, Centre + BuildingSize * 0.5f);
					const FPlacementPanelDockResult Result = PlacementPanelDocking::ChooseDock(
						MakeRequest(AvoidRect, Viewport));
					const FBox2D PanelRect = PlacementPanelDocking::MakePanelRect(Result.TopLeft, PanelSize);
					const bool bCovers = PlacementPanelDocking::GetDoRectsOverlap(PanelRect, AvoidRect);
					++CheckedCount;
					if (bCovers != Result.bOverlapsAvoidRect)
					{
						++FailureCount;
						AddError(FString::Printf(TEXT("Overlap flag wrong for rect %s on %s"),
						                         *AvoidRect.ToString(), *Viewport.ToString()));
						continue;
					}
					if (not GetIsInsideScreen(PanelRect, Viewport))
					{
						++FailureCount;
						AddError(FString::Printf(TEXT("Panel %s leaves the %s screen (rect %s)"),
						                         *PanelRect.ToString(), *Viewport.ToString(), *AvoidRect.ToString()));
						continue;
					}
					const bool bHasRoom = GetHasRoomBesideRect(AvoidRect, Viewport, FMargin(ScreenEdgeMargin));
					if (bCovers && bHasRoom)
					{
						++FailureCount;
						AddError(FString::Printf(TEXT("Panel %s covers rect %s on %s although there was room"),
						                         *PanelRect.ToString(), *AvoidRect.ToString(), *Viewport.ToString()));
						continue;
					}
					UnavoidableCount += bCovers ? 1 : 0;
				}
			}
		}
	}
	AddInfo(FString::Printf(TEXT("%d placements checked, %d with no room beside the building, %d failures"),
	                        CheckedCount, UnavoidableCount, FailureCount));
	return FailureCount == 0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlacementPanelKeepsHudBandsClearTest,
                                 "RTS.PlacementPanel.Docking.KeepsHudBandsClear",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlacementPanelKeepsHudBandsClearTest::RunTest(const FString& Parameters)
{
	using namespace PlacementPanelDockingTests;
	const FVector2D Viewport(1920.f, 1080.f);
	// A mid-size building around the screen centre has room inside the safe bands: the panel must not use the
	// bottom HUD band.
	for (float CentreX = 500.f; CentreX <= 1300.f; CentreX += 60.f)
	{
		for (float CentreY = 150.f; CentreY <= 650.f; CentreY += 50.f)
		{
			const FVector2D Centre(CentreX, CentreY);
			const FBox2D AvoidRect(Centre - FVector2D(120.f), Centre + FVector2D(120.f));
			if (not GetHasRoomBesideRect(AvoidRect, Viewport, SafeInsets))
			{
				continue;
			}
			const FPlacementPanelDockResult Result = PlacementPanelDocking::ChooseDock(MakeRequest(AvoidRect, Viewport));
			const FBox2D PanelRect = PlacementPanelDocking::MakePanelRect(Result.TopLeft, PanelSize);
			TestFalse(FString::Printf(TEXT("Covers building at %s"), *Centre.ToString()), Result.bOverlapsAvoidRect);
			TestTrue(FString::Printf(TEXT("Stays above the HUD band for a building at %s"), *Centre.ToString()),
			         PanelRect.Max.Y <= Viewport.Y - SafeInsets.Bottom + 0.01f);
			TestTrue(FString::Printf(TEXT("Stays left of the minimap column for a building at %s"), *Centre.ToString()),
			         PanelRect.Max.X <= Viewport.X - SafeInsets.Right + 0.01f);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlacementPanelKeepsItsSideTest,
                                 "RTS.PlacementPanel.Docking.KeepsItsSideWhileValid",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlacementPanelKeepsItsSideTest::RunTest(const FString& Parameters)
{
	using namespace PlacementPanelDockingTests;
	const FVector2D Viewport(1920.f, 1080.f);
	const FBox2D CentredRect(FVector2D(860.f, 340.f), FVector2D(1060.f, 540.f));
	const FPlacementPanelDockResult FirstChoice = PlacementPanelDocking::ChooseDock(MakeRequest(CentredRect, Viewport));
	TestTrue(TEXT("Docks right of a centred building by default"), FirstChoice.Dock == EPlacementPanelDock::Right);

	const FPlacementPanelDockResult LeftKept = PlacementPanelDocking::ChooseDock(
		MakeRequest(CentredRect, Viewport, EPlacementPanelDock::Left));
	TestTrue(TEXT("Stays left while left still fits"), LeftKept.Dock == EPlacementPanelDock::Left);

	// At the right screen edge there is no room on the right: the panel ends up left of the building, either as
	// the left dock or as the right dock clamped out of the minimap column (which already clears it).
	const FBox2D RightEdgeRect(FVector2D(1700.f, 340.f), FVector2D(1880.f, 540.f));
	const FPlacementPanelDockResult AtRightEdge = PlacementPanelDocking::ChooseDock(
		MakeRequest(RightEdgeRect, Viewport));
	const FBox2D RightEdgePanel = PlacementPanelDocking::MakePanelRect(AtRightEdge.TopLeft, PanelSize);
	TestTrue(TEXT("Sits left of a building at the right screen edge"), RightEdgePanel.Max.X <= RightEdgeRect.Min.X);
	TestFalse(TEXT("Panel at the right screen edge clears the building"), AtRightEdge.bOverlapsAvoidRect);
	return true;
}

#endif
