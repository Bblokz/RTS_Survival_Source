// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldBuildNodes/WorldBuildNodeRules.h"

#define LOCTEXT_NAMESPACE "WorldBuildNodeRules"

namespace WorldBuildNodeRulesConstants
{
	// Seed for the deterministic source keys of build nodes; any fixed value works, it must never change.
	constexpr uint64 BuildNodeSourceKeySeed = 0x52545342754E6F64ull;
	const TCHAR* const BuildNodeSourceKeyPrefix = TEXT("WorldBuildNode.");
}

TArray<FName> WorldBuildNodeRules::GetNodesInUnlockOrder(const TArray<FWorldBuildNodeUnlockEntry>& NodesOfCountry)
{
	TArray<FWorldBuildNodeUnlockEntry> SortedNodes = NodesOfCountry;
	SortedNodes.Sort([](const FWorldBuildNodeUnlockEntry& NodeA, const FWorldBuildNodeUnlockEntry& NodeB)
	{
		if (NodeA.UnlockOrder != NodeB.UnlockOrder)
		{
			return NodeA.UnlockOrder < NodeB.UnlockOrder;
		}
		return NodeA.BuildNodeId.LexicalLess(NodeB.BuildNodeId);
	});

	TArray<FName> NodeIdsInOrder;
	NodeIdsInOrder.Reserve(SortedNodes.Num());
	for (const FWorldBuildNodeUnlockEntry& SortedNode : SortedNodes)
	{
		NodeIdsInOrder.Add(SortedNode.BuildNodeId);
	}
	return NodeIdsInOrder;
}

TArray<FName> WorldBuildNodeRules::GetUnlockedNodeIds(const TArray<FWorldBuildNodeUnlockEntry>& NodesOfCountry,
                                                      const int32 UnlockedNodeCount)
{
	TArray<FName> NodeIdsInOrder = GetNodesInUnlockOrder(NodesOfCountry);
	const int32 ClampedUnlockedCount = FMath::Clamp(UnlockedNodeCount, 0, NodeIdsInOrder.Num());
	NodeIdsInOrder.SetNum(ClampedUnlockedCount);
	return NodeIdsInOrder;
}

EWorldBuildNodeState WorldBuildNodeRules::GetNodeStateAfterUnlockPass(const EWorldBuildNodeState CurrentState,
                                                                     const bool bIsInUnlockedSet,
                                                                     const EWorldCampaignBuilding Building)
{
	if (Building != EWorldCampaignBuilding::None || CurrentState == EWorldBuildNodeState::Built)
	{
		return EWorldBuildNodeState::Built;
	}

	// Freed ground cannot be lost for now, so an unlocked node stays unlocked.
	if (bIsInUnlockedSet || CurrentState == EWorldBuildNodeState::Unlocked)
	{
		return EWorldBuildNodeState::Unlocked;
	}
	return EWorldBuildNodeState::Locked;
}

EWorldBuildNodeBuildResult WorldBuildNodeRules::GetBuildResult(const EWorldBuildNodeState NodeState,
                                                               const EWorldCampaignBuilding ExistingBuilding,
                                                               const EWorldCampaignBuilding Building,
                                                               const FWorldMapResources* Cost,
                                                               const FWorldMapResources& Resources)
{
	if (Building == EWorldCampaignBuilding::None)
	{
		return EWorldBuildNodeBuildResult::InvalidBuilding;
	}

	if (NodeState == EWorldBuildNodeState::Built || ExistingBuilding != EWorldCampaignBuilding::None)
	{
		return EWorldBuildNodeBuildResult::NodeOccupied;
	}

	if (NodeState != EWorldBuildNodeState::Unlocked)
	{
		return EWorldBuildNodeBuildResult::NodeLocked;
	}

	if (Cost == nullptr)
	{
		return EWorldBuildNodeBuildResult::NoPriceConfigured;
	}

	return Resources.GetCanAfford(*Cost)
		       ? EWorldBuildNodeBuildResult::Success
		       : EWorldBuildNodeBuildResult::CannotAfford;
}

FText WorldBuildNodeRules::GetBuildResultText(const EWorldBuildNodeBuildResult BuildResult)
{
	switch (BuildResult)
	{
	case EWorldBuildNodeBuildResult::Success:
		return FText::GetEmpty();
	case EWorldBuildNodeBuildResult::UnknownNode:
		return LOCTEXT("UnknownNode", "This build site does not exist.");
	case EWorldBuildNodeBuildResult::InvalidBuilding:
		return LOCTEXT("InvalidBuilding", "Choose a building first.");
	case EWorldBuildNodeBuildResult::NodeLocked:
		return LOCTEXT("NodeLocked", "Free more of this country to use this build site.");
	case EWorldBuildNodeBuildResult::NodeOccupied:
		return LOCTEXT("NodeOccupied", "A building already stands here.");
	case EWorldBuildNodeBuildResult::NoPriceConfigured:
		return LOCTEXT("NoPriceConfigured", "This building cannot be built yet.");
	case EWorldBuildNodeBuildResult::CannotAfford:
		return LOCTEXT("CannotAfford", "Not enough radixite or blueprints.");
	default:
		return LOCTEXT("UnknownReason", "This cannot be built.");
	}
}

TArray<EWorldCampaignBuilding> WorldBuildNodeRules::GetBuildOptions(
	const TMap<EWorldCampaignBuilding, FWorldMapResources>& BuildingCosts)
{
	TArray<EWorldCampaignBuilding> BuildOptions;
	for (const TPair<EWorldCampaignBuilding, FWorldMapResources>& BuildingCost : BuildingCosts)
	{
		if (BuildingCost.Key == EWorldCampaignBuilding::None)
		{
			continue;
		}
		BuildOptions.Add(BuildingCost.Key);
	}

	BuildOptions.Sort([](const EWorldCampaignBuilding BuildingA, const EWorldCampaignBuilding BuildingB)
	{
		return static_cast<uint8>(BuildingA) < static_cast<uint8>(BuildingB);
	});
	return BuildOptions;
}

FWorldMapResources WorldBuildNodeRules::GetTurnIncome(const FWorldMapResources& IncomePerBuilding,
                                                      const int32 PayingBuildingCount)
{
	const int32 ClampedBuildingCount = FMath::Max(0, PayingBuildingCount);
	FWorldMapResources TurnIncome;
	TurnIncome.Radixite = FMath::Max(0, IncomePerBuilding.Radixite) * ClampedBuildingCount;
	TurnIncome.Blueprints = FMath::Max(0, IncomePerBuilding.Blueprints) * ClampedBuildingCount;
	return TurnIncome;
}

int32 WorldBuildNodeRules::GetPlayerSupportStrengthPercent(const int32 EnemyStrengthReductionPercent)
{
	return -FMath::Abs(EnemyStrengthReductionPercent);
}

bool WorldBuildNodeRules::GetIsWithinSupportRadiusXY(const FVector& SourceLocation, const FVector& TargetLocation,
                                                     const float XYRadius)
{
	if (XYRadius <= 0.f)
	{
		return false;
	}

	return FVector::DistSquared2D(SourceLocation, TargetLocation) <= FMath::Square(XYRadius);
}

FGuid WorldBuildNodeRules::GetBuildNodeSourceKey(const FName BuildNodeId)
{
	const FString SourceKeyText = WorldBuildNodeRulesConstants::BuildNodeSourceKeyPrefix + BuildNodeId.ToString();
	return FGuid::NewDeterministicGuid(SourceKeyText, WorldBuildNodeRulesConstants::BuildNodeSourceKeySeed);
}

#undef LOCTEXT_NAMESPACE
