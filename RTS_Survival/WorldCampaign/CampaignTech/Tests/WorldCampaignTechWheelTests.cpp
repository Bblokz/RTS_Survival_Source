// Copyright (C) Bas Blokzijl - All rights reserved.

#include "Misc/Build.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Misc/AutomationTest.h"
#include "RTS_Survival/WorldCampaign/CampaignTech/WorldCampaignTechWheel.h"

namespace WorldCampaignTechWheelTestData
{
	// Tech_NONE is value 0; the wheel must hold every value after it.
	constexpr uint8 FirstTechnologyValue = 1;
	constexpr uint8 LastTechnologyValue = static_cast<uint8>(ETechnology::Tech_Pioneer_Entrenching);
	// HQ plus 12 nodes in each of the five sectors (sheet 13: "61 nodes").
	constexpr int32 DefaultWheelNodeCount = 61;
	constexpr int32 DefaultKeyCount = 4;
	constexpr int32 PlentyOfResources = 100000;

	FWorldMapResources MakeWheelResources(const int32 Radixite, const int32 Blueprints)
	{
		FWorldMapResources Resources;
		Resources.Radixite = Radixite;
		Resources.Blueprints = Blueprints;
		return Resources;
	}

	FWorldTechWheelNode MakeWheelNode(const FName NodeId, const EWorldTechWheelNodeKind Kind,
	                                  const ETechnology Technology, const TArray<FName>& Neighbours)
	{
		FWorldTechWheelNode Node;
		Node.NodeId = NodeId;
		Node.Kind = Kind;
		Node.Branch = Kind == EWorldTechWheelNodeKind::Start
			              ? EWorldCampaignTechBranch::None
			              : EWorldCampaignTechBranch::Infantry;
		Node.Technology = Technology;
		Node.Neighbours = Neighbours;
		Node.Cost = Technology == ETechnology::Tech_NONE ? MakeWheelResources(40, 0) : MakeWheelResources(0, 2);
		return Node;
	}

