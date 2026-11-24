// Copyright (C) Bas Blokzijl - All rights reserved.

#include "Misc/Build.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Misc/AutomationTest.h"
#include "RTS_Survival/WorldCampaign/CampaignTech/WorldCampaignTechRules.h"

namespace WorldCampaignTechTestData
{
	// Tech_NONE is value 0; the tree must hold every value after it.
	constexpr uint8 FirstTechnologyValue = 1;
	constexpr uint8 LastTechnologyValue = static_cast<uint8>(ETechnology::Tech_Pioneer_Entrenching);
	constexpr int32 PlentyOfResources = 100000;

	FWorldMapResources MakeResources(const int32 Radixite, const int32 Blueprints)
	{
		FWorldMapResources Resources;
		Resources.Radixite = Radixite;
		Resources.Blueprints = Blueprints;
		return Resources;
	}

	/** Root A (row 0), B needs A (row 1), all in the infantry branch. */
	FWorldCampaignTechTable MakeSmallTable()
	{
		FWorldCampaignTechTable Table;
		Table.Technologies.Reset();

		FWorldCampaignTechDefinition Root;
		Root.Branch = EWorldCampaignTechBranch::Infantry;
		Root.Row = 0;
		Root.Cost = MakeResources(50, 1);
		Table.Technologies.Add(ETechnology::Tech_Infantry_Kit, Root);

		FWorldCampaignTechDefinition Child;
		Child.Branch = EWorldCampaignTechBranch::Infantry;
		Child.Row = 1;
		Child.Cost = MakeResources(100, 2);
		Child.Prerequisites = {ETechnology::Tech_Infantry_Kit};
		Table.Technologies.Add(ETechnology::Tech_Sniper_Ghillie, Child);
		return Table;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCampaignTechDefaultTableTest, "RTS.WorldCampaign.Tech.DefaultTableIsComplete",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldCampaignTechDefaultTableTest::RunTest(const FString& Parameters)
{
	using namespace WorldCampaignTechTestData;
	const FWorldCampaignTechTable Table;

	for (uint8 Value = FirstTechnologyValue; Value <= LastTechnologyValue; ++Value)
	{
		const ETechnology Technology = static_cast<ETechnology>(Value);
		TestNotNull(FString::Printf(TEXT("%s is in the tree"), *Global_GetTechAsString(Technology)),
		            Table.FindDefinition(Technology));
	}
	TestNull(TEXT("Tech_NONE is not in the tree"), Table.FindDefinition(ETechnology::Tech_NONE));

	TArray<FString> Problems;
	const bool bIsConsistent = WorldCampaignTechRules::GetIsTableConsistent(Table, Problems);
	for (const FString& Problem : Problems)
	{
		AddError(Problem);
	}
	TestTrue(TEXT("Default table is consistent"), bIsConsistent);

	for (const TPair<ETechnology, FWorldCampaignTechDefinition>& Entry : Table.Technologies)
	{
		for (const ETechnology Prerequisite : Entry.Value.Prerequisites)
		{
			const FWorldCampaignTechDefinition* PrerequisiteDefinition = Table.FindDefinition(Prerequisite);
			if (PrerequisiteDefinition == nullptr)
			{
				continue;
			}
			TestTrue(TEXT("A tech costs at least as many blueprints as its prerequisite"),
			         Entry.Value.Cost.Blueprints >= PrerequisiteDefinition->Cost.Blueprints);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCampaignTechResearchBlockTest, "RTS.WorldCampaign.Tech.ResearchBlock",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldCampaignTechResearchBlockTest::RunTest(const FString& Parameters)
{
	using namespace WorldCampaignTechTestData;
	const FWorldCampaignTechTable Table = MakeSmallTable();
	const FWorldMapResources Rich = MakeResources(PlentyOfResources, PlentyOfResources);

	TestEqual(TEXT("Tech_NONE is never buyable"),
	          WorldCampaignTechRules::GetResearchBlock(Table, ETechnology::Tech_NONE, {}, Rich),
	          EWorldCampaignTechResearchBlock::NotInTree);
	TestEqual(TEXT("A tech outside the tree is not buyable"),
	          WorldCampaignTechRules::GetResearchBlock(Table, ETechnology::Tech_Barricades, {}, Rich),
	          EWorldCampaignTechResearchBlock::NotInTree);
	TestEqual(TEXT("A root tech with enough resources is buyable"),
	          WorldCampaignTechRules::GetResearchBlock(Table, ETechnology::Tech_Infantry_Kit, {}, Rich),
	          EWorldCampaignTechResearchBlock::None);
	TestEqual(TEXT("An owned tech is not bought twice"),
	          WorldCampaignTechRules::GetResearchBlock(Table, ETechnology::Tech_Infantry_Kit,
	                                                   {ETechnology::Tech_Infantry_Kit}, Rich),
	          EWorldCampaignTechResearchBlock::AlreadyResearched);
	TestEqual(TEXT("A child needs its prerequisite"),
	          WorldCampaignTechRules::GetResearchBlock(Table, ETechnology::Tech_Sniper_Ghillie, {}, Rich),
	          EWorldCampaignTechResearchBlock::MissingPrerequisite);
	TestEqual(TEXT("A child with its prerequisite is buyable"),
	          WorldCampaignTechRules::GetResearchBlock(Table, ETechnology::Tech_Sniper_Ghillie,
	                                                   {ETechnology::Tech_Infantry_Kit}, Rich),
	          EWorldCampaignTechResearchBlock::None);
	TestEqual(TEXT("Missing blueprints block the purchase"),
	          WorldCampaignTechRules::GetResearchBlock(Table, ETechnology::Tech_Infantry_Kit, {},
	                                                   MakeResources(PlentyOfResources, 0)),
	          EWorldCampaignTechResearchBlock::CannotAfford);
	TestEqual(TEXT("Missing radixite block the purchase"),
	          WorldCampaignTechRules::GetResearchBlock(Table, ETechnology::Tech_Infantry_Kit, {},
	                                                   MakeResources(0, PlentyOfResources)),
	          EWorldCampaignTechResearchBlock::CannotAfford);
	TestEqual(TEXT("The exact price is enough"),
	          WorldCampaignTechRules::GetResearchBlock(Table, ETechnology::Tech_Infantry_Kit, {},
	                                                   MakeResources(50, 1)),
	          EWorldCampaignTechResearchBlock::None);

	const TArray<ETechnology> Missing = WorldCampaignTechRules::GetMissingPrerequisites(
		Table, ETechnology::Tech_Sniper_Ghillie, {});
	TestEqual(TEXT("One prerequisite is missing"), Missing.Num(), 1);
	TestFalse(TEXT("The blocked text names the reason"),
	          WorldCampaignTechRules::GetResearchBlockText(EWorldCampaignTechResearchBlock::MissingPrerequisite,
	                                                       Missing).IsEmpty());
	TestTrue(TEXT("No block has no text"),
	         WorldCampaignTechRules::GetResearchBlockText(EWorldCampaignTechResearchBlock::None, {}).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldCampaignTechBranchOrderTest, "RTS.WorldCampaign.Tech.BranchOrder",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldCampaignTechBranchOrderTest::RunTest(const FString& Parameters)
{
	const FWorldCampaignTechTable Table;
	int32 TechnologiesInAllBranches = 0;
	const TArray<EWorldCampaignTechBranch> Branches = {
		EWorldCampaignTechBranch::Infantry, EWorldCampaignTechBranch::LightArmour,
		EWorldCampaignTechBranch::HeavyArmour, EWorldCampaignTechBranch::Air, EWorldCampaignTechBranch::Economy
	};
	for (const EWorldCampaignTechBranch Branch : Branches)
	{
		const TArray<ETechnology> InBranch = WorldCampaignTechRules::GetTechnologiesInBranch(Table, Branch);
		TestTrue(TEXT("Every branch has a technology"), InBranch.Num() > 0);
		TechnologiesInAllBranches += InBranch.Num();
		for (int32 Index = 1; Index < InBranch.Num(); ++Index)
		{
			TestTrue(TEXT("Rows never go down within a branch"),
			         Table.FindDefinition(InBranch[Index - 1])->Row <= Table.FindDefinition(InBranch[Index])->Row);
		}
	}
	TestEqual(TEXT("Every technology is in exactly one listed branch"), TechnologiesInAllBranches,
	          Table.Technologies.Num());
	TestTrue(TEXT("The None branch is empty"),
	         WorldCampaignTechRules::GetTechnologiesInBranch(Table, EWorldCampaignTechBranch::None).IsEmpty());

	const TArray<ETechnology> Heavy = WorldCampaignTechRules::GetTechnologiesInBranch(
		Table, EWorldCampaignTechBranch::HeavyArmour);
	if (Heavy.Num() > 0)
	{
		TestEqual(TEXT("Spall liners root the heavy armour branch"), Heavy[0], ETechnology::Tech_Spalliners);
	}
	return true;
}

#endif
