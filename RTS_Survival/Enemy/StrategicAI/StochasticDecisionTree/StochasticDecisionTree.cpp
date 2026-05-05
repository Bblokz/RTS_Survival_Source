#include "StochasticDecisionTree.h"

#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Enemy/EnemyAISettings/EnemyAISettings.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyDirectControlComponent/EnemyDirectControlComponent.h"
#include "RTS_Survival/Enemy/StrategicAI/Component/EnemyStrategicAIComponent.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "StochasticHelpers/StochasticHelpers.h"

FString FStrategicAIAction::GetDebugString() const
{
	FString Debug = "--- Top level action : " + StochasticHelpers::GetStrategicAITopLevelActionAsString(M_Action) +
		" ---";
	for (const auto EachAction : M_SubActions)
	{
		FString SubActionString = EachAction->GetDebugString();
		SubActionString = "\n --" + SubActionString;
		Debug += SubActionString;
	}
	return Debug;
}

UStochasticDecisionTree::UStochasticDecisionTree()
{
}

void UStochasticDecisionTree::InitStochasticDecisionTree(
	UEnemyStrategicAIComponent* StrategicComponentWithBB,
	UEnemyDirectControlComponent* DirectControlComponent,
	AEnemyController* EnemyController)
{
	M_StrategicComponent = StrategicComponentWithBB;
	M_DirectControlComponent = DirectControlComponent;
	M_EnemyController = EnemyController;
	(void)EnsureStrategicComponentIsValid();
	(void)EnsureIsValidDirectControlComponent();
	(void)EnsureIsValidEnemyController();
}

void UStochasticDecisionTree::DecisionTree_ThinkStep(const float GameTimeSeconds,
                                                     const FStrategicAIBlackboard& Blackboard)
{
	// Filter top level actions to only get those of which we do have sub-actions to pick from that are not
	// blocked by requirements.
	const TArray<const FStrategicAIAction*> ValidActions =
		GetActionsWithValidSubActions(Blackboard, GameTimeSeconds);
	if (not EnsureHasAnyValidActions(ValidActions))
	{
		return;
	}
	const FStrategicAIAction* PickedAction = StochasticHelpers::PickStrategicAction(
		ValidActions, bM_UseCachedGenerationSeed, M_CachedGenerationSeed, GameTimeSeconds);
	if (not EnsurePickedActionIsValid(PickedAction))
	{
		return;
	}
	// const TArray<>
	// const UStrategicAISubAction* PickedSubAction = StochasticHelpers::PickSubAction(
	// 	PickedAction->GetSubActions(), bM_UseCachedGenerationSeed, M_CachedGenerationSeed, GameTimeSeconds);
}

bool UStochasticDecisionTree::EnsureHasAnyValidActions(const TArray<const FStrategicAIAction*> ValidActions) const
{
	if (ValidActions.IsEmpty())
	{
		if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
			EnemyAISettings::Debugging::StochasticDecisionTreeDebugging)
		{
			RTSFunctionLibrary::PrintString("Stochastic tree has no actions with any valid sub-actions to pick,"
			                                "possibly all are blocked by requirements", FColor::Red, 2.f);
		}
		return false;
	}
	if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
		EnemyAISettings::Debugging::StochasticDecisionTreeDebugging)
	{
		DebugActions(ValidActions);
	}
	return true;
}

void UStochasticDecisionTree::DebugActions(const TArray<const FStrategicAIAction*> ValidActions) const
{
}

bool UStochasticDecisionTree::EnsurePickedActionIsValid(const FStrategicAIAction* PickedAction) const
{
	if (not PickedAction)
	{
		RTSFunctionLibrary::ReportError("Stochastic tree cannot use action as it picked a null ptr!");
		return false;
	}
	if (PickedAction->GetSubActions().IsEmpty())
	{
		const FString ActionName = StochasticHelpers::GetStrategicAITopLevelActionAsString(
			PickedAction->GetActionEnum());
		RTSFunctionLibrary::ReportError(
			FString::Printf(TEXT("Stochastic tree cannot use action %s as it has no sub-actions!"), *ActionName));
		return false;
	}
	return true;
}

const TArray<const FStrategicAIAction*> UStochasticDecisionTree::GetActionsWithValidSubActions(
	const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds) const
{
	TArray<const FStrategicAIAction*> ValidActions;

	if (M_ActionDefinitions.IsEmpty())
	{
		return ValidActions;
	}

	for (const FStrategicAIAction& Action : M_ActionDefinitions)
	{
		const TArray<const UStrategicAISubAction*> ValidSubActions =
			GetValidSubActionsForAction(Action, Blackboard, GameTimeSeconds);

		if (ValidSubActions.IsEmpty())
		{
			continue;
		}

		ValidActions.Add(&Action);
	}

	return ValidActions;
}


const TArray<const UStrategicAISubAction*> UStochasticDecisionTree::GetValidSubActionsForAction(
	const FStrategicAIAction& Action,
	const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds) const
{
	TArray<const UStrategicAISubAction*> ValidSubActions;

	const TArray<TObjectPtr<UStrategicAISubAction>>& SubActions = Action.GetSubActions();
	if (SubActions.IsEmpty())
	{
		return ValidSubActions;
	}

	for (const TObjectPtr<UStrategicAISubAction>& SubAction : SubActions)
	{
		if (!IsValid(SubAction))
		{
			continue;
		}

		if (!SubAction->GetAreRequirementsMet(Blackboard, GameTimeSeconds))
		{
			continue;
		}

		ValidSubActions.Add(SubAction.Get());
	}

	return ValidSubActions;
}


void UStochasticDecisionTree::CacheGenerationSeedFromGameInstance()
{
	UWorld* const World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	UGameInstance* const GameInstance = World->GetGameInstance();
	URTSGameInstance* const RTSGameInstance = Cast<URTSGameInstance>(GameInstance);
	if (not IsValid(RTSGameInstance))
	{
		RTSFunctionLibrary::ReportError("Stochastic Decision Tree could not cache generation seed.");
		return;
	}

	const FCampaignGenerationSettings CampaignGenerationSettings = RTSGameInstance->GetCampaignGenerationSettings();
	M_CachedGenerationSeed = CampaignGenerationSettings.GenerationSeed;
}


bool UStochasticDecisionTree::EnsureIsValidDirectControlComponent() const
{
	if (not M_DirectControlComponent.IsValid())
	{
		RTSFunctionLibrary::ReportError(TEXT(
			"Stochastic decision tree failed to ensure valid direct control component because the direct control component is null."));
		return false;
	}
	return true;
}

bool UStochasticDecisionTree::EnsureIsValidEnemyController() const
{
	if (not M_EnemyController.IsValid())
	{
		RTSFunctionLibrary::ReportError(TEXT(
			"Stochastic decision tree failed to ensure valid enemy controller because the enemy controller is null."));
		return false;
	}
	return true;
}

bool UStochasticDecisionTree::EnsureStrategicComponentIsValid() const
{
	if (not M_StrategicComponent.IsValid())
	{
		RTSFunctionLibrary::ReportError(TEXT(
			"Stochastic decision tree failed to ensure valid strategic component because the strategic component is null."));
		return false;
	}
	return true;
}