	/** Start - Pip - Kit - Ghillie in a line; Ghillie needs Entrenching Tools, which sits on a side pip. */
	FWorldTechWheel MakeSmallWheel()
	{
		using Kind = EWorldTechWheelNodeKind;
		FWorldTechWheel Wheel;
		Wheel.Nodes.Add(MakeWheelNode(TEXT("Start"), Kind::Start, ETechnology::Tech_NONE, {TEXT("Pip")}));
		Wheel.Nodes.Add(MakeWheelNode(TEXT("Pip"), Kind::Minor, ETechnology::Tech_NONE,
		                              {TEXT("Start"), TEXT("Kit"), TEXT("Tools")}));
		Wheel.Nodes.Add(MakeWheelNode(TEXT("Kit"), Kind::Technology, ETechnology::Tech_Infantry_Kit,
		                              {TEXT("Pip"), TEXT("Ghillie")}));
		Wheel.Nodes.Add(MakeWheelNode(TEXT("Tools"), Kind::Technology, ETechnology::Tech_Pioneer_Entrenching,
		                              {TEXT("Pip")}));
		FWorldTechWheelNode Ghillie = MakeWheelNode(TEXT("Ghillie"), Kind::Rim, ETechnology::Tech_Sniper_Ghillie,
		                                            {TEXT("Kit")});
		Ghillie.RequiredTechnology = ETechnology::Tech_Pioneer_Entrenching;
		Wheel.Nodes.Add(Ghillie);
		return Wheel;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldTechWheelDefaultWheelTest, "RTS.WorldCampaign.TechWheel.DefaultWheelIsConsistent",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldTechWheelDefaultWheelTest::RunTest(const FString& Parameters)
{
	using namespace WorldCampaignTechWheelTestData;
	const FWorldTechWheel Wheel = WorldCampaignTechWheel::MakeDefaultWheel();

	TArray<FString> Problems;
	const bool bIsConsistent = WorldCampaignTechWheel::GetIsWheelConsistent(Wheel, Problems);
	for (const FString& Problem : Problems)
	{
		AddError(Problem);
	}
	TestTrue(TEXT("Default wheel is consistent"), bIsConsistent);
	TestEqual(TEXT("Default wheel has the 61 nodes of sheet 13"), Wheel.Nodes.Num(), DefaultWheelNodeCount);

	for (uint8 Value = FirstTechnologyValue; Value <= LastTechnologyValue; ++Value)
	{
		const ETechnology Technology = static_cast<ETechnology>(Value);
		TestNotNull(FString::Printf(TEXT("%s is on the wheel"), *Global_GetTechAsString(Technology)),
		            Wheel.FindNodeByTechnology(Technology));
	}
	TestEqual(TEXT("Every technology node holds one of the 24 technologies"), Wheel.GetTechnologyNodeCount(),
	          static_cast<int32>(LastTechnologyValue));

	int32 KeyCount = 0;
	for (const FWorldTechWheelNode& Node : Wheel.Nodes)
	{
		KeyCount += Node.RequiredTechnology != ETechnology::Tech_NONE ? 1 : 0;
		if (Node.GetIsTechnologyNode())
		{
			TestTrue(TEXT("A technology costs blueprints"), Node.Cost.Blueprints > 0);
		}
	}
	TestEqual(TEXT("The four keys of sheet 13 are set"), KeyCount, DefaultKeyCount);

	const FWorldTechWheelNode* const Tungsten = Wheel.FindNodeByTechnology(ETechnology::Tech_Tungsten88);
	if (TestNotNull(TEXT("Tungsten 8.8 is on the wheel"), Tungsten))
	{
		TestEqual(TEXT("Tungsten 8.8 is the heavy armour rim"), Tungsten->Kind, EWorldTechWheelNodeKind::Rim);
		TestEqual(TEXT("Tungsten 8.8 needs Harvester Capacity"), Tungsten->RequiredTechnology,
		          ETechnology::Tech_HarvesterCapacity);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldTechWheelNodeStateTest, "RTS.WorldCampaign.TechWheel.NodeState",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldTechWheelNodeStateTest::RunTest(const FString& Parameters)
{
	using namespace WorldCampaignTechWheelTestData;
	const FWorldTechWheel Wheel = MakeSmallWheel();
	const TArray<FName> NothingBought;

	TestEqual(TEXT("The start counts as bought without being saved"),
	          WorldCampaignTechWheel::GetNodeState(Wheel, TEXT("Start"), NothingBought),
	          EWorldTechWheelNodeState::Bought);
	TestEqual(TEXT("A neighbour of the start is reachable"),
	          WorldCampaignTechWheel::GetNodeState(Wheel, TEXT("Pip"), NothingBought),
	          EWorldTechWheelNodeState::Reachable);
	TestEqual(TEXT("Two steps out is locked"),
	          WorldCampaignTechWheel::GetNodeState(Wheel, TEXT("Kit"), NothingBought),
	          EWorldTechWheelNodeState::Locked);
	TestEqual(TEXT("A bought pip opens its neighbours"),
	          WorldCampaignTechWheel::GetNodeState(Wheel, TEXT("Kit"), {TEXT("Pip")}),
	          EWorldTechWheelNodeState::Reachable);
	TestEqual(TEXT("An unknown node is locked"),
	          WorldCampaignTechWheel::GetNodeState(Wheel, TEXT("Nowhere"), NothingBought),
	          EWorldTechWheelNodeState::Locked);
	TestTrue(TEXT("Reached through the bought technology"),
	         WorldCampaignTechWheel::GetBoughtNeighbour(Wheel, TEXT("Ghillie"), {TEXT("Pip"), TEXT("Kit")}) ==
	         FName(TEXT("Kit")));
	TestTrue(TEXT("No bought neighbour gives none"),
	         WorldCampaignTechWheel::GetBoughtNeighbour(Wheel, TEXT("Ghillie"), NothingBought).IsNone());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldTechWheelBuyBlockTest, "RTS.WorldCampaign.TechWheel.BuyBlock",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldTechWheelBuyBlockTest::RunTest(const FString& Parameters)
{
	using namespace WorldCampaignTechWheelTestData;
	using WorldCampaignTechWheel::GetNodeBuyBlock;
	const FWorldTechWheel Wheel = MakeSmallWheel();
	const FWorldMapResources Rich = MakeWheelResources(PlentyOfResources, PlentyOfResources);
	const TArray<ETechnology> NothingResearched;
	const TArray<FName> PathToGhillie = {TEXT("Pip"), TEXT("Kit")};

	TestEqual(TEXT("An unknown node"), GetNodeBuyBlock(Wheel, TEXT("Nowhere"), {}, NothingResearched, Rich),
	          EWorldTechWheelBuyBlock::UnknownNode);
	TestEqual(TEXT("The start is never bought"), GetNodeBuyBlock(Wheel, TEXT("Start"), {}, NothingResearched, Rich),
	          EWorldTechWheelBuyBlock::AlreadyBought);
	TestEqual(TEXT("A bought node is not bought twice"),
	          GetNodeBuyBlock(Wheel, TEXT("Pip"), {TEXT("Pip")}, NothingResearched, Rich),
	          EWorldTechWheelBuyBlock::AlreadyBought);
	TestEqual(TEXT("A node off the path"), GetNodeBuyBlock(Wheel, TEXT("Kit"), {}, NothingResearched, Rich),
	          EWorldTechWheelBuyBlock::NotReachable);
	TestEqual(TEXT("A reachable pip with radixite"), GetNodeBuyBlock(Wheel, TEXT("Pip"), {}, NothingResearched, Rich),
	          EWorldTechWheelBuyBlock::None);
	TestEqual(TEXT("The key is missing"),
	          GetNodeBuyBlock(Wheel, TEXT("Ghillie"), PathToGhillie, NothingResearched, Rich),
	          EWorldTechWheelBuyBlock::MissingRequiredTechnology);
	TestEqual(TEXT("With the key researched"),
	          GetNodeBuyBlock(Wheel, TEXT("Ghillie"), PathToGhillie, {ETechnology::Tech_Pioneer_Entrenching}, Rich),
	          EWorldTechWheelBuyBlock::None);
	TestEqual(TEXT("Too few blueprints"),
	          GetNodeBuyBlock(Wheel, TEXT("Kit"), {TEXT("Pip")}, NothingResearched,
	                          MakeWheelResources(PlentyOfResources, 1)),
	          EWorldTechWheelBuyBlock::CannotAfford);
	TestEqual(TEXT("The exact price is enough"),
	          GetNodeBuyBlock(Wheel, TEXT("Kit"), {TEXT("Pip")}, NothingResearched, MakeWheelResources(0, 2)),
	          EWorldTechWheelBuyBlock::None);
	TestFalse(TEXT("The key text names the reason"),
	          WorldCampaignTechWheel::GetNodeBuyBlockText(EWorldTechWheelBuyBlock::MissingRequiredTechnology,
	                                                      Wheel.FindNode(TEXT("Ghillie"))).IsEmpty());
	TestTrue(TEXT("No block has no text"),
	         WorldCampaignTechWheel::GetNodeBuyBlockText(EWorldTechWheelBuyBlock::None, nullptr).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldTechWheelConsistencyTest, "RTS.WorldCampaign.TechWheel.ConsistencyCheck",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldTechWheelConsistencyTest::RunTest(const FString& Parameters)
{
	using namespace WorldCampaignTechWheelTestData;
	TArray<FString> Problems;
	TestTrue(TEXT("The small wheel is consistent"),
	         WorldCampaignTechWheel::GetIsWheelConsistent(MakeSmallWheel(), Problems));

	FWorldTechWheel OneWayLink = MakeSmallWheel();
	OneWayLink.Nodes[3].Neighbours.Reset();
	TestFalse(TEXT("A one-way link is found"), WorldCampaignTechWheel::GetIsWheelConsistent(OneWayLink, Problems));

	FWorldTechWheel DuplicateId = MakeSmallWheel();
	DuplicateId.Nodes.Add(MakeWheelNode(TEXT("Pip"), EWorldTechWheelNodeKind::Minor, ETechnology::Tech_NONE, {}));
	TestFalse(TEXT("A duplicate id is found"), WorldCampaignTechWheel::GetIsWheelConsistent(DuplicateId, Problems));

	FWorldTechWheel DuplicateTechnology = MakeSmallWheel();
	DuplicateTechnology.Nodes[3].Technology = ETechnology::Tech_Infantry_Kit;
	TestFalse(TEXT("A technology placed twice is found"),
	          WorldCampaignTechWheel::GetIsWheelConsistent(DuplicateTechnology, Problems));

	FWorldTechWheel MissingKey = MakeSmallWheel();
	MissingKey.Nodes[4].RequiredTechnology = ETechnology::Tech_Tungsten88;
	TestFalse(TEXT("A key that is not on the wheel is found"),
	          WorldCampaignTechWheel::GetIsWheelConsistent(MissingKey, Problems));

	FWorldTechWheel EmptyTechnologyNode = MakeSmallWheel();
	EmptyTechnologyNode.Nodes[2].Technology = ETechnology::Tech_NONE;
	TestFalse(TEXT("A technology node without a technology is found"),
	          WorldCampaignTechWheel::GetIsWheelConsistent(EmptyTechnologyNode, Problems));
	return true;
}

#endif
