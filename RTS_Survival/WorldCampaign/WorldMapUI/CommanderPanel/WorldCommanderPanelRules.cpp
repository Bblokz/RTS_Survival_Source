// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldMapUI/CommanderPanel/WorldCommanderPanelRules.h"

namespace WorldCommanderPanelRulesPrivate
{
	FText MakeWrongPickReason(const EWorldCommanderAbilityTargetKind Kind)
	{
		return FText::Format(NSLOCTEXT("WorldCommanderPanel", "WrongPick", "That is not {0}."),
		                     WorldCommanderPanelRules::GetTargetKindNoun(Kind));
	}

	bool BuildAnchorTarget(const FWorldCommanderPickCandidate& Candidate, FWorldCommanderMapAbilityTarget& OutTarget)
	{
		if (Candidate.Thing != EWorldCommanderPickedThing::OperationTarget || not Candidate.AnchorKey.IsValid())
		{
			return false;
		}
		OutTarget.AnchorKey = Candidate.AnchorKey;
		return true;
	}

	bool BuildDivisionTarget(const FWorldCommanderPickCandidate& Candidate, FWorldCommanderMapAbilityTarget& OutTarget)
	{
		if (Candidate.Thing != EWorldCommanderPickedThing::Division || not Candidate.DivisionKey.IsValid())
		{
			return false;
		}
		OutTarget.DivisionKey = Candidate.DivisionKey;
		return true;
	}

	bool BuildBuildNodeTarget(const FWorldCommanderPickCandidate& Candidate,
	                          FWorldCommanderMapAbilityTarget& OutTarget)
	{
		if (Candidate.Thing != EWorldCommanderPickedThing::BuildNode || Candidate.BuildNodeId.IsNone())
		{
			return false;
		}
		OutTarget.BuildNodeId = Candidate.BuildNodeId;
		return true;
	}

	bool BuildLocationTarget(const FWorldCommanderPickCandidate& Candidate, FWorldCommanderMapAbilityTarget& OutTarget)
	{
		if (not Candidate.bHasWorldLocation)
		{
			return false;
		}
		OutTarget.WorldLocation = Candidate.WorldLocation;
		OutTarget.bHasWorldLocation = true;
		return true;
	}
}

EWorldCommanderAbilityButtonState WorldCommanderPanelRules::GetAbilityButtonState(const bool bHasAbility,
	const int32 CooldownTurnsRemaining, const bool bIsPickingTarget)
{
	if (not bHasAbility)
	{
		return EWorldCommanderAbilityButtonState::Unavailable;
	}
	if (CooldownTurnsRemaining > 0)
	{
		return EWorldCommanderAbilityButtonState::CoolingDown;
	}
	return bIsPickingTarget
		       ? EWorldCommanderAbilityButtonState::PickingTarget
		       : EWorldCommanderAbilityButtonState::Ready;
}

FText WorldCommanderPanelRules::GetAbilityButtonLabel(const EWorldCommanderAbilityButtonState State,
                                                      const int32 CooldownTurnsRemaining)
{
	if (State != EWorldCommanderAbilityButtonState::CoolingDown)
	{
		return FText::GetEmpty();
	}
	return FText::AsNumber(CooldownTurnsRemaining);
}

FText WorldCommanderPanelRules::GetAbilityStateLine(const EWorldCommanderAbilityButtonState State,
                                                    const int32 CooldownTurnsRemaining,
                                                    const int32 AbilityCooldownTurns)
{
	switch (State)
	{
	case EWorldCommanderAbilityButtonState::Unavailable:
		return FText::GetEmpty();
	case EWorldCommanderAbilityButtonState::CoolingDown:
		return CooldownTurnsRemaining == 1
			       ? NSLOCTEXT("WorldCommanderPanel", "ReadyInOneTurn", "READY IN 1 TURN")
			       : FText::Format(NSLOCTEXT("WorldCommanderPanel", "ReadyInTurns", "READY IN {0} TURNS"),
			                       FText::AsNumber(CooldownTurnsRemaining));
	case EWorldCommanderAbilityButtonState::Ready:
	case EWorldCommanderAbilityButtonState::PickingTarget:
		break;
	}
	if (AbilityCooldownTurns <= 0)
	{
		return NSLOCTEXT("WorldCommanderPanel", "Ready", "READY");
	}
	return AbilityCooldownTurns == 1
		       ? NSLOCTEXT("WorldCommanderPanel", "ReadyCooldownOneTurn", "READY · cooldown 1 turn")
		       : FText::Format(NSLOCTEXT("WorldCommanderPanel", "ReadyCooldownTurns", "READY · cooldown {0} turns"),
		                       FText::AsNumber(AbilityCooldownTurns));
}

FText WorldCommanderPanelRules::GetTargetKindNoun(const EWorldCommanderAbilityTargetKind Kind)
{
	switch (Kind)
	{
	case EWorldCommanderAbilityTargetKind::None:
		return FText::GetEmpty();
	case EWorldCommanderAbilityTargetKind::Anchor:
		return NSLOCTEXT("WorldCommanderPanel", "NounAnchor", "an enemy target");
	case EWorldCommanderAbilityTargetKind::Division:
		return NSLOCTEXT("WorldCommanderPanel", "NounDivision", "an enemy division");
	case EWorldCommanderAbilityTargetKind::BuildNode:
		return NSLOCTEXT("WorldCommanderPanel", "NounBuildNode", "a build site");
	case EWorldCommanderAbilityTargetKind::WorldLocation:
		return NSLOCTEXT("WorldCommanderPanel", "NounWorldLocation", "a place on the map");
	}
	return FText::GetEmpty();
}

FText WorldCommanderPanelRules::GetTargetPromptText(const EWorldCommanderAbilityTargetKind Kind)
{
	if (Kind == EWorldCommanderAbilityTargetKind::None)
	{
		return FText::GetEmpty();
	}
	return FText::Format(NSLOCTEXT("WorldCommanderPanel", "PickPrompt", "Pick a target: {0}"), GetTargetKindNoun(Kind));
}

bool WorldCommanderPanelRules::BuildTargetFromPick(const EWorldCommanderAbilityTargetKind Kind,
                                                   const FWorldCommanderPickCandidate& Candidate,
                                                   FWorldCommanderMapAbilityTarget& OutTarget,
                                                   FText& OutFailReason)
{
	using namespace WorldCommanderPanelRulesPrivate;
	OutTarget = FWorldCommanderMapAbilityTarget();
	bool bIsTargetOfKind = false;
	switch (Kind)
	{
	case EWorldCommanderAbilityTargetKind::None:
		return true;
	case EWorldCommanderAbilityTargetKind::Anchor:
		bIsTargetOfKind = BuildAnchorTarget(Candidate, OutTarget);
		break;
	case EWorldCommanderAbilityTargetKind::Division:
		bIsTargetOfKind = BuildDivisionTarget(Candidate, OutTarget);
		break;
	case EWorldCommanderAbilityTargetKind::BuildNode:
		bIsTargetOfKind = BuildBuildNodeTarget(Candidate, OutTarget);
		break;
	case EWorldCommanderAbilityTargetKind::WorldLocation:
		bIsTargetOfKind = BuildLocationTarget(Candidate, OutTarget);
		break;
	}
	if (not bIsTargetOfKind)
	{
		OutFailReason = MakeWrongPickReason(Kind);
	}
	return bIsTargetOfKind;
}
