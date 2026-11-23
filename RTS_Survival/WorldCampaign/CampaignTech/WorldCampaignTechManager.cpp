// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/CampaignTech/WorldCampaignTechManager.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignProgressManager.h"
#include "RTS_Survival/WorldCampaign/CampaignTech/WorldCampaignTechRules.h"

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
