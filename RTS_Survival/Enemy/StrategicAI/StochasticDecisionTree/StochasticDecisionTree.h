#pragma once

#include "CoreMinimal.h"

#include "StochasticDecisionTree.generated.h"

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

	FString GetDebugString()const;

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
		AEnemyController* EnemyController);

	void DecisionTree_ThinkStep(const float GameTimeSeconds, const FStrategicAIBlackboard& Blackboard);

private:

	bool EnsureHasAnyValidActions(const TArray<const FStrategicAIAction*> ValidActions) const;
	void DebugActions(const TArray<const FStrategicAIAction*> ValidActions) const;
	bool EnsurePickedActionIsValid(const FStrategicAIAction* PickedAction)const;
	bool EnsurePickedSubActionIsValid(const UStrategicAISubAction* PickedSubAction) const;
	void ExecuteSubAction(const UStrategicAISubAction* SubAction, const FStrategicAIBlackboard& Blackboard) const;
	
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
	TWeakObjectPtr<AEnemyController> M_EnemyController = nullptr;

	[[nodiscard]] bool EnsureIsValidEnemyController() const;
};
