// Copyright (C) Bas Blokzijl - All rights reserved.

#include "Misc/Build.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Misc/AutomationTest.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignProgressState.h"
#include "RTS_Survival/WorldCampaign/EnemyTurn/WorldEnemyTurnPlanning.h"

namespace WorldEnemyTurnPlanningTestData
{
	constexpr int32 ConvoyCost = 4;
	constexpr int32 DivisionCost = 8;
	constexpr int32 MaxPieces = 3;
	constexpr int32 CampaignSeed = 1234;

	FWorldEnemyTurnPlace MakePlace(const uint32 KeyPart, const float X, const float Y)
	{
		FWorldEnemyTurnPlace Place;
		Place.Key = FGuid(KeyPart, 0, 0, 1);
		Place.Location = FVector2D(X, Y);
		return Place;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldEnemyTurnPurchaseTest, "RTS.WorldCampaign.EnemyTurn.Purchase",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldEnemyTurnPurchaseTest::RunTest(const FString& Parameters)
{
	namespace TestData = WorldEnemyTurnPlanningTestData;

	const FWorldEnemyReactionPurchase Nothing = WorldEnemyTurnPlanning::PlanReactionPurchase(
		3, TestData::ConvoyCost, TestData::DivisionCost, TestData::MaxPieces);
	TestEqual(TEXT("3 points buy nothing"), Nothing.GetPieceCount(), 0);
	TestEqual(TEXT("3 points are all left over"), Nothing.PointsLeft, 3);

	const FWorldEnemyReactionPurchase OneConvoy = WorldEnemyTurnPlanning::PlanReactionPurchase(
		5, TestData::ConvoyCost, TestData::DivisionCost, TestData::MaxPieces);
	TestEqual(TEXT("5 points buy one convoy"), OneConvoy.ConvoyCount, 1);
	TestEqual(TEXT("5 points buy no division"), OneConvoy.DivisionCount, 0);
	TestEqual(TEXT("5 points leave 1"), OneConvoy.PointsLeft, 1);

	const FWorldEnemyReactionPurchase Mixed = WorldEnemyTurnPlanning::PlanReactionPurchase(
		13, TestData::ConvoyCost, TestData::DivisionCost, TestData::MaxPieces);
	TestEqual(TEXT("13 points buy a division first"), Mixed.DivisionCount, 1);
	TestEqual(TEXT("13 points then buy a convoy"), Mixed.ConvoyCount, 1);
	TestEqual(TEXT("13 points spend 12"), Mixed.PointsSpent, 12);
	TestEqual(TEXT("13 points leave 1"), Mixed.PointsLeft, 1);

	const FWorldEnemyReactionPurchase Capped = WorldEnemyTurnPlanning::PlanReactionPurchase(
		100, TestData::ConvoyCost, TestData::DivisionCost, TestData::MaxPieces);
	TestEqual(TEXT("The piece cap holds"), Capped.GetPieceCount(), TestData::MaxPieces);
	TestEqual(TEXT("Capped points are left over"), Capped.PointsLeft, 100 - TestData::MaxPieces * TestData::DivisionCost);

	const FWorldEnemyReactionPurchase Negative = WorldEnemyTurnPlanning::PlanReactionPurchase(
		-5, TestData::ConvoyCost, TestData::DivisionCost, TestData::MaxPieces);
	TestEqual(TEXT("Negative points buy nothing"), Negative.GetPieceCount(), 0);
	TestEqual(TEXT("Negative points leave nothing"), Negative.PointsLeft, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldEnemyTurnBankingTest, "RTS.WorldCampaign.EnemyTurn.Banking",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldEnemyTurnBankingTest::RunTest(const FString& Parameters)
{
	namespace TestData = WorldEnemyTurnPlanningTestData;

	FWorldEnemyReactionPurchase Purchase = WorldEnemyTurnPlanning::PlanReactionPurchase(
		3, TestData::ConvoyCost, TestData::DivisionCost, TestData::MaxPieces);
	TestEqual(TEXT("Leftover points are banked"), WorldEnemyTurnPlanning::GetBankedPointsAfterPurchase(Purchase, 24),
	          3);

	Purchase.PointsLeft = 40;
	TestEqual(TEXT("The bank is capped"), WorldEnemyTurnPlanning::GetBankedPointsAfterPurchase(Purchase, 24), 24);
	TestEqual(TEXT("A zero cap banks nothing"), WorldEnemyTurnPlanning::GetBankedPointsAfterPurchase(Purchase, 0), 0);

	// Banked points join the next turn's share: 3 banked + 5 new = 8 buys a division.
	const FWorldEnemyReactionPurchase NextTurn = WorldEnemyTurnPlanning::PlanReactionPurchase(
		3 + 5, TestData::ConvoyCost, TestData::DivisionCost, TestData::MaxPieces);
	TestEqual(TEXT("Banked plus new points buy a division"), NextTurn.DivisionCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldEnemyTurnTargetSelectionTest, "RTS.WorldCampaign.EnemyTurn.TargetSelection",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldEnemyTurnTargetSelectionTest::RunTest(const FString& Parameters)
{
	namespace TestData = WorldEnemyTurnPlanningTestData;

	const TArray<FWorldEnemyTurnPlace> Candidates = {
		TestData::MakePlace(1, 10000.f, 0.f),
		TestData::MakePlace(2, 1000.f, 0.f),
		TestData::MakePlace(3, 0.f, 5000.f),
		TestData::MakePlace(4, -1000.f, 0.f)
	};
	const TArray<FVector2D> StruckAtOrigin = {FVector2D::ZeroVector};

	const TArray<int32> TwoNearest = WorldEnemyTurnPlanning::SelectReactionTargetIndices(StruckAtOrigin, Candidates, 2);
	TestEqual(TEXT("Two targets are picked"), TwoNearest.Num(), 2);
	if (TwoNearest.Num() == 2)
	{
		// Candidates 2 and 4 are equally near; the smaller key wins the tie.
		TestEqual(TEXT("Nearest first, tie broken by key"), TwoNearest[0], 1);
		TestEqual(TEXT("The tied candidate comes second"), TwoNearest[1], 3);
	}

	const TArray<FVector2D> TwoStrikes = {FVector2D(10000.f, 100.f), FVector2D(0.f, 4900.f)};
	const TArray<int32> NearStrikes = WorldEnemyTurnPlanning::SelectReactionTargetIndices(TwoStrikes, Candidates, 2);
	TestTrue(TEXT("Each strike pulls its nearest target"),
	         NearStrikes.Num() == 2 && NearStrikes.Contains(0) && NearStrikes.Contains(2));

	TestEqual(TEXT("No strike locations give no targets"),
	          WorldEnemyTurnPlanning::SelectReactionTargetIndices(TArray<FVector2D>(), Candidates, 2).Num(), 0);
	TestEqual(TEXT("Nearest source"), WorldEnemyTurnPlanning::SelectNearestPlaceIndex(FVector2D(9000.f, 0.f),
		          Candidates), 0);
	TestEqual(TEXT("No sources"), WorldEnemyTurnPlanning::SelectNearestPlaceIndex(FVector2D::ZeroVector,
		          TArray<FWorldEnemyTurnPlace>()), static_cast<int32>(INDEX_NONE));

	TestEqual(TEXT("Pieces are dealt round-robin"), WorldEnemyTurnPlanning::GetTargetIndexForPiece(3, 2), 1);
	TestEqual(TEXT("No targets give no slot"), WorldEnemyTurnPlanning::GetTargetIndexForPiece(0, 0),
	          static_cast<int32>(INDEX_NONE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldEnemyTurnDeterminismTest, "RTS.WorldCampaign.EnemyTurn.Determinism",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldEnemyTurnDeterminismTest::RunTest(const FString& Parameters)
{
	namespace TestData = WorldEnemyTurnPlanningTestData;

	const int32 TurnSeed = WorldEnemyTurnPlanning::BuildEnemyTurnSeed(TestData::CampaignSeed, 3);
	TestEqual(TEXT("The turn seed is stable"), WorldEnemyTurnPlanning::BuildEnemyTurnSeed(TestData::CampaignSeed, 3), TurnSeed);

	FWorldEnemyReactionPurchase Purchase;
	Purchase.DivisionCount = 4;
	Purchase.ConvoyCount = 2;
	const TArray<EWorldFieldDivisions> DivisionTypes = {
		EWorldFieldDivisions::SovietLightArmorDivision,
		EWorldFieldDivisions::SovietHeavyArmorDivision
	};
	const TArray<EWorldFieldDivisions> FirstPlan = WorldEnemyTurnPlanning::PlanPieceTypes(
		Purchase, DivisionTypes, TurnSeed);
	const TArray<EWorldFieldDivisions> SecondPlan = WorldEnemyTurnPlanning::PlanPieceTypes(
		Purchase, DivisionTypes, TurnSeed);
	TestTrue(TEXT("Equal seeds give equal piece types"), FirstPlan == SecondPlan);
	TestEqual(TEXT("One type per piece"), FirstPlan.Num(), 6);
	if (FirstPlan.Num() == 6)
	{
		TestTrue(TEXT("Divisions come first"), DivisionTypes.Contains(FirstPlan[0]));
		TestEqual(TEXT("Convoys come last"), FirstPlan[5], EWorldFieldDivisions::SovietSupplyConvoy);
	}

	const FGuid PieceKey = WorldEnemyTurnPlanning::BuildReactionPieceKey(TestData::CampaignSeed, 3, 1);
	TestTrue(TEXT("Piece keys are valid"), PieceKey.IsValid());
	TestEqual(TEXT("Piece keys are stable"), WorldEnemyTurnPlanning::BuildReactionPieceKey(TestData::CampaignSeed, 3, 1),
	          PieceKey);
	TestNotEqual(TEXT("Piece keys differ per piece"), WorldEnemyTurnPlanning::BuildReactionPieceKey(TestData::CampaignSeed, 3, 2),
	             PieceKey);
	TestNotEqual(TEXT("Piece keys differ per turn"), WorldEnemyTurnPlanning::BuildReactionPieceKey(TestData::CampaignSeed, 4, 1),
	             PieceKey);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldEnemyTurnStrikeDetectionTest, "RTS.WorldCampaign.EnemyTurn.StrikeDetection",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldEnemyTurnStrikeDetectionTest::RunTest(const FString& Parameters)
{
	FWorldEnemyTurnState State;
	State.MissionsCompletedAtLastEnemyTurn = 2;
	TestFalse(TEXT("No new win and no records is a quiet turn"),
	          WorldEnemyTurnPlanning::GetHasPlayerStruckSinceLastEnemyTurn(State, 2));
	TestTrue(TEXT("A win on a division target (no anchor) counts as a strike"),
	         WorldEnemyTurnPlanning::GetHasPlayerStruckSinceLastEnemyTurn(State, 3));

	State.RecentlyStruckAnchorKeys.Add(FGuid(1, 2, 3, 4));
	TestTrue(TEXT("A struck anchor counts as a strike"),
	         WorldEnemyTurnPlanning::GetHasPlayerStruckSinceLastEnemyTurn(State, 2));

	State.RecentlyStruckAnchorKeys.Reset();
	State.RecentlyStruckLocations.Add(FVector::ZeroVector);
	TestTrue(TEXT("A destroyed piece counts as a strike"),
	         WorldEnemyTurnPlanning::GetHasPlayerStruckSinceLastEnemyTurn(State, 2));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldEnemyTurnConvoyDeliveryTest, "RTS.WorldCampaign.EnemyTurn.ConvoyDelivery",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldEnemyTurnConvoyDeliveryTest::RunTest(const FString& Parameters)
{
	TArray<EWorldFortificationStrength> Existing;
	const EWorldFortificationStrength First = WorldEnemyTurnPlanning::SelectConvoyDeliveryModifier(Existing, 7);
	TestNotEqual(TEXT("An unfortified target gets a modifier"), First, EWorldFortificationStrength::None);
	TestEqual(TEXT("The choice is stable"), WorldEnemyTurnPlanning::SelectConvoyDeliveryModifier(Existing, 7), First);

	Existing.Add(First);
	const EWorldFortificationStrength Second = WorldEnemyTurnPlanning::SelectConvoyDeliveryModifier(Existing, 7);
	TestTrue(TEXT("A second delivery adds a different modifier"),
	         Second != EWorldFortificationStrength::None && Second != First);

	Existing = {
		EWorldFortificationStrength::Bunkers, EWorldFortificationStrength::Minefields,
		EWorldFortificationStrength::AABattery
	};
	TestEqual(TEXT("A fully fortified target gets none (base strength is raised instead)"),
	          WorldEnemyTurnPlanning::SelectConvoyDeliveryModifier(Existing, 7), EWorldFortificationStrength::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldEnemyTurnSpawnDirectionTest, "RTS.WorldCampaign.EnemyTurn.SpawnDirection",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldEnemyTurnSpawnDirectionTest::RunTest(const FString& Parameters)
{
	constexpr float Tolerance = 0.001f;
	const FVector2D Straight = WorldEnemyTurnPlanning::BuildSpawnDirection(FVector2D::ZeroVector,
	                                                                       FVector2D(100.f, 0.f), 0);
	TestTrue(TEXT("The first piece faces its target"), Straight.Equals(FVector2D(1.f, 0.f), Tolerance));

	const FVector2D Fanned = WorldEnemyTurnPlanning::BuildSpawnDirection(FVector2D::ZeroVector,
	                                                                     FVector2D(100.f, 0.f), 1);
	TestTrue(TEXT("Later pieces fan out"), not Fanned.Equals(Straight, Tolerance));
	TestTrue(TEXT("Directions stay unit length"), FMath::IsNearlyEqual(Fanned.Size(), 1.0, 0.001));

	const FVector2D SamePlace = WorldEnemyTurnPlanning::BuildSpawnDirection(FVector2D::ZeroVector,
	                                                                        FVector2D::ZeroVector, 0);
	TestTrue(TEXT("Source on target still gives a direction"), FMath::IsNearlyEqual(SamePlace.Size(), 1.0, 0.001));
	return true;
}

#endif
