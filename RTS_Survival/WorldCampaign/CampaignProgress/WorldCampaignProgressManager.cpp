// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignProgressManager.h"

#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignRules.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignRulesSettings.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/MapObjects/W_MissionReward/RewardStructs/FMissionRewardStructs.h"

UWorldCampaignProgressManager::UWorldCampaignProgressManager()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWorldCampaignProgressManager::InitializeForNewCampaign(const ERTSCommander Commander,
                                                             const FRTSGameDifficulty& Difficulty)
{
	M_ProgressState = FWorldCampaignProgressState();
	M_ProgressState.Resources = UWorldCampaignRulesSettings::GetNumbers().StartingResources;
	M_ProgressState.CommanderMap.Commander = Commander;
	M_ProgressState.Difficulty = Difficulty;
	OnResourcesChanged.Broadcast(M_ProgressState.Resources);
}

void UWorldCampaignProgressManager::RestoreFromSave(const FWorldCampaignProgressState& SavedProgressState)
{
	M_ProgressState = SavedProgressState;
	OnResourcesChanged.Broadcast(M_ProgressState.Resources);
}

bool UWorldCampaignProgressManager::GetCanAfford(const FWorldMapResources& Cost) const
{
	return M_ProgressState.Resources.GetCanAfford(Cost);
}

void UWorldCampaignProgressManager::AddResources(const FWorldMapResources& Amount)
{
	M_ProgressState.Resources.Add(Amount);
	OnResourcesChanged.Broadcast(M_ProgressState.Resources);
}

bool UWorldCampaignProgressManager::TrySpendResources(const FWorldMapResources& Cost)
{
	if (not M_ProgressState.Resources.GetCanAfford(Cost))
	{
		return false;
	}

	M_ProgressState.Resources.Subtract(Cost);
	OnResourcesChanged.Broadcast(M_ProgressState.Resources);
	return true;
}

void UWorldCampaignProgressManager::BeginOperation(const FWorldPendingOperation& Operation)
{
	M_ProgressState.PendingOperation = Operation;
	M_ProgressState.PendingOperation.bIsPending = true;
}

void UWorldCampaignProgressManager::DiscardPendingOperation()
{
	M_ProgressState.PendingOperation = FWorldPendingOperation();
}

FWorldOperationResolution UWorldCampaignProgressManager::ResolvePendingOperation(
	const FWorldMissionResult& MissionResult, const FPrimaryReward* RewardOnVictory, const float WallProximity)
{
	FWorldOperationResolution Resolution;
	if (not GetDoesResultMatchPendingOperation(MissionResult))
	{
		return Resolution;
	}

	Resolution.bWasResolved = true;
	Resolution.bWasVictory = MissionResult.Outcome == EWorldMissionOutcome::Victory;
	Resolution.Operation = M_ProgressState.PendingOperation;
	M_ProgressState.PendingOperation = FWorldPendingOperation();

	if (Resolution.bWasVictory)
	{
		ApplyVictory(Resolution.Operation, RewardOnVictory, WallProximity);
	}
	else
	{
		M_ProgressState.MissionsLost++;
	}

	OnOperationResolved.Broadcast(Resolution.Operation, Resolution.bWasVictory);
	return Resolution;
}

ERTSPlayerRank UWorldCampaignProgressManager::GetPlayerRank() const
{
	return WorldCampaignRules::GetRankForMissionsCompleted(UWorldCampaignRulesSettings::GetNumbers(),
	                                                       M_ProgressState.MissionsCompleted);
}

EGlobalAbilityTier UWorldCampaignProgressManager::GetGlobalAbilityTierCap() const
{
	return WorldCampaignRules::GetTierCapForMissionsCompleted(UWorldCampaignRulesSettings::GetNumbers(),
	                                                          M_ProgressState.MissionsCompleted);
}

bool UWorldCampaignProgressManager::GetIsAnchorCleared(const FGuid& AnchorKey) const
{
	return M_ProgressState.GetIsAnchorCleared(AnchorKey);
}

bool UWorldCampaignProgressManager::GetIsTechnologyResearched(const ETechnology Technology) const
{
	return M_ProgressState.ResearchedTechnologies.Contains(Technology);
}

void UWorldCampaignProgressManager::AddResearchedTechnology(const ETechnology Technology)
{
	if (Technology == ETechnology::Tech_NONE || M_ProgressState.ResearchedTechnologies.Contains(Technology))
	{
		return;
	}

	M_ProgressState.ResearchedTechnologies.Add(Technology);
	OnTechnologyResearched.Broadcast(Technology);
}

