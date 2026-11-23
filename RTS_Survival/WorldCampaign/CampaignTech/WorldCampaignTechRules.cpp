// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/CampaignTech/WorldCampaignTechRules.h"

#define LOCTEXT_NAMESPACE "WorldCampaignTechRules"

namespace WorldCampaignTechRulesPrivate
{
	FText JoinTechnologyNames(const TArray<ETechnology>& Technologies)
	{
		TArray<FString> Names;
		for (const ETechnology Technology : Technologies)
		{
			Names.Add(Global_GetTechDisplayName(Technology));
		}
		return FText::FromString(FString::Join(Names, TEXT(", ")));
	}

	void CheckDefinitionConsistency(const FWorldCampaignTechTable& Table, const ETechnology Technology,
	                                const FWorldCampaignTechDefinition& Definition, TArray<FString>& OutProblems)
	{
		const FString TechnologyName = Global_GetTechAsString(Technology);
		if (Definition.Branch == EWorldCampaignTechBranch::None)
		{
			OutProblems.Add(TechnologyName + TEXT(" has no branch."));
		}
		for (const ETechnology Prerequisite : Definition.Prerequisites)
		{
			const FWorldCampaignTechDefinition* PrerequisiteDefinition = Table.FindDefinition(Prerequisite);
			if (PrerequisiteDefinition == nullptr)
			{
				OutProblems.Add(TechnologyName + TEXT(" needs ") + Global_GetTechAsString(Prerequisite) +
					TEXT(", which is not in the tree."));
				continue;
			}
			if (PrerequisiteDefinition->Row >= Definition.Row)
			{
				OutProblems.Add(TechnologyName + TEXT(" needs ") + Global_GetTechAsString(Prerequisite) +
					TEXT(", which is not in a lower row."));
			}
		}
	}
}

TArray<ETechnology> WorldCampaignTechRules::GetMissingPrerequisites(const FWorldCampaignTechTable& Table,
                                                                    const ETechnology Technology,
                                                                    const TArray<ETechnology>& ResearchedTechnologies)
{
	TArray<ETechnology> MissingPrerequisites;
	const FWorldCampaignTechDefinition* Definition = Table.FindDefinition(Technology);
	if (Definition == nullptr)
	{
		return MissingPrerequisites;
	}

	for (const ETechnology Prerequisite : Definition->Prerequisites)
	{
		if (not ResearchedTechnologies.Contains(Prerequisite))
		{
			MissingPrerequisites.Add(Prerequisite);
		}
	}
	return MissingPrerequisites;
}

EWorldCampaignTechResearchBlock WorldCampaignTechRules::GetResearchBlock(const FWorldCampaignTechTable& Table,
                                                                         const ETechnology Technology,
                                                                         const TArray<ETechnology>&
                                                                         ResearchedTechnologies,
                                                                         const FWorldMapResources& Resources)
{
	const FWorldCampaignTechDefinition* Definition = Table.FindDefinition(Technology);
	if (Technology == ETechnology::Tech_NONE || Definition == nullptr)
	{
		return EWorldCampaignTechResearchBlock::NotInTree;
	}
	if (ResearchedTechnologies.Contains(Technology))
	{
		return EWorldCampaignTechResearchBlock::AlreadyResearched;
	}
	if (not GetMissingPrerequisites(Table, Technology, ResearchedTechnologies).IsEmpty())
	{
		return EWorldCampaignTechResearchBlock::MissingPrerequisite;
	}
	if (not Resources.GetCanAfford(Definition->Cost))
	{
		return EWorldCampaignTechResearchBlock::CannotAfford;
	}
	return EWorldCampaignTechResearchBlock::None;
}

TArray<ETechnology> WorldCampaignTechRules::GetTechnologiesInBranch(const FWorldCampaignTechTable& Table,
                                                                    const EWorldCampaignTechBranch Branch)
{
	TArray<ETechnology> TechnologiesInBranch;
	if (Branch == EWorldCampaignTechBranch::None)
	{
		return TechnologiesInBranch;
	}

	for (const TPair<ETechnology, FWorldCampaignTechDefinition>& Entry : Table.Technologies)
	{
		if (Entry.Value.Branch == Branch)
		{
			TechnologiesInBranch.Add(Entry.Key);
		}
	}

	TechnologiesInBranch.Sort([&Table](const ETechnology Left, const ETechnology Right)
	{
		const int32 LeftRow = Table.FindDefinition(Left)->Row;
		const int32 RightRow = Table.FindDefinition(Right)->Row;
		if (LeftRow != RightRow)
		{
			return LeftRow < RightRow;
		}
		return static_cast<uint8>(Left) < static_cast<uint8>(Right);
	});
	return TechnologiesInBranch;
}

FText WorldCampaignTechRules::GetResearchBlockText(const EWorldCampaignTechResearchBlock Block,
                                                   const TArray<ETechnology>& MissingPrerequisites)
{
	switch (Block)
	{
	case EWorldCampaignTechResearchBlock::None:
		return FText::GetEmpty();
	case EWorldCampaignTechResearchBlock::NotInTree:
		return LOCTEXT("NotInTree", "This technology is not part of the campaign tech tree.");
	case EWorldCampaignTechResearchBlock::AlreadyResearched:
		return LOCTEXT("AlreadyResearched", "Already researched.");
	case EWorldCampaignTechResearchBlock::MissingPrerequisite:
		return FText::Format(LOCTEXT("MissingPrerequisite", "Research first: {0}."),
		                     WorldCampaignTechRulesPrivate::JoinTechnologyNames(MissingPrerequisites));
	case EWorldCampaignTechResearchBlock::CannotAfford:
		return LOCTEXT("CannotAfford", "Not enough blueprints or radixite.");
	}
	return FText::GetEmpty();
}

bool WorldCampaignTechRules::GetIsTableConsistent(const FWorldCampaignTechTable& Table, TArray<FString>& OutProblems)
{
	OutProblems.Reset();
	for (const TPair<ETechnology, FWorldCampaignTechDefinition>& Entry : Table.Technologies)
	{
		WorldCampaignTechRulesPrivate::CheckDefinitionConsistency(Table, Entry.Key, Entry.Value, OutProblems);
	}
	return OutProblems.IsEmpty();
}

#undef LOCTEXT_NAMESPACE
