// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/CampaignTech/WorldCampaignTechWheel.h"

#define LOCTEXT_NAMESPACE "WorldCampaignTechWheel"

namespace WorldCampaignTechWheelPrivate
{
	const FName StartNodeId(TEXT("HQ"));

	// Sheet 13 geometry (build_ui_sheets6.py FlexWheel): sector centres -90 + 72 * index, angular offsets in degrees.
	constexpr float FirstSectorCentreDegrees = -90.f;
	constexpr float SectorWidthDegrees = 72.f;
	constexpr float SideBranchOffsetDegrees = 17.f;
	// The third ring pulls the side technologies a little closer to the centre line.
	constexpr float SideTechnologyOffsetDegrees = 17.f * 0.9f;
	constexpr float LateTechnologyOffsetDegrees = -27.f;
	// Bridges sit on the border with the next sector.
	constexpr float BridgeOffsetDegrees = SectorWidthDegrees * 0.5f;

	// Ring 0 is the HQ; the sketch's five rings are 1 to 5.
	constexpr int32 RingFirstPip = 1;
	constexpr int32 RingFirstTechnology = 2;
	constexpr int32 RingSidePips = 3;
	constexpr int32 RingSideTechnologies = 4;
	constexpr int32 RingRim = 5;

	// Placeholder prices: technologies in blueprints rising outward, minor nodes a little radixite.
	constexpr int32 FirstTechnologyBlueprints = 1;
	constexpr int32 SideTechnologyBlueprints = 2;
	constexpr int32 LateTechnologyBlueprints = 3;
	constexpr int32 RimTechnologyBlueprints = 4;
	constexpr int32 MinorNodeRadixite = 40;

	/** One sector of the default wheel: its branch, id prefix and the technologies of slots 1, 3a, 3b, 4a and 4. */
	struct FDefaultSector
	{
		EWorldCampaignTechBranch Branch;
		const TCHAR* Prefix;
		ETechnology FirstTechnology;
		ETechnology SideTechnologyA;
		ETechnology SideTechnologyB;
		ETechnology LateTechnology;
		ETechnology RimTechnology;
	};

	TArray<FDefaultSector> GetDefaultSectors()
	{
		using Branch = EWorldCampaignTechBranch;
		return {
			{
				Branch::Infantry, TEXT("Inf"), ETechnology::Tech_Infantry_Kit, ETechnology::Tech_Pioneer_Entrenching,
				ETechnology::Tech_Sniper_Ghillie, ETechnology::Tech_FlameRange, ETechnology::Tech_FlameDamage
			},
			{
				Branch::LightArmour, TEXT("Lgt"), ETechnology::Tech_LightTank_TrackImprovements,
				ETechnology::Tech_LightCalibreApcr, ETechnology::Tech_LightTank_RadixiteArmor,
				ETechnology::Tech_AutoLoader, ETechnology::Tech_PzJager
			},
			{
				Branch::HeavyArmour, TEXT("Hvy"), ETechnology::Tech_Spalliners, ETechnology::Tech_StereoRangefinder,
				ETechnology::Tech_Assault_ShellHoist, ETechnology::Tech_LongRangeCalibration,
				ETechnology::Tech_Tungsten88
			},
			// The sketch's rim here is "Flak 88 Mode", the toggle of Flak 88 Ground Role, which is no technology.
			{
				Branch::Air, TEXT("Sup"), ETechnology::Tech_AutoCannonDamage, ETechnology::Tech_Flak88_GroundRole,
				ETechnology::Tech_HalfTrack_ArmoredBay, ETechnology::Tech_RepairUpgrade, ETechnology::Tech_NONE
			},
			{
				Branch::Economy, TEXT("Eco"), ETechnology::Tech_ScavengingSpeed, ETechnology::Tech_HarvesterSpeed,
				ETechnology::Tech_ScavengingReward, ETechnology::Tech_HarvesterCapacity, ETechnology::Tech_Barricades
			}
		};
	}

