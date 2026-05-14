#include "StrategicActions.h"

#include "RTS_Survival/Enemy/StrategicAI/StrategicAIBlackboard.h"
#include "RTS_Survival/Enemy/EnemyAISettings/EnemyAISettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UStrategicAISubAction::OnActionExecuted(const float Now)
{
	M_TimeStampLastExecution = Now;
}

float UStrategicAISubAction::GetLastTimeActionExecuted()
{
	return M_TimeStampLastExecution;
}

void UStrategicAISubAction::BuildTrainingPressureContributions(
	const FStrategicAIBlackboard& Blackboard,
	const float GameTimeSeconds,
	TArray<FEnemyStrategicTrainingPressureContribution>& OutPressureContributions) const
{
	if (not bM_ContributesTrainingPressure)
	{
		return;
	}

	const bool bIsBlockedByNativeRequirement = GetIsBlockedByTrainingPressureRequirement(
		M_NativeVisibleRequirements,
		Blackboard,
		GameTimeSeconds);
	const bool bIsBlockedByDesignerRequirement = GetIsBlockedByTrainingPressureRequirement(
		M_Requirements,
		Blackboard,
		GameTimeSeconds);
	if (bIsBlockedByNativeRequirement || bIsBlockedByDesignerRequirement)
	{
		return;
	}

	bool bBlockedByNonUnitRequirement = false;
	const bool bHasNativeUnitRequirements = TryBuildMissingUnitRequirementPressureContributions(
		M_NativeVisibleRequirements,
		Blackboard,
		GameTimeSeconds,
		OutPressureContributions,
		bBlockedByNonUnitRequirement);
	if (bHasNativeUnitRequirements)
	{
		return;
	}

	const bool bHasDesignerUnitRequirements = TryBuildMissingUnitRequirementPressureContributions(
		M_Requirements,
		Blackboard,
		GameTimeSeconds,
		OutPressureContributions,
		bBlockedByNonUnitRequirement);
	if (bHasDesignerUnitRequirements)
	{
		return;
	}

	AddBaseTrainingPressureContribution(OutPressureContributions, GameTimeSeconds);
}

bool UStrategicAISubAction::GetAreRequirementsMet(
	const FStrategicAIBlackboard& RequirementContext,
	const float GameTimeSeconds) const
{
	return GetAreRequirementsMetForArray(M_NativeVisibleRequirements, RequirementContext, GameTimeSeconds)
		&& GetAreRequirementsMetForArray(M_Requirements, RequirementContext, GameTimeSeconds);
}

FIdleUnitSelectionPolicy UStrategicAISubAction::BuildIdleUnitSelectionPolicy(
	const FStrategicAIBlackboard& Blackboard) const
{
	// Start with mission defaults so every SubAction has a valid selection range even when no requirement contributes selection constraints.
	FIdleUnitSelectionPolicy SelectionPolicy;
	SetupFallbackMinMaxSelectionPolicy(Blackboard, SelectionPolicy);

	/*
	 * WHY this merge step exists:
	 * - We aggregate policy contributions from both native and data-driven requirement lists so unit picking follows
	 *   the same gameplay constraints that were used to validate whether the action is allowed.
	 *
	 * How requirement types affect the policy:
	 * - Requirements that call AddMinimumIdleUnitsContribution(...) only raise MinUnitsToPick (generic count floor).
	 * - Requirements that call AddRequiredRule(...) add explicit composition constraints (for example AnyTank,
	 *   TankSubtype, SquadSubtype, TankCategory), and also raise MinUnitsToPick to at least the total required-rule count.
	 * - Requirements that only implement GetIsRequirementMet(...) act as action gates and do not change selection
	 *   composition directly.
	 *
	 * What if a requirement does not override ContributeToIdleUnitSelectionPolicy(...):
	 * - It contributes nothing to selection policy (base implementation is intentionally no-op).
	 * - That requirement can still block/allow action execution through GetIsRequirementMet(...).
	 * - Min/Max then remain driven by fallback mission settings plus any other requirements that did contribute.
	 */
	AddRequirementSelectionRulesToPolicy(M_NativeVisibleRequirements, SelectionPolicy);
	AddRequirementSelectionRulesToPolicy(M_Requirements, SelectionPolicy);
	SelectionPolicy.NormalizeMinMax();
	return SelectionPolicy;
}

