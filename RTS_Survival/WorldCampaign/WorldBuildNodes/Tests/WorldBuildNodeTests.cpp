// Copyright (C) Bas Blokzijl - All rights reserved.

#include "Misc/Build.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Misc/AutomationTest.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignRules.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignRulesSettings.h"
#include "RTS_Survival/WorldCampaign/WorldBuildNodes/WorldBuildNodeRules.h"
#include "RTS_Survival/WorldCampaign/WorldCountryOccupation/WorldCountryLiberationRules.h"

namespace WorldBuildNodeTestData
{
	const FName NodeA = TEXT("Node_A");
	const FName NodeB = TEXT("Node_B");
	const FName NodeC = TEXT("Node_C");
	const FName BaseColorName = TEXT("BaseColor");
	const FName StripeIntensityName = TEXT("StripeIntensity");
	const FName OnlyOnOccupiedName = TEXT("OnlyOnOccupied");
	const FName OnlyOnLiberatedName = TEXT("OnlyOnLiberated");
	constexpr float HalfLiberation = 0.5f;
	constexpr float BlendTolerance = 0.001f;

	FWorldBuildNodeUnlockEntry MakeEntry(const FName BuildNodeId, const int32 UnlockOrder)
	{
		FWorldBuildNodeUnlockEntry UnlockEntry;
		UnlockEntry.BuildNodeId = BuildNodeId;
		UnlockEntry.UnlockOrder = UnlockOrder;
		return UnlockEntry;
	}

	// Given out of order on purpose: C and B share order 1 and must be ordered by id.
	TArray<FWorldBuildNodeUnlockEntry> MakeCountryNodes()
	{
		return {MakeEntry(NodeC, 1), MakeEntry(NodeA, 0), MakeEntry(NodeB, 1)};
	}

	FWorldMapResources MakeResources(const int32 Radixite, const int32 Blueprints)
	{
		FWorldMapResources Resources;
		Resources.Radixite = Radixite;
		Resources.Blueprints = Blueprints;
		return Resources;
	}