	/** A cross-branch key: the node holding NodeTechnology also needs RequiredTechnology. */
	struct FDefaultKey
	{
		ETechnology NodeTechnology;
		ETechnology RequiredTechnology;
	};

	/** The four cross-branch keys of sheet 13. */
	TArray<FDefaultKey> GetDefaultKeys()
	{
		return {
			{ETechnology::Tech_Tungsten88, ETechnology::Tech_HarvesterCapacity},
			{ETechnology::Tech_Flak88_GroundRole, ETechnology::Tech_LightTank_TrackImprovements},
			{ETechnology::Tech_PzJager, ETechnology::Tech_Spalliners},
			{ETechnology::Tech_FlameDamage, ETechnology::Tech_ScavengingReward}
		};
	}

	FName MakeNodeId(const TCHAR* Prefix, const TCHAR* Slot)
	{
		return FName(*FString::Printf(TEXT("%s_%s"), Prefix, Slot));
	}

	FWorldMapResources MakeBlueprintCost(const int32 Blueprints)
	{
		FWorldMapResources Cost;
		Cost.Blueprints = Blueprints;
		return Cost;
	}

	FWorldMapResources MakeRadixiteCost(const int32 Radixite)
	{
		FWorldMapResources Cost;
		Cost.Radixite = Radixite;
		return Cost;
	}

	FWorldTechWheelNode* FindMutableNode(FWorldTechWheel& Wheel, const FName NodeId)
	{
		return Wheel.Nodes.FindByPredicate([NodeId](const FWorldTechWheelNode& Node)
		{
			return Node.NodeId == NodeId;
		});
	}

	void LinkNodes(FWorldTechWheel& Wheel, const FName FirstNodeId, const FName SecondNodeId)
	{
		FWorldTechWheelNode* const FirstNode = FindMutableNode(Wheel, FirstNodeId);
		FWorldTechWheelNode* const SecondNode = FindMutableNode(Wheel, SecondNodeId);
		if (FirstNode == nullptr || SecondNode == nullptr)
		{
			return;
		}
		FirstNode->Neighbours.AddUnique(SecondNodeId);
		SecondNode->Neighbours.AddUnique(FirstNodeId);
	}

	/**
	 * @brief Adds one node of a sector; a technology slot without a technology becomes a minor pip.
	 * @param Wheel Wheel to add to.
	 * @param Sector Sector the node belongs to.
	 * @param SectorCentreDegrees Screen angle of the sector's centre line.
	 * @param Slot Slot name of the sketch ("1", "3a", "x2").
	 * @param Ring Ring of the slot.
	 * @param OffsetDegrees Angle from the sector's centre line.
	 * @param Technology Technology of the slot, Tech_NONE for a pip.
	 * @param TechnologyKind Technology or Rim.
	 * @param TechnologyBlueprints Price of a technology in this slot.
	 */
	void AddSectorNode(FWorldTechWheel& Wheel, const FDefaultSector& Sector, const float SectorCentreDegrees,
	                   const TCHAR* Slot, const int32 Ring, const float OffsetDegrees, const ETechnology Technology,
	                   const EWorldTechWheelNodeKind TechnologyKind, const int32 TechnologyBlueprints)
	{
		FWorldTechWheelNode Node;
		Node.NodeId = MakeNodeId(Sector.Prefix, Slot);
		Node.Branch = Sector.Branch;
		Node.Ring = Ring;
		Node.AngleDegrees = SectorCentreDegrees + OffsetDegrees;
		Node.Technology = Technology;
		const bool bIsTechnology = Technology != ETechnology::Tech_NONE;
		Node.Kind = bIsTechnology ? TechnologyKind : EWorldTechWheelNodeKind::Minor;
		Node.Cost = bIsTechnology ? MakeBlueprintCost(TechnologyBlueprints) : MakeRadixiteCost(MinorNodeRadixite);
		Wheel.Nodes.Add(Node);
	}