void UStrategicAISubAction::SetupFallbackMinMaxSelectionPolicy(
	const FStrategicAIBlackboard& Blackboard,
	FIdleUnitSelectionPolicy& SelectionPolicy) const
{
	const int32 MissionMinUnitsToPick = Blackboard.StrategicAIMissionSettings.MinPickedUnitsForAction;
	const int32 MissionMaxUnitsToPick = Blackboard.StrategicAIMissionSettings.MaxPickedUnitsForAction;
	if (not bOverwriteMissionSettingsMinMaxUnitsNeeded)
	{
		SelectionPolicy.SetupFallbackMinMax(MissionMinUnitsToPick, MissionMaxUnitsToPick);
		return;
	}

	constexpr int32 MinAllowedPickCount = 0;
	const bool bHasNegativeOverride =
		MinUnitsNeededOverwrite < MinAllowedPickCount || MaxUnitsNeededOverwrite < MinAllowedPickCount;
	const bool bIsBothOverrideValuesZero =
		MinUnitsNeededOverwrite == MinAllowedPickCount && MaxUnitsNeededOverwrite == MinAllowedPickCount;
	const bool bIsInvalidMinMaxRange = MinUnitsNeededOverwrite > MaxUnitsNeededOverwrite;
	if (bHasNegativeOverride || bIsBothOverrideValuesZero || bIsInvalidMinMaxRange)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("Strategic AI SubAction has invalid Min/Max overwrite values while overwrite is enabled.")
			TEXT(" Falling back to mission MinPickedUnitsForAction/MaxPickedUnitsForAction."));
		SelectionPolicy.SetupFallbackMinMax(MissionMinUnitsToPick, MissionMaxUnitsToPick);
		return;
	}

	SelectionPolicy.SetupFallbackMinMax(MinUnitsNeededOverwrite, MaxUnitsNeededOverwrite);
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

void UStrategicAISubAction::AddRequirementSelectionRulesToPolicy(
	const TArray<TObjectPtr<UStrategicAIActionRequirement>>& Requirements,
	FIdleUnitSelectionPolicy& SelectionPolicy) const
{
	for (const TObjectPtr<UStrategicAIActionRequirement>& EachRequirement : Requirements)
	{
		if (EachRequirement == nullptr)
		{
			continue;
		}

		EachRequirement->ContributeToIdleUnitSelectionPolicy(SelectionPolicy);
	}
}

bool UStrategicAISubAction::GetIsBlockedByTrainingPressureRequirement(
	const TArray<TObjectPtr<UStrategicAIActionRequirement>>& Requirements,
	const FStrategicAIBlackboard& Blackboard,
	const float GameTimeSeconds) const
{
	for (const TObjectPtr<UStrategicAIActionRequirement>& EachRequirement : Requirements)
	{
		if (EachRequirement == nullptr)
		{
			continue;
		}

		const bool bRequirementMet = EachRequirement->GetIsRequirementMet(Blackboard, GameTimeSeconds);
		if (not bRequirementMet && EachRequirement->GetShouldBlockTrainingPressureWhenUnmet(Blackboard, GameTimeSeconds))
		{
			return true;
		}
	}

	return false;
}

