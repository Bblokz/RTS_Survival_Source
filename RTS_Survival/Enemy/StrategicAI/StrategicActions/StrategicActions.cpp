#include "StrategicActions.h"

bool UStrategicAISubAction::GetAreRequirementsMet(
	const FStrategicAIBlackboard& RequirementContext,
	const float GameTimeSeconds) const
{
	for (const TObjectPtr<UStrategicAIActionRequirement>& EachRequirement : M_Requirements)
	{
		if (EachRequirement == nullptr)
		{
			return false;
		}

		if (not EachRequirement->GetIsRequirementMet(RequirementContext, GameTimeSeconds))
		{
			return false;
		}
	}

	return true;
}

FString UStrategicAISubAction::GetDebugString() const
{
	return TEXT("DO NOT USE Default SubAction: ") + GetRequirementsDebugString();
}

FString UStrategicAISubAction::GetRequirementsDebugString() const
{
	FString DebugString = TEXT("\n--Requirements:");

	for (const TObjectPtr<UStrategicAIActionRequirement>& EachRequirement : M_Requirements)
	{
		if (EachRequirement == nullptr)
		{
			DebugString += TEXT("\n -- Req: Invalid Requirement");
			continue;
		}

		DebugString += TEXT("\n -- Req: ") + EachRequirement->GetDebugString();
	}

	return DebugString;
}

USubAction_AttackMoveToPlayerUnits::USubAction_AttackMoveToPlayerUnits()
{
	SubtypeAction = ESubtypeAction::AttackMoveToPlayerUnits;
}

FString USubAction_AttackMoveToPlayerUnits::GetDebugString() const
{
	return TEXT("Attack Move To Player Units") + GetRequirementsDebugString();
}

USubAction_AttackMoveToPlayerHQ::USubAction_AttackMoveToPlayerHQ()
{
	SubtypeAction = ESubtypeAction::AttackMoveToPlayerHQ;
}

FString USubAction_AttackMoveToPlayerHQ::GetDebugString() const
{
	return TEXT("Attack Move To Player HQ") + GetRequirementsDebugString();
}

USubAction_AttackMoveToPlayerResourceBuildings::USubAction_AttackMoveToPlayerResourceBuildings()
{
	SubtypeAction = ESubtypeAction::AttackMoveToPlayerResourceBuildings;
}

FString USubAction_AttackMoveToPlayerResourceBuildings::GetDebugString() const
{
	return TEXT("Attack Move To Player Resource Buildings") + GetRequirementsDebugString();
}

USubAction_AttackSpecificPoint::USubAction_AttackSpecificPoint()
{
	SubtypeAction = ESubtypeAction::AttackMoveSpecificPoint;
}

FString USubAction_AttackSpecificPoint::GetDebugString() const
{
	return TEXT("Attack Move To Specific Point: ") + TargetPoint.ToString() + GetRequirementsDebugString();
}

USubAction_DefendBase::USubAction_DefendBase()
{
	SubtypeAction = ESubtypeAction::DefendBase;
}

FString USubAction_DefendBase::GetDebugString() const
{
	return TEXT("Defend Base") + GetRequirementsDebugString();
}

USubAction_DefendImportantMissionPoint::USubAction_DefendImportantMissionPoint()
{
	SubtypeAction = ESubtypeAction::DefendImportantMissionPoint;
}

FString USubAction_DefendImportantMissionPoint::GetDebugString() const
{
	return TEXT("Defend Important Mission Point") + GetRequirementsDebugString();
}