	void AddSectorNodes(FWorldTechWheel& Wheel, const FDefaultSector& Sector, const float CentreDegrees)
	{
		using Kind = EWorldTechWheelNodeKind;
		constexpr ETechnology NoTechnology = ETechnology::Tech_NONE;
		AddSectorNode(Wheel, Sector, CentreDegrees, TEXT("0"), RingFirstPip, 0.f, NoTechnology, Kind::Minor, 0);
		AddSectorNode(Wheel, Sector, CentreDegrees, TEXT("1"), RingFirstTechnology, 0.f, Sector.FirstTechnology,
		              Kind::Technology, FirstTechnologyBlueprints);
		AddSectorNode(Wheel, Sector, CentreDegrees, TEXT("2a"), RingSidePips, -SideBranchOffsetDegrees, NoTechnology,
		              Kind::Minor, 0);
		AddSectorNode(Wheel, Sector, CentreDegrees, TEXT("2"), RingSidePips, 0.f, NoTechnology, Kind::Minor, 0);
		AddSectorNode(Wheel, Sector, CentreDegrees, TEXT("2b"), RingSidePips, SideBranchOffsetDegrees, NoTechnology,
		              Kind::Minor, 0);
		AddSectorNode(Wheel, Sector, CentreDegrees, TEXT("3a"), RingSideTechnologies, -SideTechnologyOffsetDegrees,
		              Sector.SideTechnologyA, Kind::Technology, SideTechnologyBlueprints);
		AddSectorNode(Wheel, Sector, CentreDegrees, TEXT("3"), RingSideTechnologies, 0.f, NoTechnology, Kind::Minor, 0);
		AddSectorNode(Wheel, Sector, CentreDegrees, TEXT("3b"), RingSideTechnologies, SideTechnologyOffsetDegrees,
		              Sector.SideTechnologyB, Kind::Technology, SideTechnologyBlueprints);
		AddSectorNode(Wheel, Sector, CentreDegrees, TEXT("4a"), RingRim, LateTechnologyOffsetDegrees,
		              Sector.LateTechnology, Kind::Technology, LateTechnologyBlueprints);
		AddSectorNode(Wheel, Sector, CentreDegrees, TEXT("4"), RingRim, 0.f, Sector.RimTechnology, Kind::Rim,
		              RimTechnologyBlueprints);
		AddSectorNode(Wheel, Sector, CentreDegrees, TEXT("x2"), RingSidePips, BridgeOffsetDegrees, NoTechnology,
		              Kind::Minor, 0);
		AddSectorNode(Wheel, Sector, CentreDegrees, TEXT("x4"), RingRim, BridgeOffsetDegrees, NoTechnology,
		              Kind::Minor, 0);
	}

	void LinkSectorNodes(FWorldTechWheel& Wheel, const TCHAR* Prefix)
	{
		struct FSlotLink
		{
			const TCHAR* FromSlot;
			const TCHAR* ToSlot;
		};
		static const FSlotLink Links[] = {
			{TEXT("0"), TEXT("1")}, {TEXT("1"), TEXT("2a")}, {TEXT("2a"), TEXT("3a")}, {TEXT("1"), TEXT("2b")},
			{TEXT("2b"), TEXT("3b")}, {TEXT("1"), TEXT("2")}, {TEXT("2"), TEXT("3")}, {TEXT("3a"), TEXT("3")},
			{TEXT("3b"), TEXT("3")}, {TEXT("3"), TEXT("4")}, {TEXT("3a"), TEXT("4a")}
		};
		LinkNodes(Wheel, StartNodeId, MakeNodeId(Prefix, TEXT("0")));
		for (const FSlotLink& Link : Links)
		{
			LinkNodes(Wheel, MakeNodeId(Prefix, Link.FromSlot), MakeNodeId(Prefix, Link.ToSlot));
		}
	}

