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
	const TArray<const UStrategicAISubAction*> SubActionsRequirementsMet = GetSubActionsThatHaveMetRequirements(
		*PickedAction, Blackboard, GameTimeSeconds);
	const UStrategicAISubAction* PickedSubAction = StochasticHelpers::PickSubAction(SubActionsRequirementsMet,
		bM_UseCachedGenerationSeed, M_CachedGenerationSeed, GameTimeSeconds);
	if(not EnsurePickedSubActionIsValid(PickedSubAction))
	{
		return;
	}
	ExecuteSubAction(PickedSubAction, Blackboard);
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
		DebugValidActions(ValidActions);
	}
	return true;
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

bool UStochasticDecisionTree::EnsurePickedSubActionIsValid(const UStrategicAISubAction* PickedSubAction) const
{
	if (not PickedSubAction)
	{
		RTSFunctionLibrary::ReportError("Stochastic tree cannot use sub-action as it picked a null ptr!");
		return false;
	}
	if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
		EnemyAISettings::Debugging::StochasticDecisionTreeDebugging)
	{
		RTSFunctionLibrary::PrintString(
			FString::Printf(TEXT("Stochastic tree picked sub-action %s"), *PickedSubAction->GetDebugString()),
			FColor::Green, 5.f);		
	}
	return true;
}

void UStochasticDecisionTree::ExecuteSubAction(const UStrategicAISubAction* SubAction,
	const FStrategicAIBlackboard& Blackboard) const
{
	switch (SubAction->SubtypeAction) {
	case ESubtypeAction::DEFAULT_OBJECT:
		RTSFunctionLibrary::ReportError("Stochastic tree cannot execute sub-action as it is a default object, likely meaning it was not properly set up in the data assets.");
		break;
	case ESubtypeAction::AttackMoveToPlayerUnits:
		
		break;
	case ESubtypeAction::AttackMoveToPlayerHQ:
		break;
	case ESubtypeAction::AttackMoveToPlayerResourceBuildings:
		break;
	case ESubtypeAction::AttackMoveSpecificPoint:
		break;
	case ESubtypeAction::DefendBase:
		break;
	case ESubtypeAction::DefendImportantMissionPoint:
		break;
	}
}

void UStochasticDecisionTree::Exe_AttackMovePlayerUnits(const FStrategicAIBlackboard& Blackboard)
{
	
}

const TArray<const FStrategicAIAction*> UStochasticDecisionTree::GetActionsWithValidSubActions(
	const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds) const
{
	TArray<const FStrategicAIAction*> ValidActions;

	if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
		EnemyAISettings::Debugging::StochasticDecisionTreeDebugging)
	{
		DebugAllActionsPriorReqCheck();
	}

	if (M_ActionDefinitions.IsEmpty())
	{
		return ValidActions;
	}

	for (const FStrategicAIAction& Action : M_ActionDefinitions)
	{
		const TArray<const UStrategicAISubAction*> ValidSubActions =
			GetSubActionsThatHaveMetRequirements(Action, Blackboard, GameTimeSeconds);

		if (ValidSubActions.IsEmpty())
		{
			continue;
		}

		ValidActions.Add(&Action);
	}

	return ValidActions;
}


const TArray<const UStrategicAISubAction*> UStochasticDecisionTree::GetSubActionsThatHaveMetRequirements(
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

void UStochasticDecisionTree::DebugAllActionsPriorReqCheck() const
{
	FString DebugString = "Actions Prior Req Check:\n";
	for(auto EachMainAction : M_ActionDefinitions)
	{
		DebugString += EachMainAction.GetDebugString() + "\n";	
	}
		RTSFunctionLibrary::PrintString(DebugString, FColor::Purple, EnemyAISettings::Debugging::StochasticActionsDebugTime);
}

void UStochasticDecisionTree::DebugValidActions(const TArray<const FStrategicAIAction*> ValidActions) const
{
	FString DebugString = "\nValid Actions:";
	for(auto EachValidAction : ValidActions)
	{
		DebugString += "\n-- " + EachValidAction->GetDebugString();
	}
	RTSFunctionLibrary::PrintString(DebugString, FColor::Green, EnemyAISettings::Debugging::StochasticActionsDebugTime);
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
