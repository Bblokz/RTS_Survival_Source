#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Enemy/EnemyWaves/AttackWave.h"
#include "RTS_Survival/Enemy/StrategicAI/StrategicAIBlackboard.h"
#include "RTS_Survival/Enemy/StrategicAI/BlackboardIdleUnitsResult/FBlackboardIdleUnitsResult.h"
#include "RTS_Survival/Enemy/StrategicAI/StrategicActions/StrategicActions.h"

#include "StochasticDecisionTree.generated.h"

struct FBlackboardIdleUnitsResult;
class UEnemyFormationController;
class UEnemyNavigationAIComponent;
struct FStrategicAIBlackboard;
class UEnemyStrategicAIComponent;
class AEnemyController;
class UEnemyDirectControlComponent;
class UStrategicAISubAction;


/**
 * This defines the main behaviour picked at this strategic AI action tick; a bundle of sub-actions will be defined
 * in the data asset of a mission that can be picked based on these.
 */
UENUM(BlueprintType)
enum class EStrategicAITopLevelAction : uint8
{
	Attack,
	Defend,
};

// A main action the AI can take; has a bundle of sub-actions defined, the score contribures to whether this action is picked or not.
USTRUCT(BlueprintType)
struct FStrategicAIAction
{
	GENERATED_BODY()

	EStrategicAITopLevelAction GetActionEnum() const
	{
		return M_Action;
	}

	float GetScore() const
	{
		return M_Score;
	}

	const TArray<TObjectPtr<UStrategicAISubAction>>& GetSubActions() const
	{
		return M_SubActions;
	}

	FString GetDebugString() const;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess=true))
	EStrategicAITopLevelAction M_Action = EStrategicAITopLevelAction::Attack;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess=true, ClampMin="0.0"))
	float M_Score = 1.0f;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, meta=(AllowPrivateAccess=true))
	TArray<TObjectPtr<UStrategicAISubAction>> M_SubActions;
};