	/** The bridges: 2b - x2 - next sector's 2a, and 3b - x4 - next sector's 4a. */
	void LinkBridges(FWorldTechWheel& Wheel, const TCHAR* Prefix, const TCHAR* NextPrefix)
	{
		LinkNodes(Wheel, MakeNodeId(Prefix, TEXT("2b")), MakeNodeId(Prefix, TEXT("x2")));
		LinkNodes(Wheel, MakeNodeId(Prefix, TEXT("x2")), MakeNodeId(NextPrefix, TEXT("2a")));
		LinkNodes(Wheel, MakeNodeId(Prefix, TEXT("3b")), MakeNodeId(Prefix, TEXT("x4")));
		LinkNodes(Wheel, MakeNodeId(Prefix, TEXT("x4")), MakeNodeId(NextPrefix, TEXT("4a")));
	}

	void ApplyDefaultKeys(FWorldTechWheel& Wheel)
	{
		const TArray<FDefaultKey> Keys = GetDefaultKeys();
		for (const FDefaultKey& Key : Keys)
		{
			FWorldTechWheelNode* const Node = Wheel.Nodes.FindByPredicate([&Key](const FWorldTechWheelNode& Candidate)
			{
				return Candidate.GetIsTechnologyNode() && Candidate.Technology == Key.NodeTechnology;
			});
			if (Node != nullptr)
			{
				Node->RequiredTechnology = Key.RequiredTechnology;
			}
		}
	}

	void CheckNodeLinks(const FWorldTechWheel& Wheel, const FWorldTechWheelNode& Node, TArray<FString>& OutProblems)
	{
		const FString NodeName = Node.NodeId.ToString();
		for (const FName NeighbourId : Node.Neighbours)
		{
			if (NeighbourId == Node.NodeId)
			{
				OutProblems.Add(NodeName + TEXT(" links to itself."));
				continue;
			}
			const FWorldTechWheelNode* const Neighbour = Wheel.FindNode(NeighbourId);
			if (Neighbour == nullptr)
			{
				OutProblems.Add(NodeName + TEXT(" links to ") + NeighbourId.ToString() +
					TEXT(", which is not on the wheel."));
				continue;
			}
			if (not Neighbour->Neighbours.Contains(Node.NodeId))
			{
				OutProblems.Add(NodeName + TEXT(" links to ") + NeighbourId.ToString() +
					TEXT(", which does not link back."));
			}
		}
	}

	void CheckNodeContent(const FWorldTechWheel& Wheel, const FWorldTechWheelNode& Node, TArray<FString>& OutProblems)
	{
		const FString NodeName = Node.NodeId.ToString();
		const bool bHasTechnology = Node.Technology != ETechnology::Tech_NONE;
		if (Node.GetIsTechnologyNode() && not bHasTechnology)
		{
			OutProblems.Add(NodeName + TEXT(" is a technology node without a technology."));
		}
		if (not Node.GetIsTechnologyNode() && bHasTechnology)
		{
			OutProblems.Add(NodeName + TEXT(" holds a technology but is not a technology node."));
		}
		if (Node.Kind != EWorldTechWheelNodeKind::Start && Node.Branch == EWorldCampaignTechBranch::None)
		{
			OutProblems.Add(NodeName + TEXT(" has no branch."));
		}
		if (Node.RequiredTechnology == ETechnology::Tech_NONE)
		{
			return;
		}
		if (Node.RequiredTechnology == Node.Technology)
		{
			OutProblems.Add(NodeName + TEXT(" needs its own technology."));
		}
		if (Wheel.FindNodeByTechnology(Node.RequiredTechnology) == nullptr)
		{
			OutProblems.Add(NodeName + TEXT(" needs ") + Global_GetTechAsString(Node.RequiredTechnology) +
				TEXT(", which is not on the wheel."));
		}
	}

