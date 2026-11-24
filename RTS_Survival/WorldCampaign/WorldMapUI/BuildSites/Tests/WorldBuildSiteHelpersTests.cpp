// Copyright (C) Bas Blokzijl - All rights reserved.

#include "Misc/Build.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Misc/AutomationTest.h"
#include "RTS_Survival/WorldCampaign/WorldBuildNodes/WorldBuildingSettings.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/BuildSites/WorldBuildSiteHelpers.h"

namespace WorldBuildSiteTestData
{
	constexpr float Tolerance = 0.001f;
	const TArray<float> UnlockThresholds = {0.5f, 1.f};

	FWorldMapResources MakeResources(const int32 Radixite, const int32 Blueprints)
	{
		FWorldMapResources Resources;
		Resources.Radixite = Radixite;
		Resources.Blueprints = Blueprints;
		return Resources;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldBuildSiteUnlockTextTest, "RTS.WorldCampaign.UI.BuildSites.UnlockLiberation",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldBuildSiteUnlockTextTest::RunTest(const FString& Parameters)
{
	using namespace WorldBuildSiteTestData;
	TestEqual(TEXT("First node opens with the first cleared target"),
	          WorldBuildSiteHelpers::GetUnlockLiberation(0, UnlockThresholds), 0.f, Tolerance);
	TestEqual(TEXT("Second node at 50 %"), WorldBuildSiteHelpers::GetUnlockLiberation(1, UnlockThresholds), 0.5f,
	          Tolerance);
	TestEqual(TEXT("Third node at 100 %"), WorldBuildSiteHelpers::GetUnlockLiberation(2, UnlockThresholds), 1.f,
	          Tolerance);
	TestEqual(TEXT("Past the thresholds only a free country opens it"),
	          WorldBuildSiteHelpers::GetUnlockLiberation(5, UnlockThresholds), 1.f, Tolerance);
	// The rules stop at the first threshold not reached, so a lower later threshold does not open a node earlier.
	TestEqual(TEXT("Unsorted thresholds"),
	          WorldBuildSiteHelpers::GetUnlockLiberation(2, TArray<float>{0.7f, 0.4f}), 0.7f, Tolerance);

	const FString LockedText = WorldBuildSiteHelpers::GetLockedReasonText(1, UnlockThresholds, 0.2f).ToString();
	TestTrue(TEXT("Names the needed percentage"), LockedText.Contains(TEXT("50 %")));
	TestTrue(TEXT("Names the current percentage"), LockedText.Contains(TEXT("20 %")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldBuildSiteEffectTextTest, "RTS.WorldCampaign.UI.BuildSites.EffectText",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldBuildSiteEffectTextTest::RunTest(const FString& Parameters)
{
	using namespace WorldBuildSiteTestData;
	const TArray<float> SupportRadii = {4500.f, 3000.f};

	FWorldBuildingEffectDefinition Depot;
	Depot.bPaysSupplyDepotIncome = true;
	TestEqual(TEXT("Depot income from the rules"),
	          WorldBuildSiteHelpers::GetBuildingEffectText(Depot, MakeResources(60, 1), SupportRadii).ToString(),
	          FString(TEXT("+60 radixite and +1 blueprint each turn")));
	TestEqual(TEXT("Depot without blueprints"),
	          WorldBuildSiteHelpers::GetBuildingEffectText(Depot, MakeResources(40, 0), SupportRadii).ToString(),
	          FString(TEXT("+40 radixite each turn")));

	FWorldBuildingEffectDefinition Airfield;
	Airfield.EnemyStrengthReductionRadius = 4500.f;
	Airfield.EnemyStrengthReductionPercent = 15;
	TestEqual(TEXT("Wide support"),
	          WorldBuildSiteHelpers::GetBuildingEffectText(Airfield, MakeResources(0, 0), SupportRadii).ToString(),
	          FString(TEXT("-15 % enemy strength, long range")));

	FWorldBuildingEffectDefinition Battery;
	Battery.EnemyStrengthReductionRadius = 3000.f;
	Battery.EnemyStrengthReductionPercent = 20;
	TestEqual(TEXT("Close support"),
	          WorldBuildSiteHelpers::GetBuildingEffectText(Battery, MakeResources(0, 0), SupportRadii).ToString(),
	          FString(TEXT("-20 % enemy strength, short range")));
	TestTrue(TEXT("Nothing to compare with"),
	         WorldBuildSiteHelpers::GetSupportRangeWord(3000.f, TArray<float>{3000.f}) == EWorldBuildSiteRangeWord::None);

	FWorldBuildingEffectDefinition Unknown;
	Unknown.Description = FText::FromString(TEXT("Does something new."));
	TestEqual(TEXT("Falls back to the description"),
	          WorldBuildSiteHelpers::GetBuildingEffectText(Unknown, MakeResources(60, 1), SupportRadii).ToString(),
	          FString(TEXT("Does something new.")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldBuildSiteCodeTest, "RTS.WorldCampaign.UI.BuildSites.BuildingCode",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldBuildSiteCodeTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Fixed code"), WorldBuildSiteHelpers::GetBuildingCode(
		          EWorldCampaignBuilding::Airfield, FText::FromString(TEXT("Airfield"))).ToString(), FString(TEXT("AF")));
	TestEqual(TEXT("Initials of a new building"), WorldBuildSiteHelpers::GetBuildingCode(
		          EWorldCampaignBuilding::None, FText::FromString(TEXT("Field hospital"))).ToString(), FString(TEXT("FH")));
	TestEqual(TEXT("One word gives its first two letters"), WorldBuildSiteHelpers::GetBuildingCode(
		          EWorldCampaignBuilding::None, FText::FromString(TEXT("Radar"))).ToString(), FString(TEXT("RA")));
	TestEqual(TEXT("Site title"), WorldBuildSiteHelpers::GetSiteTitle(FText::FromString(TEXT("Germany")), 2).ToString(),
	          FString(TEXT("BUILD SITE \u00B7 GERMANY 02")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldBuildSitePlacementTest, "RTS.WorldCampaign.UI.BuildSites.PanelPlacement",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldBuildSitePlacementTest::RunTest(const FString& Parameters)
{
	using namespace WorldBuildSiteTestData;
	const FVector2D ViewSize(1920.f, 1080.f);
	const FVector2D PanelSize(690.f, 306.f);
	constexpr float Gap = 46.f;
	constexpr float Margin = 12.f;
	constexpr float TopBar = 55.f;

	const FVector2D Centred = WorldBuildSiteHelpers::GetPanelTopLeft(FVector2D(960.f, 600.f), PanelSize, ViewSize, Gap,
	                                                                  Margin, TopBar);
	TestTrue(TEXT("Centred above the site"), Centred.Equals(FVector2D(615.f, 248.f), Tolerance));

	const FVector2D NearTop = WorldBuildSiteHelpers::GetPanelTopLeft(FVector2D(960.f, 100.f), PanelSize, ViewSize, Gap,
	                                                                  Margin, TopBar);
	TestTrue(TEXT("Below the site when there is no room above"), NearTop.Equals(FVector2D(615.f, 146.f), Tolerance));

	// Above the site the panel would start at 28, on screen but under the 55 high bar.
	const FVector2D UnderBar = WorldBuildSiteHelpers::GetPanelTopLeft(FVector2D(960.f, 380.f), PanelSize, ViewSize, Gap,
	                                                                   Margin, TopBar);
	TestTrue(TEXT("Below the site rather than under the top bar"), UnderBar.Equals(FVector2D(615.f, 426.f), Tolerance));

	// A site behind the bar: the panel goes below it and never above the bar's lower edge plus the margin.
	const FVector2D BehindBar = WorldBuildSiteHelpers::GetPanelTopLeft(FVector2D(960.f, 5.f), PanelSize, ViewSize, Gap,
	                                                                    Margin, TopBar);
	TestTrue(TEXT("Never under the top bar"), BehindBar.Equals(FVector2D(615.f, TopBar + Margin), Tolerance));

	const FVector2D NearLeft = WorldBuildSiteHelpers::GetPanelTopLeft(FVector2D(50.f, 600.f), PanelSize, ViewSize, Gap,
	                                                                   Margin, TopBar);
	TestTrue(TEXT("Kept inside the left edge"), FMath::IsNearlyEqual(NearLeft.X, static_cast<double>(Margin)));

	const FVector2D NearRight = WorldBuildSiteHelpers::GetPanelTopLeft(FVector2D(1900.f, 600.f), PanelSize, ViewSize,
	                                                                    Gap, Margin, TopBar);
	TestTrue(TEXT("Kept inside the right edge"),
	         FMath::IsNearlyEqual(NearRight.X, ViewSize.X - PanelSize.X - Margin));

	// A small panel (one built card) is centred on the site as well.
	const FVector2D SmallPanelSize(280.f, 284.f);
	const FVector2D Small = WorldBuildSiteHelpers::GetPanelTopLeft(FVector2D(960.f, 600.f), SmallPanelSize, ViewSize,
	                                                                Gap, Margin, TopBar);
	TestTrue(TEXT("A small panel is centred above the site"), Small.Equals(FVector2D(820.f, 270.f), Tolerance));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldBuildSiteCardRowTest, "RTS.WorldCampaign.UI.BuildSites.CardRowWidth",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldBuildSiteCardRowTest::RunTest(const FString& Parameters)
{
	using namespace WorldBuildSiteTestData;
	constexpr float CardWidth = 154.f;
	constexpr float CardGap = 10.f;
	TestEqual(TEXT("No cards, no width"), WorldBuildSiteHelpers::GetCardRowWidth(0, CardWidth, CardGap), 0.f, Tolerance);
	TestEqual(TEXT("One card has no gap"), WorldBuildSiteHelpers::GetCardRowWidth(1, CardWidth, CardGap), 154.f,
	          Tolerance);
	TestEqual(TEXT("Four cards and three gaps"), WorldBuildSiteHelpers::GetCardRowWidth(4, CardWidth, CardGap), 646.f,
	          Tolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldBuildSiteStemTest, "RTS.WorldCampaign.UI.BuildSites.Stem",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldBuildSiteStemTest::RunTest(const FString& Parameters)
{
	using namespace WorldBuildSiteTestData;
	// The panel spans 100..500 across and 100..300 down.
	const FVector2D PanelTopLeft(100.f, 100.f);
	const FVector2D PanelSize(400.f, 200.f);
	constexpr float CornerInset = 30.f;
	constexpr float Clearance = 10.f;
	constexpr float AngleTolerance = 0.01f;

	const FWorldBuildSiteStem Below = WorldBuildSiteHelpers::GetStemPlacement(
		FVector2D(300.f, 346.f), PanelTopLeft, PanelSize, CornerInset, Clearance);
	TestTrue(TEXT("A site below the panel gets a line"), Below.bIsShown);
	TestTrue(TEXT("The line leaves the lower edge above the site"), Below.Start.Equals(FVector2D(300.f, 300.f), Tolerance));
	TestEqual(TEXT("The line stops at the marker's rim"), Below.Length, 36.f, Tolerance);
	TestEqual(TEXT("The line hangs straight down"), Below.AngleDegrees, 0.f, AngleTolerance);

	const FWorldBuildSiteStem Above = WorldBuildSiteHelpers::GetStemPlacement(
		FVector2D(300.f, 54.f), PanelTopLeft, PanelSize, CornerInset, Clearance);
	TestTrue(TEXT("A site above the panel gets a line"), Above.bIsShown);
	TestTrue(TEXT("The line leaves the upper edge"), Above.Start.Equals(FVector2D(300.f, 100.f), Tolerance));
	TestEqual(TEXT("Same length upward"), Above.Length, 36.f, Tolerance);
	TestEqual(TEXT("The line points straight up"), FMath::Abs(Above.AngleDegrees), 180.f, AngleTolerance);

	// The site lies left of the panel's corner: the line starts at the inset and leans 3-4-5 to the left.
	const FWorldBuildSiteStem Leaning = WorldBuildSiteHelpers::GetStemPlacement(
		FVector2D(100.f, 340.f), PanelTopLeft, PanelSize, CornerInset, Clearance);
	TestTrue(TEXT("A site past the corner still gets a line"), Leaning.bIsShown);
	TestTrue(TEXT("The line starts at the corner inset"), Leaning.Start.Equals(FVector2D(130.f, 300.f), Tolerance));
	TestEqual(TEXT("Length along the lean, less the clearance"), Leaning.Length, 40.f, Tolerance);
	TestEqual(TEXT("Leaning clockwise to reach a site on the left"), Leaning.AngleDegrees, 36.8699f, AngleTolerance);

	TestFalse(TEXT("No line to a site beside the panel"), WorldBuildSiteHelpers::GetStemPlacement(
		          FVector2D(50.f, 200.f), PanelTopLeft, PanelSize, CornerInset, Clearance).bIsShown);
	TestFalse(TEXT("No line to a site closer than the clearance"), WorldBuildSiteHelpers::GetStemPlacement(
		          FVector2D(300.f, 305.f), PanelTopLeft, PanelSize, CornerInset, Clearance).bIsShown);
	return true;
}

#endif