	FWorldCountryAnchorTargetInput MakeTargetInput(const bool bIsTarget, const bool bIsStory, const bool bIsCleared)
	{
		FWorldCountryAnchorTargetInput TargetInput;
		TargetInput.bIsTarget = bIsTarget;
		TargetInput.bIsStoryMission = bIsStory;
		TargetInput.bIsCleared = bIsCleared;
		return TargetInput;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldBuildNodeUnlockOrderTest, "RTS.WorldCampaign.BuildNodes.UnlockOrder",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldBuildNodeUnlockOrderTest::RunTest(const FString& Parameters)
{
	using namespace WorldBuildNodeTestData;
	const TArray<FName> NodesInOrder = WorldBuildNodeRules::GetNodesInUnlockOrder(MakeCountryNodes());
	TestEqual(TEXT("All nodes are ordered"), NodesInOrder.Num(), 3);
	if (NodesInOrder.Num() != 3)
	{
		return false;
	}
	TestEqual(TEXT("Lowest unlock order first"), NodesInOrder[0], NodeA);
	TestEqual(TEXT("Equal order is broken by id (B)"), NodesInOrder[1], NodeB);
	TestEqual(TEXT("Equal order is broken by id (C)"), NodesInOrder[2], NodeC);

	const TArray<FName> TwoUnlocked = WorldBuildNodeRules::GetUnlockedNodeIds(MakeCountryNodes(), 2);
	TestEqual(TEXT("Two nodes unlocked"), TwoUnlocked.Num(), 2);
	TestTrue(TEXT("The first two in order are unlocked"), TwoUnlocked.Contains(NodeA) && TwoUnlocked.Contains(NodeB));
	TestEqual(TEXT("A count above the node count is clamped"),
	          WorldBuildNodeRules::GetUnlockedNodeIds(MakeCountryNodes(), 10).Num(), 3);
	TestEqual(TEXT("A negative count unlocks nothing"),
	          WorldBuildNodeRules::GetUnlockedNodeIds(MakeCountryNodes(), -1).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldBuildNodeUnlockCountTest, "RTS.WorldCampaign.BuildNodes.UnlockFromLiberation",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldBuildNodeUnlockCountTest::RunTest(const FString& Parameters)
{
	// Default numbers: first node at the first cleared target, then at 50 % and 100 %.
	const FWorldCampaignRuleNumbers Numbers;
	constexpr int32 NodesInCountry = 3;
	constexpr float LowLiberation = 0.2f;
	TestEqual(TEXT("Nothing cleared: all locked"),
	          WorldCampaignRules::GetUnlockedBuildNodeCount(Numbers, NodesInCountry, 0, 0.f), 0);
	TestEqual(TEXT("First cleared target opens one node"),
	          WorldCampaignRules::GetUnlockedBuildNodeCount(Numbers, NodesInCountry, 1, LowLiberation), 1);
	TestEqual(TEXT("Half free opens two nodes"),
	          WorldCampaignRules::GetUnlockedBuildNodeCount(Numbers, NodesInCountry, 2, 0.5f), 2);
	TestEqual(TEXT("Fully free opens every node"),
	          WorldCampaignRules::GetUnlockedBuildNodeCount(Numbers, NodesInCountry, 4, 1.f), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldBuildNodeStateTest, "RTS.WorldCampaign.BuildNodes.StateNeverRegresses",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldBuildNodeStateTest::RunTest(const FString& Parameters)
{
	using EState = EWorldBuildNodeState;
	constexpr EWorldCampaignBuilding NoBuilding = EWorldCampaignBuilding::None;
	TestEqual(TEXT("Locked stays locked outside the unlocked set"),
	          WorldBuildNodeRules::GetNodeStateAfterUnlockPass(EState::Locked, false, NoBuilding), EState::Locked);
	TestEqual(TEXT("Locked unlocks inside the unlocked set"),
	          WorldBuildNodeRules::GetNodeStateAfterUnlockPass(EState::Locked, true, NoBuilding), EState::Unlocked);
	TestEqual(TEXT("Unlocked never locks again"),
	          WorldBuildNodeRules::GetNodeStateAfterUnlockPass(EState::Unlocked, false, NoBuilding), EState::Unlocked);
	TestEqual(TEXT("A stored building means Built even when locked"),
	          WorldBuildNodeRules::GetNodeStateAfterUnlockPass(EState::Locked, false, EWorldCampaignBuilding::Airfield),
	          EState::Built);
	TestEqual(TEXT("Built stays built"),
	          WorldBuildNodeRules::GetNodeStateAfterUnlockPass(EState::Built, false, NoBuilding), EState::Built);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldBuildNodeBuildCheckTest, "RTS.WorldCampaign.BuildNodes.BuildChecks",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldBuildNodeBuildCheckTest::RunTest(const FString& Parameters)
{
	using namespace WorldBuildNodeTestData;
	using EResult = EWorldBuildNodeBuildResult;
	using EState = EWorldBuildNodeState;
	constexpr EWorldCampaignBuilding NoBuilding = EWorldCampaignBuilding::None;
	constexpr EWorldCampaignBuilding Depot = EWorldCampaignBuilding::SupplyDepot;
	const FWorldMapResources Cost = MakeResources(150, 1);
	const FWorldMapResources Rich = MakeResources(500, 5);
	const FWorldMapResources Poor = MakeResources(100, 5);

	TestEqual(TEXT("No building chosen"),
	          WorldBuildNodeRules::GetBuildResult(EState::Unlocked, NoBuilding, NoBuilding, &Cost, Rich),
	          EResult::InvalidBuilding);
	TestEqual(TEXT("Locked node"),
	          WorldBuildNodeRules::GetBuildResult(EState::Locked, NoBuilding, Depot, &Cost, Rich), EResult::NodeLocked);
	TestEqual(TEXT("Built node"),
	          WorldBuildNodeRules::GetBuildResult(EState::Built, NoBuilding, Depot, &Cost, Rich), EResult::NodeOccupied);
	TestEqual(TEXT("A saved building occupies the node"),
	          WorldBuildNodeRules::GetBuildResult(EState::Unlocked, EWorldCampaignBuilding::WatchPost, Depot, &Cost,
	                                              Rich), EResult::NodeOccupied);
	TestEqual(TEXT("A building without a price is not free"),
	          WorldBuildNodeRules::GetBuildResult(EState::Unlocked, NoBuilding, Depot, nullptr, Rich),
	          EResult::NoPriceConfigured);
	TestEqual(TEXT("Too little radixite"),
	          WorldBuildNodeRules::GetBuildResult(EState::Unlocked, NoBuilding, Depot, &Cost, Poor),
	          EResult::CannotAfford);
	TestEqual(TEXT("Affordable on an unlocked empty node"),
	          WorldBuildNodeRules::GetBuildResult(EState::Unlocked, NoBuilding, Depot, &Cost, Rich), EResult::Success);
	TestTrue(TEXT("Every refusal has a reason text"),
	         not WorldBuildNodeRules::GetBuildResultText(EResult::CannotAfford).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldBuildNodeOptionsAndIncomeTest, "RTS.WorldCampaign.BuildNodes.OptionsAndIncome",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldBuildNodeOptionsAndIncomeTest::RunTest(const FString& Parameters)
{
	using namespace WorldBuildNodeTestData;
	TMap<EWorldCampaignBuilding, FWorldMapResources> BuildingCosts;
	BuildingCosts.Add(EWorldCampaignBuilding::Battery, MakeResources(250, 1));
	BuildingCosts.Add(EWorldCampaignBuilding::None, MakeResources(1, 0));
	BuildingCosts.Add(EWorldCampaignBuilding::WatchPost, MakeResources(100, 0));
	const TArray<EWorldCampaignBuilding> BuildOptions = WorldBuildNodeRules::GetBuildOptions(BuildingCosts);
	TestEqual(TEXT("None is never an option"), BuildOptions.Num(), 2);
	if (BuildOptions.Num() == 2)
	{
		TestEqual(TEXT("Options follow enum order (watch post)"), BuildOptions[0], EWorldCampaignBuilding::WatchPost);
		TestEqual(TEXT("Options follow enum order (battery)"), BuildOptions[1], EWorldCampaignBuilding::Battery);
	}

	const FWorldMapResources TurnIncome = WorldBuildNodeRules::GetTurnIncome(MakeResources(60, 1), 3);
	TestEqual(TEXT("Three depots pay three times the radixite"), TurnIncome.Radixite, 180);
	TestEqual(TEXT("Three depots pay three times the blueprints"), TurnIncome.Blueprints, 3);
	TestEqual(TEXT("No depots pay nothing"), WorldBuildNodeRules::GetTurnIncome(MakeResources(60, 1), -2).Radixite, 0);

	TestEqual(TEXT("Player support lowers strength"), WorldBuildNodeRules::GetPlayerSupportStrengthPercent(15), -15);
	TestEqual(TEXT("A negative setting still lowers strength"),
	          WorldBuildNodeRules::GetPlayerSupportStrengthPercent(-15), -15);
	TestTrue(TEXT("Inside the radius"),
	         WorldBuildNodeRules::GetIsWithinSupportRadiusXY(FVector::ZeroVector, FVector(300.f, 400.f, 9000.f), 500.f));
	TestFalse(TEXT("Outside the radius"),
	          WorldBuildNodeRules::GetIsWithinSupportRadiusXY(FVector::ZeroVector, FVector(300.f, 401.f, 0.f), 500.f));

	TestEqual(TEXT("Source keys are stable"), WorldBuildNodeRules::GetBuildNodeSourceKey(NodeA),
	          WorldBuildNodeRules::GetBuildNodeSourceKey(NodeA));
	TestNotEqual(TEXT("Source keys differ per node"), WorldBuildNodeRules::GetBuildNodeSourceKey(NodeA),
	             WorldBuildNodeRules::GetBuildNodeSourceKey(NodeB));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCountryTargetTallyTest, "RTS.WorldCampaign.Countries.TargetTally",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldCountryTargetTallyTest::RunTest(const FString& Parameters)
{
	using namespace WorldBuildNodeTestData;
	const TArray<FWorldCountryAnchorTargetInput> AnchorInputs = {
		MakeTargetInput(true, true, true),
		MakeTargetInput(true, false, false),
		MakeTargetInput(true, false, true),
		MakeTargetInput(false, false, false)
	};
	const FWorldCountryTargetTally TargetTally = WorldCountryLiberationRules::TallyCountryTargets(AnchorInputs);
	TestEqual(TEXT("One story mission"), TargetTally.StoryMissionsTotal, 1);
	TestEqual(TEXT("Story mission cleared"), TargetTally.StoryMissionsCleared, 1);
	TestEqual(TEXT("Non-targets are ignored"), TargetTally.OtherTargetsTotal, 2);
	TestEqual(TEXT("One other target cleared"), TargetTally.OtherTargetsCleared, 1);

	// Story weight 2: (1 * 2 + 1) / (1 * 2 + 2) = 0.75.
	FWorldCampaignRuleNumbers Numbers;
	Numbers.StoryMissionLiberationWeight = 2;
	const float Liberation = WorldCampaignRules::GetCountryLiberation(
		Numbers, TargetTally.StoryMissionsTotal, TargetTally.StoryMissionsCleared, TargetTally.OtherTargetsTotal,
		TargetTally.OtherTargetsCleared);
	TestEqual(TEXT("Story missions weigh double"), Liberation, 0.75f, BlendTolerance);
	TestEqual(TEXT("A country without targets is free"),
	          WorldCampaignRules::GetCountryLiberation(Numbers, 0, 0, 0, 0), 1.f, BlendTolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCountryPointInMeshTest, "RTS.WorldCampaign.Countries.PointInMesh",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldCountryPointInMeshTest::RunTest(const FString& Parameters)
{
	// An L-shaped country of two triangles plus a far overseas triangle: its bounds would hold the empty corner.
	TArray<FWorldCountryTriangle2D> CountryTriangles;
	FWorldCountryTriangle2D Triangle;
	TestTrue(TEXT("Clockwise triangle accepted"), WorldCountryLiberationRules::TryMakeCountryTriangle2D(
		         FVector2D(0.0, 0.0), FVector2D(0.0, 100.0), FVector2D(100.0, 0.0), Triangle));
	CountryTriangles.Add(Triangle);
	TestTrue(TEXT("Counter-clockwise triangle accepted"), WorldCountryLiberationRules::TryMakeCountryTriangle2D(
		         FVector2D(1000.0, 1000.0), FVector2D(1100.0, 1000.0), FVector2D(1000.0, 1100.0), Triangle));
	CountryTriangles.Add(Triangle);
	TestFalse(TEXT("Degenerate triangle refused"), WorldCountryLiberationRules::TryMakeCountryTriangle2D(
		          FVector2D(0.0, 0.0), FVector2D(50.0, 50.0), FVector2D(100.0, 100.0), Triangle));

	TestTrue(TEXT("Inside the first triangle"),
	         WorldCountryLiberationRules::GetIsPointInCountryTriangles(FVector2D(10.0, 10.0), CountryTriangles));
	TestTrue(TEXT("Inside the overseas triangle"),
	         WorldCountryLiberationRules::GetIsPointInCountryTriangles(FVector2D(1010.0, 1010.0), CountryTriangles));
	TestTrue(TEXT("On an edge counts as inside"),
	         WorldCountryLiberationRules::GetIsPointInCountryTriangles(FVector2D(50.0, 0.0), CountryTriangles));
	TestFalse(TEXT("Inside the bounds but outside every triangle"),
	          WorldCountryLiberationRules::GetIsPointInCountryTriangles(FVector2D(500.0, 500.0), CountryTriangles));
	TestFalse(TEXT("Across the hypotenuse is outside"),
	          WorldCountryLiberationRules::GetIsPointInCountryTriangles(FVector2D(60.0, 60.0), CountryTriangles));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCountryLookBlendTest, "RTS.WorldCampaign.Countries.LookBlend",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldCountryLookBlendTest::RunTest(const FString& Parameters)
{
	using namespace WorldBuildNodeTestData;
	FWorldCountryMaterialParameterSet OccupiedSet;
	OccupiedSet.VectorValues.Add(BaseColorName, FLinearColor(1.f, 0.f, 0.f, 1.f));
	OccupiedSet.ScalarValues.Add(StripeIntensityName, 1.f);
	OccupiedSet.ScalarValues.Add(OnlyOnOccupiedName, 5.f);

	FWorldCountryMaterialParameterSet LiberatedSet;
	LiberatedSet.VectorValues.Add(BaseColorName, FLinearColor(0.f, 0.f, 1.f, 1.f));
	LiberatedSet.VectorValues.Add(OnlyOnLiberatedName, FLinearColor::White);
	LiberatedSet.ScalarValues.Add(StripeIntensityName, 0.f);

	const FWorldCountryMaterialParameterSet HalfSet =
		WorldCountryLiberationRules::BlendParameterSets(OccupiedSet, LiberatedSet, HalfLiberation);
	TestEqual(TEXT("Only shared vectors are blended"), HalfSet.VectorValues.Num(), 1);
	TestEqual(TEXT("Only shared scalars are blended"), HalfSet.ScalarValues.Num(), 1);
	const FLinearColor* HalfColor = HalfSet.VectorValues.Find(BaseColorName);
	const float* HalfStripes = HalfSet.ScalarValues.Find(StripeIntensityName);
	if (HalfColor == nullptr || HalfStripes == nullptr)
	{
		AddError(TEXT("Blended parameters are missing."));
		return false;
	}
	TestEqual(TEXT("Half-way red"), HalfColor->R, 0.5f, BlendTolerance);
	TestEqual(TEXT("Half-way blue"), HalfColor->B, 0.5f, BlendTolerance);
	TestEqual(TEXT("Stripes fade with liberation"), *HalfStripes, 0.5f, BlendTolerance);

	const FWorldCountryMaterialParameterSet OverSet =
		WorldCountryLiberationRules::BlendParameterSets(OccupiedSet, LiberatedSet, 2.f);
	const FLinearColor* OverColor = OverSet.VectorValues.Find(BaseColorName);
	TestTrue(TEXT("Liberation above 1 is clamped to the liberated look"),
	         OverColor != nullptr && FMath::IsNearlyEqual(OverColor->B, 1.f, BlendTolerance)
	         && FMath::IsNearlyEqual(OverColor->R, 0.f, BlendTolerance));
	return true;
}

#endif