	void CheckIdsAndTechnologies(const FWorldTechWheel& Wheel, TArray<FString>& OutProblems)
	{
		TSet<FName> SeenIds;
		TSet<ETechnology> SeenTechnologies;
		int32 StartNodeCount = 0;
		for (const FWorldTechWheelNode& Node : Wheel.Nodes)
		{
			if (Node.NodeId.IsNone())
			{
				OutProblems.Add(TEXT("A node has no id."));
			}
			else if (SeenIds.Contains(Node.NodeId))
			{
				OutProblems.Add(Node.NodeId.ToString() + TEXT(" is used by more than one node."));
			}
			SeenIds.Add(Node.NodeId);
			StartNodeCount += Node.Kind == EWorldTechWheelNodeKind::Start ? 1 : 0;
			if (Node.Technology == ETechnology::Tech_NONE)
			{
				continue;
			}
			if (SeenTechnologies.Contains(Node.Technology))
			{
				OutProblems.Add(Global_GetTechAsString(Node.Technology) + TEXT(" is on the wheel more than once."));
			}
			SeenTechnologies.Add(Node.Technology);
		}
		if (StartNodeCount != 1)
		{
			OutProblems.Add(FString::Printf(TEXT("The wheel has %d start nodes instead of one."), StartNodeCount));
		}
	}

	void VisitNeighbours(const FWorldTechWheelNode& Node, TSet<FName>& InOutVisited, TArray<FName>& InOutFrontier)
	{
		for (const FName NeighbourId : Node.Neighbours)
		{
			if (InOutVisited.Contains(NeighbourId))
			{
				continue;
			}
			InOutVisited.Add(NeighbourId);
			InOutFrontier.Add(NeighbourId);
		}
	}

	void CheckEveryNodeReachable(const FWorldTechWheel& Wheel, TArray<FString>& OutProblems)
	{
		const FName StartId = Wheel.GetStartNodeId();
		if (StartId.IsNone())
		{
			return;
		}
		TSet<FName> Visited;
		TArray<FName> Frontier = {StartId};
		Visited.Add(StartId);
		while (Frontier.Num() > 0)
		{
			const FWorldTechWheelNode* const Node = Wheel.FindNode(Frontier.Pop());
			if (Node == nullptr)
			{
				continue;
			}
			VisitNeighbours(*Node, Visited, Frontier);
		}
		for (const FWorldTechWheelNode& Node : Wheel.Nodes)
		{
			if (not Visited.Contains(Node.NodeId))
			{
				OutProblems.Add(Node.NodeId.ToString() + TEXT(" cannot be reached from the start."));
			}
		}
	}
}

FWorldTechWheel WorldCampaignTechWheel::MakeDefaultWheel()
{
	using namespace WorldCampaignTechWheelPrivate;
	FWorldTechWheel Wheel;
	FWorldTechWheelNode StartNode;
	StartNode.NodeId = StartNodeId;
	StartNode.Kind = EWorldTechWheelNodeKind::Start;
	StartNode.Ring = 0;
	Wheel.Nodes.Add(StartNode);

	const TArray<FDefaultSector> Sectors = GetDefaultSectors();
	for (int32 SectorIndex = 0; SectorIndex < Sectors.Num(); ++SectorIndex)
	{
		const float CentreDegrees = FirstSectorCentreDegrees + SectorWidthDegrees * SectorIndex;
		AddSectorNodes(Wheel, Sectors[SectorIndex], CentreDegrees);
	}
	for (int32 SectorIndex = 0; SectorIndex < Sectors.Num(); ++SectorIndex)
	{
		LinkSectorNodes(Wheel, Sectors[SectorIndex].Prefix);
		const FDefaultSector& NextSector = Sectors[(SectorIndex + 1) % Sectors.Num()];
		LinkBridges(Wheel, Sectors[SectorIndex].Prefix, NextSector.Prefix);
	}
	ApplyDefaultKeys(Wheel);
	return Wheel;
}

