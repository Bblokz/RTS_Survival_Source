#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Enemy/StrategicAI/IdleUnitSelectionPolicy.h"
#include "RTS_Survival/Enemy/TrainingAndUnitCreation/StrategicTrainingState/EnemyTrainingPressureTypes.h"
#include "RTS_Survival/Units/SquadController.h"
#include "UObject/Object.h"
#include "StrategicAIActionRequirements.generated.h"

struct FStrategicAIBlackboard;
class AActor;

// DO NOT USE IN BLUEPRINTS; use the derived requirements!
UCLASS(Abstract, BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UStrategicAIActionRequirement : public UObject
{
	GENERATED_BODY()

public:
	virtual bool GetIsRequirementMet(const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds) const
	{
		return true;
	}
	virtual void ContributeToIdleUnitSelectionPolicy(FIdleUnitSelectionPolicy& SelectionPolicy) const
	{
	}
	/**
	 * @brief Prevents target/time/location gates from creating impossible production demand.
	 * @param Blackboard Strategic context used to decide whether production could help.
	 * @param GameTimeSeconds Current time for requirements that unlock later in the match.
	 * @return True when unmet requirement should suppress pressure instead of creating it.
	 */
	virtual bool GetShouldBlockTrainingPressureWhenUnmet(
		const FStrategicAIBlackboard& Blackboard,
		const float GameTimeSeconds) const
	{
		return not GetIsRequirementMet(Blackboard, GameTimeSeconds);
	}


	/**
	 * @brief Marks requirements that production can solve so missing units become bucket pressure.
	 * @return True when unmet state should be converted into training pressure.
	 */
	virtual bool GetIsUnitTrainingPressureRequirement() const
	{
		return false;
	}


	/**
	 * @brief Routes missing-unit debt into buckets while preserving the sub-action's tactical specialty.
	 * @param OutPressureContributions Receives the pressure reason to store on the training state.
	 * @param PressureAmount Age/score-scaled pressure supplied by the sub-action.
	 * @param SourceDebugName Human-readable action/requirement reason for bucket debugging.
	 * @param BaseSpecialtyPressure Specialty selected by the sub-action before the requirement adds focus pressure.
	 */
	virtual void AddMissingUnitTrainingPressureContribution(
		TArray<FEnemyStrategicTrainingPressureContribution>& OutPressureContributions,
		const float PressureAmount,
		const FString& SourceDebugName,
		const EAITrainingFocusSpecialty BaseSpecialtyPressure) const
	{
	}
	virtual FString GetDebugString() const;
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UStrategicAIGameTimePassedRequirement final : public UStrategicAIActionRequirement
{
	GENERATED_BODY()

public:
	virtual bool GetIsRequirementMet(const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds) const override;

	float GetRequiredGameTimeSeconds() const
	{
		return RequiredGameTimeSeconds;
	}
	virtual FString GetDebugString() const override;
	// Delays pressure and execution so the AI does not train for late-game plans too early.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess=true, ClampMin="0.0"))
	float RequiredGameTimeSeconds = 0.0f;
	

private:
};

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UStrategicAIActorIsValidRequirement final : public UStrategicAIActionRequirement
{
	GENERATED_BODY()

public:
	virtual bool GetIsRequirementMet(const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds) const override;

	AActor* GetRequiredActor() const
	{
		return RequiredActor;
	}
	// Blocks pressure when a required designer-placed target is missing, because training cannot fix level setup.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<AActor> RequiredActor = nullptr;
	

	bool GetIsValidRequiredActor() const;

	virtual FString GetDebugString() const override;

};

/**
 * @brief Used by strategic actions that need a known player HQ target before running.
 */
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UStrategicAIHasPlayerHQLocationRequirement final : public UStrategicAIActionRequirement
{
	GENERATED_BODY()

public:
	virtual bool GetIsRequirementMet(const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds) const override;

	virtual FString GetDebugString() const override;
};

/**
 * @brief Used by strategic actions that need at least one known player resource building target.
 */
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UStrategicAIHasPlayerResourceBuildingLocationsRequirement final : public UStrategicAIActionRequirement
{
	GENERATED_BODY()

public:
	virtual bool GetIsRequirementMet(const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds) const override;

	virtual FString GetDebugString() const override;
};



/**
 * @brief Used by strategic actions to require a minimum number of idle tanks and squads of any subtype.
 */
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UStrategicAIHasAtLeastAnyIdleUnits final : public UStrategicAIActionRequirement
{
	GENERATED_BODY()

public:
	virtual bool GetIsRequirementMet(const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds) const override;
	virtual void ContributeToIdleUnitSelectionPolicy(FIdleUnitSelectionPolicy& SelectionPolicy) const override;
	virtual bool GetShouldBlockTrainingPressureWhenUnmet(
		const FStrategicAIBlackboard& Blackboard,
		const float GameTimeSeconds) const override;
	virtual bool GetIsUnitTrainingPressureRequirement() const override;
	virtual void AddMissingUnitTrainingPressureContribution(
		TArray<FEnemyStrategicTrainingPressureContribution>& OutPressureContributions,
		const float PressureAmount,
		const FString& SourceDebugName,
		const EAITrainingFocusSpecialty BaseSpecialtyPressure) const override;

	// Generic idle-unit count spreads pressure because any trainable family can help satisfy it.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 AmountIdleNeeded = 1;

	virtual FString GetDebugString() const override;
};

/**
 * @brief Used by strategic actions that require a specific infantry squad to make the plan meaningful.
 */
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UStrategicAIHasAtLeastIdleSquads final : public UStrategicAIActionRequirement
{
	GENERATED_BODY()

public:
	virtual bool GetIsRequirementMet(const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds) const override;
	virtual void ContributeToIdleUnitSelectionPolicy(FIdleUnitSelectionPolicy& SelectionPolicy) const override;
	virtual bool GetShouldBlockTrainingPressureWhenUnmet(
		const FStrategicAIBlackboard& Blackboard,
		const float GameTimeSeconds) const override;
	virtual bool GetIsUnitTrainingPressureRequirement() const override;
	virtual void AddMissingUnitTrainingPressureContribution(
		TArray<FEnemyStrategicTrainingPressureContribution>& OutPressureContributions,
		const float PressureAmount,
		const FString& SourceDebugName,
		const EAITrainingFocusSpecialty BaseSpecialtyPressure) const override;

	// Specific squad demand maps blocked actions back to infantry production pressure.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ESquadSubtype RequiredSquadSubtype = ESquadSubtype::Squad_None;

	// Higher required squad counts intentionally create stronger infantry pressure.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 AmountIdleSpecificSquadsNeeded = 1;
	
	virtual FString GetDebugString() const override;

};

/**
 * @brief Used by strategic actions to require a minimum number of idle tanks of one subtype.
 */
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UStrategicAIHasAtLeastIdleTanks final : public UStrategicAIActionRequirement
{
	GENERATED_BODY()

public:
	virtual bool GetIsRequirementMet(const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds) const override;
	virtual void ContributeToIdleUnitSelectionPolicy(FIdleUnitSelectionPolicy& SelectionPolicy) const override;
	virtual bool GetShouldBlockTrainingPressureWhenUnmet(
		const FStrategicAIBlackboard& Blackboard,
		const float GameTimeSeconds) const override;
	virtual bool GetIsUnitTrainingPressureRequirement() const override;
	virtual void AddMissingUnitTrainingPressureContribution(
		TArray<FEnemyStrategicTrainingPressureContribution>& OutPressureContributions,
		const float PressureAmount,
		const FString& SourceDebugName,
		const EAITrainingFocusSpecialty BaseSpecialtyPressure) const override;

	// Specific tank subtype is reduced to light/medium/heavy focus pressure for future tech-aware selection.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ETankSubtype RequiredTankSubtype = ETankSubtype::Tank_None;

	// Larger missing tank groups should pull harder on the matching tank focus bucket.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 AmountIdleOfSpecificTanksNeeded = 1;

	virtual FString GetDebugString() const override;
};


/**
 * @brief Used by strategic actions to require a minimum number of idle tanks of any subtype.
 */
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UStrategicAIHasAtLeastAnyIdleTanks final : public UStrategicAIActionRequirement
{
	GENERATED_BODY()

public:
	virtual bool GetIsRequirementMet(const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds) const override;
	virtual void ContributeToIdleUnitSelectionPolicy(FIdleUnitSelectionPolicy& SelectionPolicy) const override;
	virtual bool GetShouldBlockTrainingPressureWhenUnmet(
		const FStrategicAIBlackboard& Blackboard,
		const float GameTimeSeconds) const override;
	virtual bool GetIsUnitTrainingPressureRequirement() const override;
	virtual void AddMissingUnitTrainingPressureContribution(
		TArray<FEnemyStrategicTrainingPressureContribution>& OutPressureContributions,
		const float PressureAmount,
		const FString& SourceDebugName,
		const EAITrainingFocusSpecialty BaseSpecialtyPressure) const override;

	// Any-tank requirements default to medium tank pressure as a flexible future-production fallback.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 AmountIdleTanksNeeded = 3;

	virtual FString GetDebugString() const override;
};

/**
 * @brief Used by strategic actions to require a minimum number of idle aircraft of one subtype.
 */
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UStrategicAIHasAtLeastIdleAircraft final : public UStrategicAIActionRequirement
{
	GENERATED_BODY()

public:
	virtual bool GetIsRequirementMet(const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds) const override;

	// Aircraft is currently only a gate because this training pressure system does not yet choose aircraft options.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EAircraftSubtype RequiredAircraftSubtype = EAircraftSubtype::Aircarft_None;

	// Aircraft count remains visible to designers even though it does not emit unit pressure yet.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 AmountIdleAircraftNeeded = 1;

	virtual FString GetDebugString() const override;
};


// By default set to 4 light tanks needed.
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UStrategicAIHasEnoughLightTanks final : public UStrategicAIActionRequirement
{
	GENERATED_BODY()

public:
	virtual bool GetIsRequirementMet(const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds) const override;
	virtual void ContributeToIdleUnitSelectionPolicy(FIdleUnitSelectionPolicy& SelectionPolicy) const override;
	virtual bool GetShouldBlockTrainingPressureWhenUnmet(
		const FStrategicAIBlackboard& Blackboard,
		const float GameTimeSeconds) const override;
	virtual bool GetIsUnitTrainingPressureRequirement() const override;
	virtual void AddMissingUnitTrainingPressureContribution(
		TArray<FEnemyStrategicTrainingPressureContribution>& OutPressureContributions,
		const float PressureAmount,
		const FString& SourceDebugName,
		const EAITrainingFocusSpecialty BaseSpecialtyPressure) const override;

	// Missing light tank groups create proportional pressure for fast/harass-capable production.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 AmountIdleLightTanksNeeded =5;
	
	virtual FString GetDebugString() const override;

};


// By default set to two heavy tanks needed.
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UStrategicAIHasEnoughHeavyTanks final : public UStrategicAIActionRequirement
{
	GENERATED_BODY()

public:
	virtual bool GetIsRequirementMet(const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds) const override;
	virtual void ContributeToIdleUnitSelectionPolicy(FIdleUnitSelectionPolicy& SelectionPolicy) const override;
	virtual bool GetShouldBlockTrainingPressureWhenUnmet(
		const FStrategicAIBlackboard& Blackboard,
		const float GameTimeSeconds) const override;
	virtual bool GetIsUnitTrainingPressureRequirement() const override;
	virtual void AddMissingUnitTrainingPressureContribution(
		TArray<FEnemyStrategicTrainingPressureContribution>& OutPressureContributions,
		const float PressureAmount,
		const FString& SourceDebugName,
		const EAITrainingFocusSpecialty BaseSpecialtyPressure) const override;

	// Missing heavy tank groups create proportional pressure for breakthrough-capable production.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 AmountIdleHeavyTanksNeeded = 2;
	
	virtual FString GetDebugString() const override;

};

// By default set to one player heavy tank needed.
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UStrategicAIDoesPlayerHaveHeavyTanks final : public UStrategicAIActionRequirement
{
	GENERATED_BODY()

public:
	virtual bool GetIsRequirementMet(const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds) const override;

	// Player heavy threshold gates counter-plans without directly forcing production pressure by itself.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 AmountPlayerHeaviesNeeded = 1;
	
	virtual FString GetDebugString() const override;

};


// By default set to one player heavy tank needed.
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UStrategicAIDoesBlackboardHaveHeavyTankFlankPositions final : public UStrategicAIActionRequirement
{
	GENERATED_BODY()
public:
	

public:
	virtual bool GetIsRequirementMet(const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds) const override;

	virtual FString GetDebugString() const override;

};
