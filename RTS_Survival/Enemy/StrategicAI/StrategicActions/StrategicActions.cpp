#include "StrategicActions.h"

bool UStrategicAISubAction::GetAreRequirementsMet(
	const FStrategicAIBlackboard& RequirementContext,
	const float GameTimeSeconds) const
{
	return GetAreRequirementsMetForArray(M_NativeVisibleRequirements, RequirementContext, GameTimeSeconds)
		&& GetAreRequirementsMetForArray(M_Requirements, RequirementContext, GameTimeSeconds);
}

void UStrategicAISubAction::AddNativeVisibleRequirement(UStrategicAIActionRequirement* Requirement)
{
	M_NativeVisibleRequirements.Add(Requirement);
}

bool UStrategicAISubAction::GetAreRequirementsMetForArray(
	const TArray<TObjectPtr<UStrategicAIActionRequirement>>& Requirements,
	const FStrategicAIBlackboard& RequirementContext,
	const float GameTimeSeconds) const
{
	for (const TObjectPtr<UStrategicAIActionRequirement>& EachRequirement : Requirements)
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

FString UStrategicAISubAction::GetNameFromActionEnum() const
{
	return UEnum::GetValueAsString(SubtypeAction);
}

FString UStrategicAISubAction::GetRequirementsDebugString() const
{
	return GetRequirementsDebugStringForArray(M_NativeVisibleRequirements, TEXT("\n--Native Visible Requirements:"))
		+ GetRequirementsDebugStringForArray(M_Requirements, TEXT("\n--Requirements:"));
}

FString UStrategicAISubAction::GetRequirementsDebugStringForArray(
	const TArray<TObjectPtr<UStrategicAIActionRequirement>>& Requirements,
	const FString& HeaderText) const
{
	FString DebugString = HeaderText;

	for (const TObjectPtr<UStrategicAIActionRequirement>& EachRequirement : Requirements)
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
	AddNativeVisibleRequirement(CreateDefaultSubobject<UStrategicAIHasPlayerHQLocationRequirement>(
		TEXT("HasPlayerHQLocationRequirement")));
}

FString USubAction_AttackMoveToPlayerHQ::GetDebugString() const
{
	return TEXT("Attack Move To Player HQ") + GetRequirementsDebugString();
}

USubAction_AttackMoveToPlayerResourceBuildings::USubAction_AttackMoveToPlayerResourceBuildings()
{
	SubtypeAction = ESubtypeAction::AttackMoveToPlayerResourceBuildings;
	AddNativeVisibleRequirement(CreateDefaultSubobject<UStrategicAIHasPlayerResourceBuildingLocationsRequirement>(
		TEXT("HasPlayerResourceBuildingLocationsRequirement")));
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

USubAction_LightTanksAttackPlayerUnits::USubAction_LightTanksAttackPlayerUnits()
{
	SubtypeAction = ESubtypeAction::AttackMoveLightTanksToPlayerUnits;
	AddNativeVisibleRequirement(CreateDefaultSubobject<UStrategicAIHasEnoughLightTanks>(
		TEXT("HasIdleLightTanksRequirement")));	
}

FString USubAction_LightTanksAttackPlayerUnits::GetDebugString() const
{
	return TEXT("Light Tanks Attack Player Units") + GetRequirementsDebugString();
}

USubAction_HeavyTankPushPlayerBaseOrUnits::USubAction_HeavyTankPushPlayerBaseOrUnits()
{
	SubtypeAction = ESubtypeAction::HeavyTankPushPlayerBaseOrUnits;
	AddNativeVisibleRequirement(CreateDefaultSubobject<UStrategicAIHasEnoughHeavyTanks>(
		TEXT("HasIdleHeavyTanksRequirement")));
	
}

FString USubAction_HeavyTankPushPlayerBaseOrUnits::GetDebugString() const
{
	return TEXT("Heavy Tank Push Player Base Or Units") + GetRequirementsDebugString();
}

USubAction_FlankPlayerHeavies::USubAction_FlankPlayerHeavies()
{
	SubtypeAction = ESubtypeAction::FlankPlayerHeavies;
	AddNativeVisibleRequirement(CreateDefaultSubobject<UStrategicAIDoesBlackboardHaveHeavyTankFlankPositions>(
		TEXT("DoesBlackboardHaveHeavyFlankPositions")));
	AddNativeVisibleRequirement(CreateDefaultSubobject<UStrategicAIHasAtLeastAnyIdleTanks>(
	TEXT("HasAnyXTanksRequirement")));
}

FString USubAction_FlankPlayerHeavies::GetDebugString() const
{
	return TEXT("Flank Player Heavies") + GetRequirementsDebugString();
}