TArray<EWorldCampaignTechBranch> WorldCampaignTechWheel::GetSectorBranches()
{
	using Branch = EWorldCampaignTechBranch;
	return {Branch::Infantry, Branch::LightArmour, Branch::HeavyArmour, Branch::Air, Branch::Economy};
}

bool WorldCampaignTechWheel::GetSectorCentreDegrees(const EWorldCampaignTechBranch Branch, float& OutCentreDegrees)
{
	using namespace WorldCampaignTechWheelPrivate;
	const int32 SectorIndex = GetSectorBranches().IndexOfByKey(Branch);
	if (SectorIndex == INDEX_NONE)
	{
		return false;
	}
	OutCentreDegrees = FirstSectorCentreDegrees + SectorWidthDegrees * SectorIndex;
	return true;
}

float WorldCampaignTechWheel::GetSectorHalfWidthDegrees()
{
	return WorldCampaignTechWheelPrivate::SectorWidthDegrees * 0.5f;
}

bool WorldCampaignTechWheel::GetIsNodeBought(const FWorldTechWheel& Wheel, const FName NodeId,
                                             const TArray<FName>& BoughtNodeIds)
{
	const FWorldTechWheelNode* const Node = Wheel.FindNode(NodeId);
	if (Node == nullptr)
	{
		return false;
	}
	return Node->Kind == EWorldTechWheelNodeKind::Start || BoughtNodeIds.Contains(NodeId);
}

EWorldTechWheelNodeState WorldCampaignTechWheel::GetNodeState(const FWorldTechWheel& Wheel, const FName NodeId,
                                                              const TArray<FName>& BoughtNodeIds)
{
	const FWorldTechWheelNode* const Node = Wheel.FindNode(NodeId);
	if (Node == nullptr)
	{
		return EWorldTechWheelNodeState::Locked;
	}
	if (GetIsNodeBought(Wheel, NodeId, BoughtNodeIds))
	{
		return EWorldTechWheelNodeState::Bought;
	}
	for (const FName NeighbourId : Node->Neighbours)
	{
		if (GetIsNodeBought(Wheel, NeighbourId, BoughtNodeIds))
		{
			return EWorldTechWheelNodeState::Reachable;
		}
	}
	return EWorldTechWheelNodeState::Locked;
}

EWorldTechWheelBuyBlock WorldCampaignTechWheel::GetNodeBuyBlock(const FWorldTechWheel& Wheel, const FName NodeId,
                                                                const TArray<FName>& BoughtNodeIds,
                                                                const TArray<ETechnology>& ResearchedTechnologies,
                                                                const FWorldMapResources& Resources)
{
	const FWorldTechWheelNode* const Node = Wheel.FindNode(NodeId);
	if (Node == nullptr)
	{
		return EWorldTechWheelBuyBlock::UnknownNode;
	}
	const EWorldTechWheelNodeState State = GetNodeState(Wheel, NodeId, BoughtNodeIds);
	if (State == EWorldTechWheelNodeState::Bought)
	{
		return EWorldTechWheelBuyBlock::AlreadyBought;
	}
	if (State == EWorldTechWheelNodeState::Locked)
	{
		return EWorldTechWheelBuyBlock::NotReachable;
	}
	if (Node->RequiredTechnology != ETechnology::Tech_NONE && not ResearchedTechnologies.Contains(
		Node->RequiredTechnology))
	{
		return EWorldTechWheelBuyBlock::MissingRequiredTechnology;
	}
	if (not Resources.GetCanAfford(Node->Cost))
	{
		return EWorldTechWheelBuyBlock::CannotAfford;
	}
	return EWorldTechWheelBuyBlock::None;
}