EWorldCampaignBuilding UWorldCampaignProgressManager::GetBuildingOnNode(const FName BuildNodeId) const
{
	return M_ProgressState.GetBuildingOnNode(BuildNodeId);
}

void UWorldCampaignProgressManager::SetBuildingOnNode(const FName BuildNodeId, const EWorldCampaignBuilding Building,
                                                      const int32 CurrentTurn)
{
	if (BuildNodeId.IsNone() || Building == EWorldCampaignBuilding::None)
	{
		return;
	}

	// A node holds one building and buildings cannot be lost or replaced for now.
	if (M_ProgressState.GetBuildingOnNode(BuildNodeId) != EWorldCampaignBuilding::None)
	{
		return;
	}

	FWorldBuildNodeSaveData BuildNodeSaveData;
	BuildNodeSaveData.BuildNodeId = BuildNodeId;
	BuildNodeSaveData.Building = Building;
	BuildNodeSaveData.BuiltOnTurn = CurrentTurn;
	M_ProgressState.Buildings.Add(BuildNodeSaveData);
	OnBuildingBuilt.Broadcast(BuildNodeId, Building);
}

int32 UWorldCampaignProgressManager::GetBuildingCount(const EWorldCampaignBuilding Building) const
{
	int32 BuildingCount = 0;
	for (const FWorldBuildNodeSaveData& BuildNodeSaveData : M_ProgressState.Buildings)
	{
		if (BuildNodeSaveData.Building == Building)
		{
			BuildingCount++;
		}
	}
	return BuildingCount;
}

void UWorldCampaignProgressManager::ApplyVictory(const FWorldPendingOperation& Operation,
                                                 const FPrimaryReward* RewardOnVictory, const float WallProximity)
{
	const FWorldCampaignRuleNumbers& Numbers = UWorldCampaignRulesSettings::GetNumbers();
	const ERTSPlayerRank RankBefore = GetPlayerRank();
	M_ProgressState.MissionsCompleted++;

	if (Operation.Target == EWorldOperationTarget::Mission && Operation.MissionType != EMapMission::None)
	{
		M_ProgressState.CompletedMissions.AddUnique(Operation.MissionType);
	}

	FWorldEnemyTurnState& EnemyTurnState = M_ProgressState.EnemyTurn;
	EnemyTurnState.Alert = WorldCampaignRules::GetAlertAfterStrike(Numbers, EnemyTurnState.Alert, Operation.Target,
	                                                              WallProximity);
	if (RewardOnVictory != nullptr)
	{
		GrantReward(*RewardOnVictory);
	}

	if (Operation.AnchorKey.IsValid())
	{
		EnemyTurnState.RecentlyStruckAnchorKeys.AddUnique(Operation.AnchorKey);
		MarkAnchorCleared(Operation.AnchorKey);
	}

	const ERTSPlayerRank RankAfter = GetPlayerRank();
	if (RankAfter != RankBefore)
	{
		OnRankChanged.Broadcast(RankAfter, GetGlobalAbilityTierCap());
	}
}

void UWorldCampaignProgressManager::GrantReward(const FPrimaryReward& Reward)
{
	FWorldMapResources RewardResources;
	RewardResources.Radixite = FMath::Max(0, Reward.RadixiteReward);
	// The map has one blueprint currency; the reward's blueprint types are summed into it.
	for (const TPair<ERTSResourceType, int32>& BlueprintAmount : Reward.Blueprints.ResourceCosts)
	{
		RewardResources.Blueprints += FMath::Max(0, BlueprintAmount.Value);
	}

	AddResources(RewardResources);
}

void UWorldCampaignProgressManager::MarkAnchorCleared(const FGuid& AnchorKey)
{
	if (M_ProgressState.ClearedAnchorKeys.Contains(AnchorKey))
	{
		return;
	}

	M_ProgressState.ClearedAnchorKeys.Add(AnchorKey);
	OnAnchorCleared.Broadcast(AnchorKey);
}

bool UWorldCampaignProgressManager::GetDoesResultMatchPendingOperation(const FWorldMissionResult& MissionResult) const
{
	const FWorldPendingOperation& PendingOperation = M_ProgressState.PendingOperation;
	if (not PendingOperation.bIsPending || not MissionResult.GetHasResult())
	{
		return false;
	}

	if (PendingOperation.Target == EWorldOperationTarget::EnemyDivision)
	{
		return PendingOperation.DivisionKey == MissionResult.SourceDivisionKey;
	}
	return PendingOperation.AnchorKey == MissionResult.SourceAnchorKey;
}
