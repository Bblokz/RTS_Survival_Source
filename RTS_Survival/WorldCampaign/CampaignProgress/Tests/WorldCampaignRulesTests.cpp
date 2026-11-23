// Copyright (C) Bas Blokzijl - All rights reserved.

#include "Misc/Build.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Misc/AutomationTest.h"
#include "RTS_Survival/Game/RTSGameInstance/WorldMissionContext/WorldMissionContext.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignProgressManager.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignRules.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/MapObjects/W_MissionReward/RewardStructs/FMissionRewardStructs.h"

namespace WorldCampaignRulesTestData
{
	// Fixed numbers so the tests do not move when the project's config is retuned.
	FWorldCampaignRuleNumbers MakeNumbers()
	{
		FWorldCampaignRuleNumbers Numbers;
		Numbers.MissionsNeededPerTierAboveOne = {2, 5, 9};
		Numbers.MissionsPerRank = 1;
		Numbers.EnemyBudgetPerTurn = 10;
		Numbers.AlertGainMissionWon = 0.25f;
		Numbers.AlertGainEnemyItemDestroyed = 0.15f;
		Numbers.AlertGainMaxExtraNearWall = 0.2f;
		Numbers.AlertDecayPerQuietTurn = 0.1f;
		Numbers.FreezePointsPerStage = {30, 40};
		Numbers.FreezeFinalStage = 3;
		Numbers.StoryMissionLiberationWeight = 2;
		Numbers.BuildNodeUnlockLiberation = {0.5f, 1.f};
		return Numbers;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCampaignRulesTierAndRankTest, "RTS.WorldCampaign.Rules.TierAndRank",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldCampaignRulesTierAndRankTest::RunTest(const FString& Parameters)
{
	const FWorldCampaignRuleNumbers Numbers = WorldCampaignRulesTestData::MakeNumbers();
	TestEqual(TEXT("No missions = Tier I"), WorldCampaignRules::GetTierCapForMissionsCompleted(Numbers, 0),
	          EGlobalAbilityTier::TierI);
	TestEqual(TEXT("One mission is still Tier I"), WorldCampaignRules::GetTierCapForMissionsCompleted(Numbers, 1),
	          EGlobalAbilityTier::TierI);
	TestEqual(TEXT("Two missions = Tier II"), WorldCampaignRules::GetTierCapForMissionsCompleted(Numbers, 2),
	          EGlobalAbilityTier::TierII);
	TestEqual(TEXT("Five missions = Tier III"), WorldCampaignRules::GetTierCapForMissionsCompleted(Numbers, 5),
	          EGlobalAbilityTier::TierIII);
	TestEqual(TEXT("Nine missions = Tier IV"), WorldCampaignRules::GetTierCapForMissionsCompleted(Numbers, 9),
	          EGlobalAbilityTier::TierIV);
	TestEqual(TEXT("The cap never passes Tier IV"), WorldCampaignRules::GetTierCapForMissionsCompleted(Numbers, 99),
	          EGlobalAbilityTier::TierIV);

	TestEqual(TEXT("A new campaign starts as Corporal"), WorldCampaignRules::GetRankForMissionsCompleted(Numbers, 0),
	          ERTSPlayerRank::Corporal);
	TestEqual(TEXT("Each mission is one rank"), WorldCampaignRules::GetRankForMissionsCompleted(Numbers, 3),
	          ERTSPlayerRank::SergeantFirstClass);
	TestEqual(TEXT("Rank stops at Field Marshal"), WorldCampaignRules::GetRankForMissionsCompleted(Numbers, 99),
	          ERTSPlayerRank::FieldMarshal);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCampaignRulesAlertAndBudgetTest, "RTS.WorldCampaign.Rules.AlertAndBudget",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldCampaignRulesAlertAndBudgetTest::RunTest(const FString& Parameters)
{
	const FWorldCampaignRuleNumbers Numbers = WorldCampaignRulesTestData::MakeNumbers();
	const float AlertAfterMissionAtHome = WorldCampaignRules::GetAlertAfterStrike(
		Numbers, 0.f, EWorldOperationTarget::Mission, 0.f);
	TestEqual(TEXT("A mission won at home adds the base gain"), AlertAfterMissionAtHome, 0.25f);
	const float AlertAfterItemAtWall = WorldCampaignRules::GetAlertAfterStrike(
		Numbers, 0.f, EWorldOperationTarget::EnemyItem, 1.f);
	TestEqual(TEXT("A strike at the wall adds the proximity gain"), AlertAfterItemAtWall, 0.35f);
	TestEqual(TEXT("Alert never passes 1"),
	          WorldCampaignRules::GetAlertAfterStrike(Numbers, 0.95f, EWorldOperationTarget::Mission, 1.f), 1.f);
	TestEqual(TEXT("A quiet turn lowers alert"), WorldCampaignRules::GetAlertAfterQuietTurn(Numbers, 0.25f), 0.15f);
	TestEqual(TEXT("Alert never drops below 0"), WorldCampaignRules::GetAlertAfterQuietTurn(Numbers, 0.05f), 0.f);

	const FWorldEnemyBudgetSplit CalmSplit = WorldCampaignRules::SplitEnemyBudget(Numbers, 0.f);
	TestEqual(TEXT("A calm enemy puts everything into the freeze"), CalmSplit.FreezePoints, 10);
	TestEqual(TEXT("A calm enemy does not react"), CalmSplit.ReactionPoints, 0);
	const FWorldEnemyBudgetSplit AlarmedSplit = WorldCampaignRules::SplitEnemyBudget(Numbers, 1.f);
	TestEqual(TEXT("A fully alarmed enemy only reacts"), AlarmedSplit.ReactionPoints, 10);
	const FWorldEnemyBudgetSplit MixedSplit = WorldCampaignRules::SplitEnemyBudget(Numbers, 0.3f);
	TestEqual(TEXT("Alert is the reaction share"), MixedSplit.ReactionPoints, 3);
	TestEqual(TEXT("The shares add up to the budget"), MixedSplit.ReactionPoints + MixedSplit.FreezePoints, 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCampaignRulesFreezeTest, "RTS.WorldCampaign.Rules.Freeze",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldCampaignRulesFreezeTest::RunTest(const FString& Parameters)
{
	const FWorldCampaignRuleNumbers Numbers = WorldCampaignRulesTestData::MakeNumbers();
	const FWorldFreezeAdvance PartialAdvance = WorldCampaignRules::AdvanceFreeze(Numbers, 0, 0, 10);
	TestEqual(TEXT("Points below the stage cost stay as progress"), PartialAdvance.NewProgressPoints, 10);
	TestEqual(TEXT("No stage is gained yet"), PartialAdvance.StagesGained, 0);

	const FWorldFreezeAdvance StageAdvance = WorldCampaignRules::AdvanceFreeze(Numbers, 25, 0, 10);
	TestEqual(TEXT("Reaching the cost finishes the stage"), StageAdvance.NewStage, 1);
	TestEqual(TEXT("The surplus carries into the next stage"), StageAdvance.NewProgressPoints, 5);

	TestEqual(TEXT("The last configured cost repeats for later stages"),
	          WorldCampaignRules::GetFreezePointsNeededForStage(Numbers, 5), 40);
	const FWorldFreezeAdvance FinalAdvance = WorldCampaignRules::AdvanceFreeze(Numbers, 0, 0, 1000);
	TestEqual(TEXT("The freeze stops at its final stage"), FinalAdvance.NewStage, 3);
	TestEqual(TEXT("A finished freeze holds no spare points"), FinalAdvance.NewProgressPoints, 0);
	TestTrue(TEXT("The final stage is complete"), WorldCampaignRules::GetIsFreezeComplete(Numbers, 3));
	const FWorldFreezeAdvance AfterCompleteAdvance = WorldCampaignRules::AdvanceFreeze(Numbers, 0, 3, 50);
	TestEqual(TEXT("Nothing is gained after completion"), AfterCompleteAdvance.StagesGained, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCampaignRulesCountriesTest, "RTS.WorldCampaign.Rules.Countries",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldCampaignRulesCountriesTest::RunTest(const FString& Parameters)
{
	const FWorldCampaignRuleNumbers Numbers = WorldCampaignRulesTestData::MakeNumbers();
	TestEqual(TEXT("A country without targets is free"), WorldCampaignRules::GetCountryLiberation(Numbers, 0, 0, 0, 0),
	          1.f);
	TestEqual(TEXT("Nothing cleared = 0"), WorldCampaignRules::GetCountryLiberation(Numbers, 1, 0, 2, 0), 0.f);
	TestEqual(TEXT("The story mission weighs double"), WorldCampaignRules::GetCountryLiberation(Numbers, 1, 1, 2, 0),
	          0.5f);
	TestEqual(TEXT("Everything cleared = 1"), WorldCampaignRules::GetCountryLiberation(Numbers, 1, 1, 2, 2), 1.f);

	TestEqual(TEXT("No node opens before the first cleared target"),
	          WorldCampaignRules::GetUnlockedBuildNodeCount(Numbers, 3, 0, 0.f), 0);
	TestEqual(TEXT("The first cleared target opens one node"),
	          WorldCampaignRules::GetUnlockedBuildNodeCount(Numbers, 3, 1, 0.25f), 1);
	TestEqual(TEXT("Half free opens the second node"),
	          WorldCampaignRules::GetUnlockedBuildNodeCount(Numbers, 3, 2, 0.5f), 2);
	TestEqual(TEXT("A free country opens every node"),
	          WorldCampaignRules::GetUnlockedBuildNodeCount(Numbers, 3, 3, 1.f), 3);
	TestEqual(TEXT("Never more nodes than the country has"),
	          WorldCampaignRules::GetUnlockedBuildNodeCount(Numbers, 1, 3, 1.f), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCampaignProgressOperationTest, "RTS.WorldCampaign.Progress.OperationResult",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldCampaignProgressOperationTest::RunTest(const FString& Parameters)
{
	UWorldCampaignProgressManager* ProgressManager = NewObject<UWorldCampaignProgressManager>();
	FRTSGameDifficulty Difficulty;
	ProgressManager->InitializeForNewCampaign(ERTSCommander::BalancedCommander, Difficulty);
	const FWorldMapResources StartingResources = ProgressManager->GetResources();

	const FGuid TargetAnchorKey = FGuid::NewGuid();
	FWorldPendingOperation PendingOperation;
	PendingOperation.Target = EWorldOperationTarget::Mission;
	PendingOperation.AnchorKey = TargetAnchorKey;
	PendingOperation.MissionType = EMapMission::HetzerMission;
	ProgressManager->BeginOperation(PendingOperation);

	FWorldMissionResult StrangerResult;
	StrangerResult.Outcome = EWorldMissionOutcome::Victory;
	StrangerResult.SourceAnchorKey = FGuid::NewGuid();
	TestFalse(TEXT("A result for another anchor is not applied"),
	          ProgressManager->ResolvePendingOperation(StrangerResult, nullptr, 0.f).bWasResolved);

	FWorldMissionResult DefeatResult;
	DefeatResult.Outcome = EWorldMissionOutcome::Defeat;
	DefeatResult.SourceAnchorKey = TargetAnchorKey;
	const FWorldOperationResolution DefeatResolution = ProgressManager->ResolvePendingOperation(
		DefeatResult, nullptr, 0.f);
	TestTrue(TEXT("A defeat resolves the operation"), DefeatResolution.bWasResolved);
	TestFalse(TEXT("A defeat is not a victory"), DefeatResolution.bWasVictory);
	TestEqual(TEXT("A defeat completes no mission"), ProgressManager->GetMissionsCompleted(), 0);
	TestFalse(TEXT("A defeat clears nothing"), ProgressManager->GetIsAnchorCleared(TargetAnchorKey));
	TestEqual(TEXT("A defeat raises no alert"), ProgressManager->GetProgressState().EnemyTurn.Alert, 0.f);

	ProgressManager->BeginOperation(PendingOperation);
	FPrimaryReward Reward;
	Reward.RadixiteReward = 120;
	Reward.Blueprints.ResourceCosts.Add(ERTSResourceType::Resource_Metal, 2);
	FWorldMissionResult VictoryResult;
	VictoryResult.Outcome = EWorldMissionOutcome::Victory;
	VictoryResult.SourceAnchorKey = TargetAnchorKey;
	const FWorldOperationResolution VictoryResolution = ProgressManager->ResolvePendingOperation(
		VictoryResult, &Reward, 0.f);
	TestTrue(TEXT("A victory resolves the operation"), VictoryResolution.bWasVictory);
	TestEqual(TEXT("A victory counts one mission"), ProgressManager->GetMissionsCompleted(), 1);
	TestTrue(TEXT("A victory clears the anchor"), ProgressManager->GetIsAnchorCleared(TargetAnchorKey));
	TestEqual(TEXT("The radixite reward is paid"), ProgressManager->GetResources().Radixite,
	          StartingResources.Radixite + 120);
	TestEqual(TEXT("The blueprint reward is paid"), ProgressManager->GetResources().Blueprints,
	          StartingResources.Blueprints + 2);
	TestTrue(TEXT("A victory raises alert"), ProgressManager->GetProgressState().EnemyTurn.Alert > 0.f);
	TestFalse(TEXT("The operation is no longer pending"),
	          ProgressManager->GetProgressState().PendingOperation.bIsPending);
	TestFalse(TEXT("The same result cannot be applied twice"),
	          ProgressManager->ResolvePendingOperation(VictoryResult, &Reward, 0.f).bWasResolved);

	FWorldMapResources TooExpensive;
	TooExpensive.Blueprints = 9999;
	TestFalse(TEXT("An unaffordable price takes nothing"), ProgressManager->TrySpendResources(TooExpensive));
	TestEqual(TEXT("Resources are unchanged after a refused purchase"), ProgressManager->GetResources().Radixite,
	          StartingResources.Radixite + 120);
	return true;
}

#endif