FText WorldCampaignTechWheel::GetNodeBuyBlockText(const EWorldTechWheelBuyBlock Block,
                                                  const FWorldTechWheelNode* Node)
{
	switch (Block)
	{
	case EWorldTechWheelBuyBlock::None:
		return FText::GetEmpty();
	case EWorldTechWheelBuyBlock::AlreadyBought:
		return LOCTEXT("AlreadyBought", "Already researched.");
	case EWorldTechWheelBuyBlock::NotReachable:
		return LOCTEXT("NotReachable", "Not reachable yet.");
	case EWorldTechWheelBuyBlock::MissingRequiredTechnology:
		return FText::Format(LOCTEXT("MissingKey", "Research {0} first."),
		                     Node != nullptr ? GetTechnologyDisplayName(Node->RequiredTechnology) : FText::GetEmpty());
	case EWorldTechWheelBuyBlock::CannotAfford:
		return LOCTEXT("CannotAfford", "Not enough blueprints or radixite.");
	case EWorldTechWheelBuyBlock::UnknownNode:
		return LOCTEXT("UnknownNode", "This node is not on the tech wheel.");
	}
	return FText::GetEmpty();
}

FName WorldCampaignTechWheel::GetBoughtNeighbour(const FWorldTechWheel& Wheel, const FName NodeId,
                                                 const TArray<FName>& BoughtNodeIds)
{
	const FWorldTechWheelNode* const Node = Wheel.FindNode(NodeId);
	if (Node == nullptr)
	{
		return NAME_None;
	}
	FName FirstBoughtNeighbour = NAME_None;
	for (const FName NeighbourId : Node->Neighbours)
	{
		if (not GetIsNodeBought(Wheel, NeighbourId, BoughtNodeIds))
		{
			continue;
		}
		const FWorldTechWheelNode* const Neighbour = Wheel.FindNode(NeighbourId);
		if (Neighbour != nullptr && Neighbour->GetIsTechnologyNode())
		{
			return NeighbourId;
		}
		if (FirstBoughtNeighbour.IsNone())
		{
			FirstBoughtNeighbour = NeighbourId;
		}
	}
	return FirstBoughtNeighbour;
}

FText WorldCampaignTechWheel::GetBranchDisplayName(const EWorldCampaignTechBranch Branch)
{
	switch (Branch)
	{
	case EWorldCampaignTechBranch::Infantry:
		return LOCTEXT("BranchInfantry", "INFANTRY");
	case EWorldCampaignTechBranch::LightArmour:
		return LOCTEXT("BranchLightArmour", "LIGHT ARMOUR");
	case EWorldCampaignTechBranch::HeavyArmour:
		return LOCTEXT("BranchHeavyArmour", "HEAVY ARMOUR");
	case EWorldCampaignTechBranch::Air:
		return LOCTEXT("BranchSupport", "SUPPORT");
	case EWorldCampaignTechBranch::Economy:
		return LOCTEXT("BranchEconomy", "ECONOMY");
	case EWorldCampaignTechBranch::None:
		break;
	}
	return FText::GetEmpty();
}

FText WorldCampaignTechWheel::GetTechnologyDisplayName(const ETechnology Technology)
{
	const FWorldTechWheelTechPresentation* const Presentation =
		UWorldCampaignTechSettings::GetWheelPresentation().FindPresentation(Technology);
	if (Presentation != nullptr && not Presentation->DisplayName.IsEmpty())
	{
		return Presentation->DisplayName;
	}
	return FText::FromString(Global_GetTechDisplayName(Technology));
}

bool WorldCampaignTechWheel::GetIsWheelConsistent(const FWorldTechWheel& Wheel, TArray<FString>& OutProblems)
{
	using namespace WorldCampaignTechWheelPrivate;
	OutProblems.Reset();
	CheckIdsAndTechnologies(Wheel, OutProblems);
	for (const FWorldTechWheelNode& Node : Wheel.Nodes)
	{
		CheckNodeLinks(Wheel, Node, OutProblems);
		CheckNodeContent(Wheel, Node, OutProblems);
	}
	CheckEveryNodeReachable(Wheel, OutProblems);
	return OutProblems.IsEmpty();
}

#undef LOCTEXT_NAMESPACE
