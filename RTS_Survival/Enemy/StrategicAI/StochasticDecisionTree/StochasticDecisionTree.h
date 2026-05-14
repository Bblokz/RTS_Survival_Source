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

	TArray<TObjectPtr<UStrategicAISubAction>>& GetSubActions()
	{
		return M_SubActions;
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
	bool EnsureHasAnyValidActions(const TArray<FStrategicAIAction*>& ValidActions) const;
	bool EnsurePickedActionIsValid(FStrategicAIAction* PickedAction) const;
	bool EnsurePickedSubActionIsValid(UStrategicAISubAction* PickedSubAction) const;
	void ExecuteSubAction(UStrategicAISubAction* SubAction, const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds);

	void Exe_AttackMovePlayerUnits(UStrategicAISubAction* SubAction, const FStrategicAIBlackboard& Blackboard);
	void Exe_AttackMovePlayerHQ(UStrategicAISubAction* SubAction, const FStrategicAIBlackboard& Blackboard);
	void Exe_AttackMovePlayerResourceBuildings(UStrategicAISubAction* SubAction,
	                                           const FStrategicAIBlackboard& Blackboard);
	void Exe_AttackMoveSpecificPoint(UStrategicAISubAction* SubAction, const FStrategicAIBlackboard& Blackboard);
	void Exe_AttackMoveLightTanksToPlayerUnits(UStrategicAISubAction* SubAction,
	                                           const FStrategicAIBlackboard& Blackboard);
	void Exe_HeavyTankPushPlayerBaseOrUnits(UStrategicAISubAction* SubAction,
	                                        const FStrategicAIBlackboard& Blackboard);
	void Exe_FlankPlayerHeavies(UStrategicAISubAction* SubAction, const FStrategicAIBlackboard& Blackboard);

	void Exe_DefendBase(UStrategicAISubAction* SubAction, const FStrategicAIBlackboard& Blackboard);


	// ------------------- Formation Logic Using Blackboard Idle units ------------------------
	void CreateAttackMoveFormation(
		UStrategicAISubAction* SubAction,
		TArray<FVector> AttackLocations,
		const FStrategicAIBlackboard& Blackboard);
	void CreateFlankingAttack(
		UStrategicAISubAction* SubAction,
		const TArray<FWeakActorLocations>& FlankingPositions,
		const FStrategicAIBlackboard& Blackboard);
	/**
	 * @brief Builds general attack-move routes through navigation so every wave can share the same fallback rules.
	 * @param StartLocation Average picked-unit location after the projection attempt.
	 * @param bStartLocationIsProjected Whether the start can be safely preserved as the first waypoint.
	 * @param AttackLocations Candidate attack targets to sort from the wave start.
	 * @return Final movement route for the formation controller.
	 */
	TArray<FVector> BuildAttackMoveWayPoints(
		const FVector& StartLocation,
		bool bStartLocationIsProjected,
		TArray<FVector> AttackLocations) const;
	/**
	 * @brief Isolates attack path generation so failed nav path work can fall back to direct target waypoints.
	 * @param StartLocation Average picked-unit location used as the path start.
	 * @param TargetLocation First sorted attack location used as the path end.
	 * @param bStartLocationIsProjected Whether the path should preserve StartLocation as its first point.
	 * @param OutWayPoints Start/path points/final target when pathfinding succeeds.
	 * @return True when a usable attack path was generated.
	 */
	bool PathFindAttackPath(
		const FVector& StartLocation,
		const FVector& TargetLocation,
		bool bStartLocationIsProjected,
		TArray<FVector>& OutWayPoints) const;
	TArray<TPair<FVector, TWeakObjectPtr<AActor>>> GetTotalAggregatedWeakActorLocations(const TArray<FWeakActorLocations>& WeakActorLocations) const;
	bool EnsureHasNonZeroPickedUnits(const FBlackboardIdleUnitsResult& PickedUnits, const FString& DebugContext);
	int32 GetMaxFormationWidthForPickedUnits(const FBlackboardIdleUnitsResult& PickedUnits) const;
	FAttackMoveWaveSettings M_AttackMoveWaveSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess="true"))
	FStochasticPathFindingSettings M_AttackMovePathFindingSettings;
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

	TArray<FStrategicAIAction*> GetActionsWithValidSubActions(
		const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds);

	TArray<UStrategicAISubAction*> GetSubActionsThatHaveMetRequirements(
		FStrategicAIAction& Action,
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
	void DebugValidActions(const TArray<FStrategicAIAction*>& ValidActions) const;
	void DebugAttackLocations(const TArray<FVector>& Locations, const FString& DebugContext) const;
	void DebugPickedLocation(const FVector& PickedLocation, const FString& DebugContext) const;
	void DebugFlankPositions(const TArray<FWeakActorLocations>& FlankPositions, const FString& DebugContext) const;
	void DebugPoint(const FVector& Point, const float Radius,
	                const FColor& Color, const float Duration, const FString& Text) const;
	void DebugPickedUnitsAndWayPoints(const FBlackboardIdleUnitsResult& Picked,
	                                  TArray<FVector> Waypoints);
};
