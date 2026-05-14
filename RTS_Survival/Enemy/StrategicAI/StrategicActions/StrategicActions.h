#pragma once

#include "CoreMinimal.h"
#include "StrategicActionRequirements/StrategicAIActionRequirements.h"

#include "StrategicActions.generated.h"

UENUM(Blueprintable)
enum class ESubtypeAction : uint8
{
	DEFAULT_OBJECT,
	AttackMoveToPlayerUnits,
	AttackMoveLightTanksToPlayerUnits,
	HeavyTankPushPlayerBaseOrUnits,
	FlankPlayerHeavies,
	AttackMoveToPlayerHQ,
	AttackMoveToPlayerResourceBuildings,
	AttackMoveSpecificPoints,
	DefendBase,
	DefendImportantMissionPoint,
};

/**
 * @brief Used as instanced tree entries that score and gate strategic AI commands.
 */
UCLASS(Abstract, BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UStrategicAISubAction : public UObject
{
	GENERATED_BODY()

public:
	UStrategicAISubAction() = default;

	UPROPERTY(VisibleAnywhere)
	ESubtypeAction SubtypeAction = ESubtypeAction::DEFAULT_OBJECT;

	void OnActionExecuted(const float Now);
	float GetLastTimeActionExecuted();

	float GetScore() const
	{
		return M_Score;
	}

	const TArray<TObjectPtr<UStrategicAIActionRequirement>>& GetRequirements() const
	{
		return M_Requirements;
	}

	const TArray<TObjectPtr<UStrategicAIActionRequirement>>& GetNativeVisibleRequirements() const
	{
		return M_NativeVisibleRequirements;
	}

	bool GetAreRequirementsMet(
		const FStrategicAIBlackboard& RequirementContext,
		const float GameTimeSeconds) const;

	virtual FString GetDebugString() const;

	/**
	 * @brief Emits pressure separately from action execution so training can prepare for plans that are currently blocked.
	 * @param Blackboard Provides target/unit context to prevent non-unit-gated plans from biasing training.
	 * @param GameTimeSeconds Used to age old strategic desires into stronger training pressure.
	 * @param OutPressureContributions Receives actionable training reasons for the bucket state.
	 */
	void BuildTrainingPressureContributions(
		const FStrategicAIBlackboard& Blackboard,
		const float GameTimeSeconds,
		TArray<FEnemyStrategicTrainingPressureContribution>& OutPressureContributions) const;
	// We aggregate policy contributions from both native and data-driven (Designer-added) requirement lists so unit picking follows
	// the same gameplay constraints that were used to validate whether the action is allowed.
	virtual FIdleUnitSelectionPolicy BuildIdleUnitSelectionPolicy(const FStrategicAIBlackboard& Blackboard) const;

	FString GetNameFromActionEnum() const;

protected:
	void AddNativeVisibleRequirement(UStrategicAIActionRequirement* Requirement);

	FString GetRequirementsDebugString() const;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		meta = (AllowPrivateAccess = true ))
	bool bOverwriteMissionSettingsMinMaxUnitsNeeded = false;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		meta = (
			AllowPrivateAccess = true,
			EditCondition="bOverwriteMissionSettingsMinMaxUnitsNeeded",
			ClampMin = "0"
			))
	int32 MinUnitsNeededOverwrite = 0;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		meta = (
			AllowPrivateAccess = true,
			EditCondition = "bOverwriteMissionSettingsMinMaxUnitsNeeded",
			ClampMin = "0"))
	int32 MaxUnitsNeededOverwrite = 0;

	// Disable when an action should command units but never influence future production demand.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	bool bM_ContributesTrainingPressure = true;

	// Designer-authored broad unit-family desire used only when no missing-unit requirement gives a stronger answer.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	EAITrainingFocus M_FocusPressure = EAITrainingFocus::NoFocus;

	// Designer-authored role desire; NoTrainingPressure keeps specialty buckets untouched for this sub-action.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	EAITrainingFocusSpecialty M_SpecialtyPressure = EAITrainingFocusSpecialty::NoTrainingPressure;

	// Multiplies score/age pressure so important actions can prepare production earlier than low-stakes actions.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true, ClampMin = "0.0"))
	float M_BaseTrainingPressureAmount = 1.f;
	

