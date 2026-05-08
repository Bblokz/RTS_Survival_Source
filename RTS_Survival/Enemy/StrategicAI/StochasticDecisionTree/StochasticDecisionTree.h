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



/**
 * @brief Tuned by designers to split long strategic attack moves into cheaper, easier-to-follow path chunks.
 */
USTRUCT(BlueprintType)
struct FStochasticPathFindingSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="100.0"))
	float AveragePathPointDistance = 10000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0"))
	int32 MaxIntermediatePathPoints = 12;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.1"))
	float ProjectionScale = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.0"))
	float MinimumFinalSegmentDistance = 2000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.0"))
	float DebugDrawZOffset = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.0"))
	float DebugSphereRadius = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="4"))
	int32 DebugSphereSegments = 12;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.0"))
	float DebugDrawDuration = 5.f;
};

/**
 * @brief Used by the enemy strategic AI component to pick scored sub-actions and assign idle units to the chosen order.
 */
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
	/**
	 * @brief Keeps HQ attack waves on chunked paths while preserving existing fallback waypoint behavior.
	 * @param SubAction Chosen action that decides whether HQ path resampling applies.
	 * @param StartLocation Average picked-unit location after the projection attempt.
	 * @param bStartLocationIsProjected Whether non-HQ fallback may safely prepend the start point.
	 * @param AttackLocations Candidate attack targets to sort from the wave start.
	 * @param OutWayPoints Final movement route for the formation controller.
	 * @param OutFinalMoveRotation Facing direction based on the final waypoint.
	 */
	void BuildAttackMoveWayPoints(
		const UStrategicAISubAction* SubAction,
		const FVector& StartLocation,
		bool bStartLocationIsProjected,
		TArray<FVector> AttackLocations,
		TArray<FVector>& OutWayPoints,
		FRotator& OutFinalMoveRotation) const;
	/**
	 * @brief Isolates HQ path generation so failed nav path work can fall back to the old direct HQ target.
	 * @param StartLocation Average picked-unit location used as the path start.
	 * @param TargetLocation Projected player HQ location used as the path end.
	 * @param OutWayPoints Start, sampled path points, and final target when pathfinding succeeds.
	 * @return True when a usable HQ path was generated.
	 */
	bool TryBuildPlayerHQAttackPath(
		const FVector& StartLocation,
		const FVector& TargetLocation,
		TArray<FVector>& OutWayPoints) const;
	TArray<TPair<FVector, TWeakObjectPtr<AActor>>> GetTotalAggregatedWeakActorLocations(const TArray<FWeakActorLocations>& WeakActorLocations) const;
	bool EnsureHasNonZeroPickedUnits(const FBlackboardIdleUnitsResult& PickedUnits, const FString& DebugContext);
	int32 GetMaxFormationWidthForPickedUnits(const FBlackboardIdleUnitsResult& PickedUnits) const;
	FAttackMoveWaveSettings M_AttackMoveWaveSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess="true"))
	FStochasticPathFindingSettings M_AttackMovePlayerHQPathFindingSettings;
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
