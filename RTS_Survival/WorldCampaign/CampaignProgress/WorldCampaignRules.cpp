// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignRules.h"

namespace WorldCampaignRulesConstants
{
	constexpr int32 HighestRankIndex = static_cast<int32>(ERTSPlayerRank::FieldMarshal);
	constexpr int32 HighestTierIndex = static_cast<int32>(EGlobalAbilityTier::TierIV);
	constexpr int32 FallbackFreezePointsPerStage = 50;
}

EGlobalAbilityTier WorldCampaignRules::GetTierCapForMissionsCompleted(const FWorldCampaignRuleNumbers& Numbers,
                                                                      const int32 MissionsCompleted)
{
	int32 TierIndex = 0;
	for (const int32 MissionsNeeded : Numbers.MissionsNeededPerTierAboveOne)
	{
		if (MissionsCompleted < MissionsNeeded)
		{
			break;
		}
		TierIndex++;
	}

	TierIndex = FMath::Min(TierIndex, WorldCampaignRulesConstants::HighestTierIndex);
	return static_cast<EGlobalAbilityTier>(TierIndex);
}

ERTSPlayerRank WorldCampaignRules::GetRankForMissionsCompleted(const FWorldCampaignRuleNumbers& Numbers,
                                                               const int32 MissionsCompleted)
{
	const int32 MissionsPerRank = FMath::Max(1, Numbers.MissionsPerRank);
	const int32 RankIndex = FMath::Clamp(MissionsCompleted / MissionsPerRank, 0,
	                                     WorldCampaignRulesConstants::HighestRankIndex);
	return static_cast<ERTSPlayerRank>(RankIndex);
}

float WorldCampaignRules::GetAlertAfterStrike(const FWorldCampaignRuleNumbers& Numbers, const float CurrentAlert,
                                              const EWorldOperationTarget Target, const float WallProximity)
{
	const float BaseGain = Target == EWorldOperationTarget::Mission
		                       ? Numbers.AlertGainMissionWon
		                       : Numbers.AlertGainEnemyItemDestroyed;
	const float ProximityGain = Numbers.AlertGainMaxExtraNearWall * FMath::Clamp(WallProximity, 0.f, 1.f);
	return FMath::Clamp(CurrentAlert + BaseGain + ProximityGain, 0.f, 1.f);
}

float WorldCampaignRules::GetAlertAfterQuietTurn(const FWorldCampaignRuleNumbers& Numbers, const float CurrentAlert)
{
	return FMath::Clamp(CurrentAlert - Numbers.AlertDecayPerQuietTurn, 0.f, 1.f);
}

FWorldEnemyBudgetSplit WorldCampaignRules::SplitEnemyBudget(const FWorldCampaignRuleNumbers& Numbers,
                                                            const float Alert)
{
	FWorldEnemyBudgetSplit BudgetSplit;
	const int32 Budget = FMath::Max(0, Numbers.EnemyBudgetPerTurn);
	BudgetSplit.ReactionPoints = FMath::Clamp(FMath::RoundToInt(Budget * FMath::Clamp(Alert, 0.f, 1.f)), 0, Budget);
	BudgetSplit.FreezePoints = Budget - BudgetSplit.ReactionPoints;
	return BudgetSplit;
}

int32 WorldCampaignRules::GetFreezePointsNeededForStage(const FWorldCampaignRuleNumbers& Numbers,
                                                        const int32 StageIndex)
{
	if (Numbers.FreezePointsPerStage.Num() == 0)
	{
		return WorldCampaignRulesConstants::FallbackFreezePointsPerStage;
	}

	const int32 ClampedIndex = FMath::Clamp(StageIndex, 0, Numbers.FreezePointsPerStage.Num() - 1);
	return FMath::Max(1, Numbers.FreezePointsPerStage[ClampedIndex]);
}

FWorldFreezeAdvance WorldCampaignRules::AdvanceFreeze(const FWorldCampaignRuleNumbers& Numbers,
                                                      const int32 CurrentProgressPoints, const int32 CurrentStage,
                                                      const int32 PointsToAdd)
{
	FWorldFreezeAdvance FreezeAdvance;
	FreezeAdvance.NewStage = FMath::Max(0, CurrentStage);
	FreezeAdvance.NewProgressPoints = FMath::Max(0, CurrentProgressPoints);
	if (GetIsFreezeComplete(Numbers, FreezeAdvance.NewStage))
	{
		FreezeAdvance.NewProgressPoints = 0;
		return FreezeAdvance;
	}

	FreezeAdvance.NewProgressPoints += FMath::Max(0, PointsToAdd);
	while (not GetIsFreezeComplete(Numbers, FreezeAdvance.NewStage))
	{
		const int32 PointsNeeded = GetFreezePointsNeededForStage(Numbers, FreezeAdvance.NewStage);
		if (FreezeAdvance.NewProgressPoints < PointsNeeded)
		{
			break;
		}

		FreezeAdvance.NewProgressPoints -= PointsNeeded;
		FreezeAdvance.NewStage++;
		FreezeAdvance.StagesGained++;
	}

	if (GetIsFreezeComplete(Numbers, FreezeAdvance.NewStage))
	{
		FreezeAdvance.NewProgressPoints = 0;
	}
	return FreezeAdvance;
}

bool WorldCampaignRules::GetIsFreezeComplete(const FWorldCampaignRuleNumbers& Numbers, const int32 Stage)
{
	return Stage >= FMath::Max(1, Numbers.FreezeFinalStage);
}

float WorldCampaignRules::GetCountryLiberation(const FWorldCampaignRuleNumbers& Numbers,
                                               const int32 StoryMissionsTotal, const int32 StoryMissionsCleared,
                                               const int32 OtherTargetsTotal, const int32 OtherTargetsCleared)
{
	const int32 StoryWeight = FMath::Max(1, Numbers.StoryMissionLiberationWeight);
	const int32 TotalWeight = FMath::Max(0, StoryMissionsTotal) * StoryWeight + FMath::Max(0, OtherTargetsTotal);
	if (TotalWeight <= 0)
	{
		return 1.f;
	}

	const int32 ClearedWeight = FMath::Clamp(StoryMissionsCleared, 0, StoryMissionsTotal) * StoryWeight
		+ FMath::Clamp(OtherTargetsCleared, 0, OtherTargetsTotal);
	return static_cast<float>(ClearedWeight) / static_cast<float>(TotalWeight);
}

int32 WorldCampaignRules::GetUnlockedBuildNodeCount(const FWorldCampaignRuleNumbers& Numbers,
                                                    const int32 NodesInCountry, const int32 ClearedTargetsInCountry,
                                                    const float Liberation)
{
	if (NodesInCountry <= 0)
	{
		return 0;
	}

	// A country without any target is free ground from the start: every node is open.
	if (Liberation >= 1.f)
	{
		return NodesInCountry;
	}

	if (ClearedTargetsInCountry <= 0)
	{
		return 0;
	}

	int32 UnlockedNodes = 1;
	for (const float LiberationNeeded : Numbers.BuildNodeUnlockLiberation)
	{
		if (Liberation + KINDA_SMALL_NUMBER < LiberationNeeded)
		{
			break;
		}
		UnlockedNodes++;
	}
	return FMath::Min(UnlockedNodes, NodesInCountry);
}
