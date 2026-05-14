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
	// Some sub-actions are only meant to command existing units.
	// If this flag is disabled, the action should not influence future production at all,
	// even if its requirements are currently unmet.
	if (not bM_ContributesTrainingPressure)
	{
		return;
	}

	// First check whether any native requirement blocks training pressure completely.
	// Native requirements are code-created requirements that are always part of this sub-action.
	// If one of these fails for a reason production cannot solve, such as missing target data,
	// then adding training pressure would be misleading.
	const bool bIsBlockedByNativeRequirement = GetIsBlockedByTrainingPressureRequirement(
		M_NativeVisibleRequirements,
		Blackboard,
		GameTimeSeconds);

	// Designer requirements are checked separately because they are authored on the sub-action data.
	// They can also represent gates that production cannot fix, for example a required time,
	// player location, or other strategic condition.
	const bool bIsBlockedByDesignerRequirement = GetIsBlockedByTrainingPressureRequirement(
		M_Requirements,
		Blackboard,
		GameTimeSeconds);

	// If either requirement group contains a non-trainable blocker, stop immediately.
	// This prevents the AI from training units for a plan that is impossible for reasons
	// unrelated to available unit counts.
	if (bIsBlockedByNativeRequirement || bIsBlockedByDesignerRequirement)
	{
		return;
	}

	// At this point, the action is allowed to create training pressure.
	// Before adding the generic/base pressure from the sub-action itself, we look for unmet
	// unit-count requirements. Missing-unit requirements are more specific than the fallback
	// pressure settings, because they can say exactly which focus bucket should receive pressure.
	bool bBlockedByNonUnitRequirement = false;

	// Native requirements are processed first because they are part of the sub-action's built-in logic.
	// If a native missing-unit requirement emits pressure, we return after that so the same action
	// does not also add generic fallback pressure and double-count the same strategic need.
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

	// If native requirements did not emit unit pressure, try the designer-authored requirements.
	// This allows designers to add missing-unit gates in data and have those gates become
	// production pressure when the action cannot currently run due to lacking units.
	const bool bHasDesignerUnitRequirements = TryBuildMissingUnitRequirementPressureContributions(
		M_Requirements,
		Blackboard,
		GameTimeSeconds,
		OutPressureContributions,
		bBlockedByNonUnitRequirement);

	// As with native requirements, a designer missing-unit requirement is considered the specific
	// reason for training pressure. We return here to avoid adding the broader fallback pressure
	// on top of the more precise requirement pressure.
	if (bHasDesignerUnitRequirements)
	{
		return;
	}

	// No requirement blocked pressure, and no missing-unit requirement gave us a more specific reason.
	// In that case, add the sub-action's base pressure contribution. This is the fallback path for
	// actions that should bias future production even when they are not waiting on a concrete unit gate.
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
		// Only return true when this requirement fails AND it blocks training pressure (e.g. no locations available
		// or game time not reached certain point) those are examples where training pressure would be misleading
		// as the cause is not some missing unit!
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
	// This flag answers a very specific question for the caller:
	// "Did this requirement list contain any unit-training requirement at all?"
	//
	// That matters even if every unit requirement is already met, because the caller uses this
	// to avoid adding generic fallback pressure on top of a requirement-driven action.
	bool bHasUnitRequirements = false;

	for (const TObjectPtr<UStrategicAIActionRequirement>& EachRequirement : Requirements)
	{
		// A null requirement cannot safely be evaluated.
		// We skip it instead of failing the whole pressure pass because designer-authored arrays can
		// temporarily contain empty entries while being edited, and an empty slot should not create
		// false training pressure.
		if (EachRequirement == nullptr)
		{
			continue;
		}

		// Evaluate the requirement once and reuse the result for both blocker checks and unit-pressure checks.
		// This keeps the logic consistent inside this loop iteration and avoids asking the same requirement
		// for its state multiple times.
		const bool bRequirementMet = EachRequirement->GetIsRequirementMet(Blackboard, GameTimeSeconds);

		// Non-unit gates must be handled before unit-pressure conversion.
		//
		// Example: an action might need both "player HQ location is known" and "2 idle tanks".
		// If the HQ location is unknown, training more tanks will not make this action executable.
		// In that case, any pressure already added by earlier requirements in this same pass would be
		// misleading, so we clear it and tell the caller that a non-unit blocker stopped pressure.
		if (not bRequirementMet && EachRequirement->GetShouldBlockTrainingPressureWhenUnmet(Blackboard, GameTimeSeconds))
		{
			OutPressureContributions.Reset();
			bOutBlockedByNonUnitRequirement = true;
			return false;
		}

		// Requirements that are not marked as unit-training pressure requirements are only gates.
		// If they are met, they do not add pressure.
		// If they are unmet and reached this point, they also did not block pressure, so they can be ignored
		// for the purpose of missing-unit pressure generation.
		if (not EachRequirement->GetIsUnitTrainingPressureRequirement())
		{
			continue;
		}

		// From this point onward, the requirement list is known to contain at least one unit requirement.
		// We record that even if this particular requirement is already met, because the caller should then
		// know that this action's production intent is requirement-driven rather than fallback-driven.
		bHasUnitRequirements = true;

		// A unit requirement that is already satisfied should not create more demand.
		// We continue looking because a later unit requirement in the same list may still be unmet and may
		// need to emit pressure.
		if (bRequirementMet)
		{
			continue;
		}

		// This is the first unmet unit requirement that production can help solve.
		// We emit pressure through the requirement itself so the requirement can decide the correct focus
		// bucket, for example light tanks, heavy tanks, squads, or spread pressure for "any idle unit".
		//
		// The pressure amount comes from the sub-action so action age/importance stays centralized.
		// The debug name combines the owning action and the requirement so later bucket debug output can
		// explain exactly why this pressure was added.
		// The specialty pressure is passed through from the sub-action because the action usually knows the
		// tactical role it wanted, while the requirement usually knows the broad unit family it is missing.
		EachRequirement->AddMissingUnitTrainingPressureContribution(
			OutPressureContributions,
			GetTrainingPressureAmount(GameTimeSeconds),
			GetNameFromActionEnum() + TEXT(" -> ") + EachRequirement->GetDebugString(),
			M_SpecialtyPressure);

		// Return true because this requirement list produced requirement-specific training pressure.
		// The caller should not add fallback/base pressure after this, otherwise the same blocked action
		// would be counted twice.
		return true;
	}

	// If we reach the end, no unmet unit requirement emitted pressure.
	//
	// Returning true still has meaning here: it says "this list contained unit requirements, but they were
	// already satisfied." The caller can then avoid adding generic fallback pressure for an action whose
	// unit requirements do not currently need help.
	//
	// Returning false means this list had no unit-training requirements, so the caller may still consider
	// generic fallback pressure if no other requirement list handled the action.
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