// The main brain component on the strategic AI, defines the main actions and sub-actions the AI can pick from,
// as well as the scores of each action to influence their chances of being picked.
// The main idea is to have a separate component doing the unit creation and adding those units to the blackboard as idle,
// then after picking an action this component will go through those idle units and assign them to their action
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UStochasticDecisionTree final : public UObject
{
	GENERATED_BODY()

public:
	UStochasticDecisionTree();

	const TArray<FStrategicAIAction>& GetActionDefinitions() const
	{
		return M_ActionDefinitions;
	}

	void InitStochasticDecisionTree(
		UEnemyStrategicAIComponent* StrategicComponentWithBB,
		UEnemyDirectControlComponent* DirectControlComponent,
		UEnemyNavigationAIComponent* EnemyNavigationAI,
		UEnemyFormationController* EnemyFormationController, AEnemyController* EnemyController);

	// Note that prior to calling this all the idle units in the blackboard are RTSIsValid and
	// all arrays of location vectors are check for near zero.
	void DecisionTree_ThinkStep(const float GameTimeSeconds, FStrategicAIBlackboard& Blackboard);

private:
	bool EnsureHasAnyValidActions(const TArray<const FStrategicAIAction*> ValidActions) const;
	bool EnsurePickedActionIsValid(const FStrategicAIAction* PickedAction) const;
	bool EnsurePickedSubActionIsValid(const UStrategicAISubAction* PickedSubAction) const;
	void ExecuteSubAction(const UStrategicAISubAction* SubAction, const FStrategicAIBlackboard& Blackboard);

	void Exe_AttackMovePlayerUnits(const UStrategicAISubAction* SubAction, const FStrategicAIBlackboard& Blackboard);
	void Exe_AttackMovePlayerHQ(const UStrategicAISubAction* SubAction, const FStrategicAIBlackboard& Blackboard);
	void Exe_AttackMovePlayerResourceBuildings(const UStrategicAISubAction* SubAction,
	                                           const FStrategicAIBlackboard& Blackboard);
	void Exe_AttackMoveSpecificPoint(const UStrategicAISubAction* SubAction, const FStrategicAIBlackboard& Blackboard);
	void Exe_AttackMoveLightTanksToPlayerUnits(const UStrategicAISubAction* SubAction,
	                                           const FStrategicAIBlackboard& Blackboard);
	void Exe_HeavyTankPushPlayerBaseOrUnits(const UStrategicAISubAction* SubAction,
	                                        const FStrategicAIBlackboard& Blackboard);
	void Exe_FlankPlayerHeavies(const UStrategicAISubAction* SubAction, const FStrategicAIBlackboard& Blackboard);

	void Exe_DefendBase(const UStrategicAISubAction* SubAction, const FStrategicAIBlackboard& Blackboard);


	// ------------------- Formation Logic Using Blackboard Idle units ------------------------
	void CreateAttackMoveFormation(
		const UStrategicAISubAction* SubAction,
		TArray<FVector> AttackLocations,
		const FStrategicAIBlackboard& Blackboard);
	void CreateFlankingAttack(
		const UStrategicAISubAction* SubAction,
		const TArray<FWeakActorLocations>& FlankingPositions,
		const FStrategicAIBlackboard& Blackboard);
	TArray<TPair<FVector, TWeakObjectPtr<AActor>>> GetTotalAggregatedWeakActorLocations(const TArray<FWeakActorLocations>& WeakActorLocations) const;
	bool EnsureHasNonZeroPickedUnits(const FBlackboardIdleUnitsResult& PickedUnits, const FString& DebugContext);
	int32 GetMaxFormationWidthForPickedUnits(const FBlackboardIdleUnitsResult& PickedUnits) const;
	FAttackMoveWaveSettings M_AttackMoveWaveSettings;
	// Multiplies with the inner radius of the unit's RTS component to determine spacing in formation.
	float M_AttackMoveWave_FormationOffsetMultiplier = 1.5f;

	// ------------------- Issue Orders to Picked Blackboard Units ------------------------
	TArray<ICommands*> GetCommandsArrayOfUnits(const FBlackboardIdleUnitsResult& PickedUnits) const;
	bool IssueMoveOrder(ICommands* UnitToCommand, const FVector& TargetLocation, const bool bResetOrderQueue = false,
	                    const FRotator& FinishedMovementRotation = {}) const;
	bool IssueAttackOrder(ICommands* UnitToCommand, TWeakObjectPtr<AActor> TargetActor,
	                      const bool bResetOrderQueue = false) const;
	bool IssueRegisterWithBlackboardOrder(ICommands* UnitToCommand, const bool bResetOrderQueue = false) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess="true"))
	TArray<FStrategicAIAction> M_ActionDefinitions;

	const TArray<const FStrategicAIAction*> GetActionsWithValidSubActions(
		const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds) const;

	const TArray<const UStrategicAISubAction*> GetSubActionsThatHaveMetRequirements(
		const FStrategicAIAction& Action,
		const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds) const;

	void CacheGenerationSeedFromGameInstance();
	// Generation seed from game instance.
	int32 M_CachedGenerationSeed = 0;
	bool bM_UseCachedGenerationSeed = true;

	UPROPERTY()
	TWeakObjectPtr<UEnemyStrategicAIComponent> M_StrategicComponent = nullptr;
	[[nodiscard]] bool EnsureStrategicComponentIsValid() const;

	UPROPERTY()
	TWeakObjectPtr<UEnemyDirectControlComponent> M_DirectControlComponent = nullptr;

	[[nodiscard]] bool EnsureIsValidDirectControlComponent() const;

	UPROPERTY()
	TWeakObjectPtr<UEnemyNavigationAIComponent> M_EnemyNavigationAIComponent = nullptr;

	[[nodiscard]] bool EnsureIsValidEnemyNavigationAIComponent() const;

	UPROPERTY()
	TWeakObjectPtr<UEnemyFormationController> M_EnemyFormationController = nullptr;

	[[nodiscard]] bool EnsureIsValidEnemyFormation() const;

	UPROPERTY()
	TWeakObjectPtr<AEnemyController> M_EnemyController = nullptr;

	[[nodiscard]] bool EnsureIsValidEnemyController() const;

	// ------------ Utils --------------------
	TArray<FVector> GetProjectedPlayerBulkLocations(const FStrategicAIBlackboard& Blackboard);
	TArray<FVector> GetProjectedPlayerAvgLocationAttackers(const FStrategicAIBlackboard& Blackboard) const;
	TArray<FVector> GetProjectedLocationsUnderAttack(const FStrategicAIBlackboard& Blackboard) const;
	TArray<FVector> GetProjectedPlayerHQLocation(const FStrategicAIBlackboard& Blackboard) const;
	TArray<FVector> GetProjectedPlayerResourceBuildings(const FStrategicAIBlackboard& Blackboard) const;
	TArray<FVector> GetProjectedPlayerBaseLocations(const FStrategicAIBlackboard& Blackboard) const;
	TArray<FVector> GetProjectedAttackSpecificPoints(const TArray<FVector>& TargetPoints) const;
	TArray<FWeakActorLocations> GetProjectedAggregatedHeavyTankFlankingPositions(
		const FStrategicAIBlackboard& Blackboard) const;
	FWeakActorLocations ProjectHeavyTankFlankLocations(const FWeakActorLocations& FlankLocations) const;


	// Debug call over all actions regardless of requirements.
	void DebugAllActionsPriorReqCheck() const;
	// Called After removing those main actions of which all sub actions have no requirements met.
	void DebugValidActions(const TArray<const FStrategicAIAction*> ValidActions) const;
	void DebugAttackLocations(const TArray<FVector>& Locations, const FString& DebugContext) const;
	void DebugPickedLocation(const FVector& PickedLocation, const FString& DebugContext) const;
	void DebugFlankPositions(const TArray<FWeakActorLocations>& FlankPositions, const FString& DebugContext) const;
	void DebugPoint(const FVector& Point, const float Radius,
	                const FColor& Color, const float Duration, const FString& Text) const;
	void DebugPickedUnitsAndWayPoints(const FBlackboardIdleUnitsResult& Picked,
	                                  TArray<FVector> Waypoints);
};