private:
	float M_TimeStampLastExecution = 0.f;
	bool GetAreRequirementsMetForArray(
		const TArray<TObjectPtr<UStrategicAIActionRequirement>>& Requirements,
		const FStrategicAIBlackboard& RequirementContext,
		const float GameTimeSeconds) const;
	FString GetRequirementsDebugStringForArray(
		const TArray<TObjectPtr<UStrategicAIActionRequirement>>& Requirements,
		const FString& HeaderText) const;
	void AddRequirementSelectionRulesToPolicy(
		const TArray<TObjectPtr<UStrategicAIActionRequirement>>& Requirements,
		FIdleUnitSelectionPolicy& SelectionPolicy) const;
	/**
	 * @brief Blocks pressure from plans that cannot run for reasons training cannot solve.
	 * @param Requirements Native or designer gates that decide whether pressure is meaningful.
	 * @param Blackboard Current strategic context used by target/time/location gates.
	 * @param GameTimeSeconds Current world time for time-gated requirements.
	 * @return True when training pressure should be suppressed for this requirement set.
	 */
	bool GetIsBlockedByTrainingPressureRequirement(
		const TArray<TObjectPtr<UStrategicAIActionRequirement>>& Requirements,
		const FStrategicAIBlackboard& Blackboard,
		const float GameTimeSeconds) const;

	/**
	 * @brief Converts missing unit requirements into pressure instead of letting blocked actions stay invisible.
	 * @param Requirements Native or designer requirements scanned for missing-unit demand.
	 * @param Blackboard Current strategic context used to see whether the requirement is already satisfied.
	 * @param GameTimeSeconds Current world time used for age-weighted pressure.
	 * @param OutPressureContributions Receives one missing-unit reason when production can help.
	 * @param bOutBlockedByNonUnitRequirement True when a non-production gate made pressure invalid.
	 * @return True when this requirement set contains unit requirements and base pressure should not also be added.
	 */
	bool TryBuildMissingUnitRequirementPressureContributions(
		const TArray<TObjectPtr<UStrategicAIActionRequirement>>& Requirements,
		const FStrategicAIBlackboard& Blackboard,
		const float GameTimeSeconds,
		TArray<FEnemyStrategicTrainingPressureContribution>& OutPressureContributions,
		bool& bOutBlockedByNonUnitRequirement) const;

	/**
	 * @brief Adds designer-authored fallback pressure for actions that are not waiting on missing units.
	 * @param OutPressureContributions Receives the fallback action desire when it is configured to matter.
	 * @param GameTimeSeconds Current world time used to increase pressure from long-neglected plans.
	 */
	void AddBaseTrainingPressureContribution(
		TArray<FEnemyStrategicTrainingPressureContribution>& OutPressureContributions,
		const float GameTimeSeconds) const;

	/**
	 * @brief Ages stale plans so the AI eventually revisits needs that were not recently executed.
	 * @param GameTimeSeconds Current world time compared against the last sub-action execution time.
	 * @return Final scalar pressure before focus/specialty bucket routing.
	 */
	float GetTrainingPressureAmount(const float GameTimeSeconds) const;

	/**
	 * @brief Applies either mission fallback min/max or SubAction override values to selection policy.
	 * @param Blackboard Provides mission-level fallback min/max values.
	 * @param SelectionPolicy Policy instance that receives the chosen min/max setup.
	 */
	void SetupFallbackMinMaxSelectionPolicy(
		const FStrategicAIBlackboard& Blackboard,
		FIdleUnitSelectionPolicy& SelectionPolicy) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true, ClampMin = "0.0"))
	float M_Score = 1.0f;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	TArray<TObjectPtr<UStrategicAIActionRequirement>> M_Requirements;

	// Native requirements are visible for designer clarity, but only C++ classes decide what is always enforced.
	UPROPERTY(VisibleAnywhere, Instanced, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	TArray<TObjectPtr<UStrategicAIActionRequirement>> M_NativeVisibleRequirements;


};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API USubAction_AttackMoveToPlayerUnits final : public UStrategicAISubAction
{
	GENERATED_BODY()

public:
	USubAction_AttackMoveToPlayerUnits();

	virtual FString GetDebugString() const override;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API USubAction_AttackMoveToPlayerHQ final : public UStrategicAISubAction
{
	GENERATED_BODY()

public:
	USubAction_AttackMoveToPlayerHQ();

	virtual FString GetDebugString() const override;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API USubAction_AttackMoveToPlayerResourceBuildings final : public UStrategicAISubAction
{
	GENERATED_BODY()

public:
	USubAction_AttackMoveToPlayerResourceBuildings();

	virtual FString GetDebugString() const override;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API USubAction_AttackSpecificPoints final : public UStrategicAISubAction
{
	GENERATED_BODY()

public:
	USubAction_AttackSpecificPoints();

	const TArray<FVector>& GetTargetPoint() const
	{
		return TargetPoints;
	}

	virtual FString GetDebugString() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true, MakeEditWidget = true))
	TArray<FVector> TargetPoints = {};
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API USubAction_DefendBase final : public UStrategicAISubAction
{
	GENERATED_BODY()

public:
	USubAction_DefendBase();

	virtual FString GetDebugString() const override;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API USubAction_DefendImportantMissionPoint final : public UStrategicAISubAction
{
	GENERATED_BODY()

public:
	USubAction_DefendImportantMissionPoint();

	virtual FString GetDebugString() const override;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API USubAction_LightTanksAttackPlayerUnits final : public UStrategicAISubAction
{
	GENERATED_BODY()

public:
	USubAction_LightTanksAttackPlayerUnits();

	virtual FString GetDebugString() const override;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API USubAction_HeavyTankPushPlayerBaseOrUnits final : public UStrategicAISubAction
{
	GENERATED_BODY()

public:
	USubAction_HeavyTankPushPlayerBaseOrUnits();

	virtual FString GetDebugString() const override;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API USubAction_FlankPlayerHeavies final : public UStrategicAISubAction
{
	GENERATED_BODY()

public:
	USubAction_FlankPlayerHeavies();

	virtual FString GetDebugString() const override;
};
