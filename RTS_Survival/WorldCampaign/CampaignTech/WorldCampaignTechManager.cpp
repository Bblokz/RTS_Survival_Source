// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/CampaignTech/WorldCampaignTechManager.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignProgressManager.h"
#include "RTS_Survival/WorldCampaign/CampaignTech/WorldCampaignTechRules.h"
#include "RTS_Survival/WorldCampaign/CampaignTech/WorldCampaignTechWheel.h"

#define LOCTEXT_NAMESPACE "WorldCampaignTechManager"

UWorldCampaignTechManager::UWorldCampaignTechManager()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWorldCampaignTechManager::BeginPlay()
{
	Super::BeginPlay();

	const AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return;
	}
	M_ProgressManager = Owner->FindComponentByClass<UWorldCampaignProgressManager>();
}

bool UWorldCampaignTechManager::TryResearchTechnology(const ETechnology Technology, FText& OutFailReason)
{
	if (not GetCanResearchTechnology(Technology, OutFailReason))
	{
		return false;
	}

	const FWorldCampaignTechDefinition* Definition = UWorldCampaignTechSettings::GetTechTable().FindDefinition(
		Technology);
	if (Definition == nullptr || not M_ProgressManager->TrySpendResources(Definition->Cost))
	{
		OutFailReason = WorldCampaignTechRules::GetResearchBlockText(EWorldCampaignTechResearchBlock::CannotAfford, {});
		return false;
	}

	M_ProgressManager->AddResearchedTechnology(Technology);
	OnTechnologyPurchased.Broadcast(Technology, M_ProgressManager->GetResources());
	return true;
}

bool UWorldCampaignTechManager::GetCanResearchTechnology(const ETechnology Technology, FText& OutFailReason) const
{
	OutFailReason = FText::GetEmpty();
	if (not GetIsValidProgressManager())
	{
		OutFailReason = LOCTEXT("NoCampaign", "No campaign is running.");
		return false;
	}

	const FWorldCampaignTechTable& TechTable = UWorldCampaignTechSettings::GetTechTable();
	const FWorldCampaignProgressState& ProgressState = M_ProgressManager->GetProgressState();
	const EWorldCampaignTechResearchBlock Block = WorldCampaignTechRules::GetResearchBlock(
		TechTable, Technology, ProgressState.ResearchedTechnologies, ProgressState.Resources);
	if (Block == EWorldCampaignTechResearchBlock::None)
	{
		return true;
	}

	const TArray<ETechnology> MissingPrerequisites = WorldCampaignTechRules::GetMissingPrerequisites(
		TechTable, Technology, ProgressState.ResearchedTechnologies);
	OutFailReason = WorldCampaignTechRules::GetResearchBlockText(Block, MissingPrerequisites);
	return false;
}

bool UWorldCampaignTechManager::GetTechnologyDefinition(const ETechnology Technology,
                                                        FWorldCampaignTechDefinition& OutDefinition) const
{
	const FWorldCampaignTechDefinition* Definition = UWorldCampaignTechSettings::GetTechTable().FindDefinition(
		Technology);
	if (Definition == nullptr)
	{
		return false;
	}
	OutDefinition = *Definition;
	return true;
}

TArray<ETechnology> UWorldCampaignTechManager::GetTechnologiesInBranch(const EWorldCampaignTechBranch Branch) const
{
	return WorldCampaignTechRules::GetTechnologiesInBranch(UWorldCampaignTechSettings::GetTechTable(), Branch);
}

TArray<ETechnology> UWorldCampaignTechManager::GetResearchedTechnologies() const
{
	if (not GetIsValidProgressManager())
	{
		return TArray<ETechnology>();
	}
	return M_ProgressManager->GetProgressState().ResearchedTechnologies;
}

bool UWorldCampaignTechManager::GetIsTechnologyResearched(const ETechnology Technology) const
{
	if (not GetIsValidProgressManager())
	{
		return false;
	}
	return M_ProgressManager->GetIsTechnologyResearched(Technology);
}

bool UWorldCampaignTechManager::TryBuyWheelNode(const FName NodeId, FText& OutFailReason)
{
	if (not GetCanBuyWheelNode(NodeId, OutFailReason))
	{
		return false;
	}

	const FWorldTechWheelNode* const Node = GetWheel().FindNode(NodeId);
	if (Node == nullptr || not M_ProgressManager->TrySpendResources(Node->Cost))
	{
		OutFailReason = WorldCampaignTechWheel::GetNodeBuyBlockText(EWorldTechWheelBuyBlock::CannotAfford, Node);
		return false;
	}

	const ETechnology Technology = Node->GetIsTechnologyNode() ? Node->Technology : ETechnology::Tech_NONE;
	M_ProgressManager->AddBoughtTechNode(NodeId);
	if (Technology != ETechnology::Tech_NONE)
	{
		M_ProgressManager->AddResearchedTechnology(Technology);
		OnTechnologyPurchased.Broadcast(Technology, M_ProgressManager->GetResources());
	}
	OnWheelNodeBought.Broadcast(NodeId);
	return true;
}

bool UWorldCampaignTechManager::GetCanBuyWheelNode(const FName NodeId, FText& OutFailReason) const
{
	OutFailReason = FText::GetEmpty();
	if (not GetIsValidProgressManager())
	{
		OutFailReason = LOCTEXT("NoCampaignWheel", "No campaign is running.");
		return false;
	}

	const FWorldTechWheel& Wheel = GetWheel();
	const FWorldCampaignProgressState& ProgressState = M_ProgressManager->GetProgressState();
	const EWorldTechWheelBuyBlock Block = WorldCampaignTechWheel::GetNodeBuyBlock(
		Wheel, NodeId, ProgressState.BoughtTechNodeIds, ProgressState.ResearchedTechnologies, ProgressState.Resources);
	if (Block == EWorldTechWheelBuyBlock::None)
	{
		return true;
	}

	OutFailReason = WorldCampaignTechWheel::GetNodeBuyBlockText(Block, Wheel.FindNode(NodeId));
	return false;
}

EWorldTechWheelNodeState UWorldCampaignTechManager::GetWheelNodeState(const FName NodeId) const
{
	if (not GetIsValidProgressManager())
	{
		return EWorldTechWheelNodeState::Locked;
	}
	return WorldCampaignTechWheel::GetNodeState(GetWheel(), NodeId,
	                                            M_ProgressManager->GetProgressState().BoughtTechNodeIds);
}

TArray<FWorldTechWheelNode> UWorldCampaignTechManager::GetWheelNodes() const
{
	return GetWheel().Nodes;
}

const FWorldTechWheel& UWorldCampaignTechManager::GetWheel() const
{
	return UWorldCampaignTechSettings::GetWheel();
}

bool UWorldCampaignTechManager::GetIsValidProgressManager() const
{
	if (M_ProgressManager.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_ProgressManager",
		"GetIsValidProgressManager",
		GetOwner()
	);
	return false;
}

#undef LOCTEXT_NAMESPACE