bool UStrategicAISubAction::TryBuildMissingUnitRequirementPressureContributions(
	const TArray<TObjectPtr<UStrategicAIActionRequirement>>& Requirements,
	const FStrategicAIBlackboard& Blackboard,
	const float GameTimeSeconds,
	TArray<FEnemyStrategicTrainingPressureContribution>& OutPressureContributions,
	bool& bOutBlockedByNonUnitRequirement) const
{
	bool bHasUnitRequirements = false;
	for (const TObjectPtr<UStrategicAIActionRequirement>& EachRequirement : Requirements)
	{
		if (EachRequirement == nullptr)
		{
			continue;
		}

		const bool bRequirementMet = EachRequirement->GetIsRequirementMet(Blackboard, GameTimeSeconds);
		if (not bRequirementMet && EachRequirement->GetShouldBlockTrainingPressureWhenUnmet(Blackboard, GameTimeSeconds))
		{
			OutPressureContributions.Reset();
			bOutBlockedByNonUnitRequirement = true;
			return false;
		}

		if (not EachRequirement->GetIsUnitTrainingPressureRequirement())
		{
			continue;
		}

		bHasUnitRequirements = true;
		if (bRequirementMet)
		{
			continue;
		}

		EachRequirement->AddMissingUnitTrainingPressureContribution(
			OutPressureContributions,
			GetTrainingPressureAmount(GameTimeSeconds),
			GetNameFromActionEnum() + TEXT(" -> ") + EachRequirement->GetDebugString(),
			M_SpecialtyPressure);
		return true;
	}

	return bHasUnitRequirements;
}

void UStrategicAISubAction::AddBaseTrainingPressureContribution(
	TArray<FEnemyStrategicTrainingPressureContribution>& OutPressureContributions,
	const float GameTimeSeconds) const
{
	FEnemyStrategicTrainingPressureContribution PressureContribution;
	PressureContribution.FocusPressure = M_FocusPressure;
	PressureContribution.SpecialtyPressure = M_SpecialtyPressure;
	PressureContribution.PressureAmount = GetTrainingPressureAmount(GameTimeSeconds);
	PressureContribution.SourceDebugName = GetNameFromActionEnum();

	const bool bHasNoBaseTrainingPressure = M_FocusPressure == EAITrainingFocus::NoFocus
		&& M_SpecialtyPressure == EAITrainingFocusSpecialty::NoTrainingPressure;
	if (bHasNoBaseTrainingPressure)
	{
		return;
	}

	PressureContribution.bSpreadFocusPressureAcrossAllBuckets = M_FocusPressure == EAITrainingFocus::NoFocus;
	OutPressureContributions.Add(PressureContribution);
}

float UStrategicAISubAction::GetTrainingPressureAmount(const float GameTimeSeconds) const
{
	const float TimeSinceLastExecution = FMath::Max(0.f, GameTimeSeconds - M_TimeStampLastExecution);
	const float AgeBonusMultiplier = FMath::Clamp(
		TimeSinceLastExecution / EnemyAISettings::TrainingPressure::TrainingPressureAgeRampSeconds,
		0.f,
		EnemyAISettings::TrainingPressure::TrainingPressureMaxAgeBonusMultiplier);

	return FMath::Max(0.f, M_Score) * FMath::Max(0.f, M_BaseTrainingPressureAmount) * (1.f + AgeBonusMultiplier);
}

USubAction_AttackMoveToPlayerUnits::USubAction_AttackMoveToPlayerUnits()
{
	SubtypeAction = ESubtypeAction::AttackMoveToPlayerUnits;
	M_FocusPressure = EAITrainingFocus::Infantry;
	M_SpecialtyPressure = EAITrainingFocusSpecialty::AntiInfantry;
}

FString USubAction_AttackMoveToPlayerUnits::GetDebugString() const
{
	return TEXT("Attack Move To Player Units") + GetRequirementsDebugString();
}

USubAction_AttackMoveToPlayerHQ::USubAction_AttackMoveToPlayerHQ()
{
	SubtypeAction = ESubtypeAction::AttackMoveToPlayerHQ;
	M_FocusPressure = EAITrainingFocus::MediumTanks;
	M_SpecialtyPressure = EAITrainingFocusSpecialty::SpecialWeaponCarrier;
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
	M_FocusPressure = EAITrainingFocus::LightTanks;
	M_SpecialtyPressure = EAITrainingFocusSpecialty::GeneralPurpose;
	AddNativeVisibleRequirement(CreateDefaultSubobject<UStrategicAIHasPlayerResourceBuildingLocationsRequirement>(
		TEXT("HasPlayerResourceBuildingLocationsRequirement")));
}

FString USubAction_AttackMoveToPlayerResourceBuildings::GetDebugString() const
{
	return TEXT("Attack Move To Player Resource Buildings") + GetRequirementsDebugString();
}

USubAction_AttackSpecificPoints::USubAction_AttackSpecificPoints()
{
	SubtypeAction = ESubtypeAction::AttackMoveSpecificPoints;
	M_SpecialtyPressure = EAITrainingFocusSpecialty::GeneralPurpose;
}

FString USubAction_AttackSpecificPoints::GetDebugString() const
{
	FString DebugString = TEXT("Attack Move To Specific Points") + GetRequirementsDebugString();
	return DebugString;
}

USubAction_DefendBase::USubAction_DefendBase()
{
	SubtypeAction = ESubtypeAction::DefendBase;
	M_FocusPressure = EAITrainingFocus::Infantry;
	M_SpecialtyPressure = EAITrainingFocusSpecialty::AntiInfantry;
}

FString USubAction_DefendBase::GetDebugString() const
{
	return TEXT("Defend Base") + GetRequirementsDebugString();
}

USubAction_DefendImportantMissionPoint::USubAction_DefendImportantMissionPoint()
{
	SubtypeAction = ESubtypeAction::DefendImportantMissionPoint;
	M_FocusPressure = EAITrainingFocus::Infantry;
	M_SpecialtyPressure = EAITrainingFocusSpecialty::GeneralPurpose;
}

FString USubAction_DefendImportantMissionPoint::GetDebugString() const
{
	return TEXT("Defend Important Mission Point") + GetRequirementsDebugString();
}

USubAction_LightTanksAttackPlayerUnits::USubAction_LightTanksAttackPlayerUnits()
{
	SubtypeAction = ESubtypeAction::AttackMoveLightTanksToPlayerUnits;
	M_FocusPressure = EAITrainingFocus::LightTanks;
	M_SpecialtyPressure = EAITrainingFocusSpecialty::GeneralPurpose;
	AddNativeVisibleRequirement(CreateDefaultSubobject<UStrategicAIHasEnoughLightTanks>(
		TEXT("HasIdleLightTanksRequirement")));
	bOverwriteMissionSettingsMinMaxUnitsNeeded = true;
	MinUnitsNeededOverwrite = 5;
	MaxUnitsNeededOverwrite = 5;
}

FString USubAction_LightTanksAttackPlayerUnits::GetDebugString() const
{
	return TEXT("Light Tanks Attack Player Units") + GetRequirementsDebugString();
}

USubAction_HeavyTankPushPlayerBaseOrUnits::USubAction_HeavyTankPushPlayerBaseOrUnits()
{
	SubtypeAction = ESubtypeAction::HeavyTankPushPlayerBaseOrUnits;
	M_FocusPressure = EAITrainingFocus::HeavyTanks;
	M_SpecialtyPressure = EAITrainingFocusSpecialty::AntiInfantry;
	AddNativeVisibleRequirement(CreateDefaultSubobject<UStrategicAIHasEnoughHeavyTanks>(
		TEXT("HasIdleHeavyTanksRequirement")));
	bOverwriteMissionSettingsMinMaxUnitsNeeded = true;
	MinUnitsNeededOverwrite = 2;
	MaxUnitsNeededOverwrite = 10;
	
}

FString USubAction_HeavyTankPushPlayerBaseOrUnits::GetDebugString() const
{
	return TEXT("Heavy Tank Push Player Base Or Units") + GetRequirementsDebugString();
}

USubAction_FlankPlayerHeavies::USubAction_FlankPlayerHeavies()
{
	SubtypeAction = ESubtypeAction::FlankPlayerHeavies;
	M_FocusPressure = EAITrainingFocus::LightTanks;
	M_SpecialtyPressure = EAITrainingFocusSpecialty::AntiTank;
	AddNativeVisibleRequirement(CreateDefaultSubobject<UStrategicAIDoesBlackboardHaveHeavyTankFlankPositions>(
		TEXT("DoesBlackboardHaveHeavyFlankPositions")));
	AddNativeVisibleRequirement(CreateDefaultSubobject<UStrategicAIHasAtLeastAnyIdleTanks>(
		TEXT("HasAnyXTanksRequirement")));
}

FString USubAction_FlankPlayerHeavies::GetDebugString() const
{
	return TEXT("Flank Player Heavies") + GetRequirementsDebugString();
}
