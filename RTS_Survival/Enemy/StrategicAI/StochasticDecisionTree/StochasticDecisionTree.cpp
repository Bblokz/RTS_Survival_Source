#include "StochasticDecisionTree.h"

#include "Kismet/KismetMathLibrary.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Enemy/EnemyAISettings/EnemyAISettings.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyDirectControlComponent/EnemyDirectControlComponent.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyFormationController/EnemyFormationController.h"
#include "RTS_Survival/Enemy/StrategicAI/Component/EnemyStrategicAIComponent.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
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
	M_AttackMoveWaveSettings.HelpOffsetRadiusMltMax = 2.f;
	M_AttackMoveWaveSettings.HelpOffsetRadiusMltMin = 1.f;
	M_AttackMoveWaveSettings.MaxAttackTimeBeforeAdvancingToNextWayPoint = 120.f;
	M_AttackMoveWaveSettings.MaxTriesFindNavPointForHelpOffset = 5;
	M_AttackMoveWaveSettings.ProjectionScale = 1.f;
}

void UStochasticDecisionTree::InitStochasticDecisionTree(
	UEnemyStrategicAIComponent* StrategicComponentWithBB,
	UEnemyDirectControlComponent* DirectControlComponent,
	UEnemyNavigationAIComponent* EnemyNavigationAI,
	UEnemyFormationController* EnemyFormationController, AEnemyController* EnemyController)
{
	M_StrategicComponent = StrategicComponentWithBB;
	M_DirectControlComponent = DirectControlComponent;
	M_EnemyNavigationAIComponent = EnemyNavigationAI;
	M_EnemyFormationController = EnemyFormationController;
	M_EnemyController = EnemyController;
	(void)EnsureStrategicComponentIsValid();
	(void)EnsureIsValidDirectControlComponent();
	(void)EnsureIsValidEnemyNavigationAIComponent();
	(void)EnsureIsValidEnemyFormation();
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
	if (not EnsurePickedSubActionIsValid(PickedSubAction))
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
                                               const FStrategicAIBlackboard& Blackboard)
{
	if (not EnsureIsValidEnemyNavigationAIComponent() || not EnsureIsValidEnemyFormation())
	{
		return;
	}
	switch (SubAction->SubtypeAction)
	{
	case ESubtypeAction::DEFAULT_OBJECT:
		RTSFunctionLibrary::ReportError(
			"Stochastic tree cannot execute sub-action as it is a default object, likely meaning it was not properly set up in the data assets.");
		break;
	case ESubtypeAction::AttackMoveToPlayerUnits:
		Exe_AttackMovePlayerUnits(Blackboard);
		break;
	case ESubtypeAction::AttackMoveToPlayerHQ:
		break;
	case ESubtypeAction::AttackMoveToPlayerResourceBuildings:
		break;
	case ESubtypeAction::AttackMoveSpecificPoints:
		break;
	case ESubtypeAction::DefendBase:
		break;
	case ESubtypeAction::DefendImportantMissionPoint:
		break;
	case ESubtypeAction::AttackMoveLightTanksToPlayerUnits:
		break;
	case ESubtypeAction::HeavyTankPushPlayerBaseOrUnits:
		break;
	case ESubtypeAction::FlankPlayerHeavies:
		break;
	}
}

void UStochasticDecisionTree::Exe_AttackMovePlayerUnits(const FStrategicAIBlackboard& Blackboard)
{
	TArray<FVector> ValidAttackLocations;
	ValidAttackLocations.Append(GetProjectedPlayerBulkLocations(Blackboard));
	ValidAttackLocations.Append(GetProjectedPlayerAvgLocationAttackers(Blackboard));
	// Note that this is not an error as projections may miss depending on player unit placement.
	if (ValidAttackLocations.IsEmpty())
	{
		if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
			EnemyAISettings::Debugging::StochasticDecisionTreeDebugging)
		{
			RTSFunctionLibrary::PrintString(
				"StochasticTree: could not attack move player units as no valid locations found / projected",
				FColor::Orange, EnemyAISettings::Debugging::StochasticActionsDebugTime);
		}
		return;
	}
	if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
		EnemyAISettings::Debugging::StochasticDecisionTreeDebugging)
	{
		DebugAttackLocations(ValidAttackLocations, "Att Mv Player Units");
	}
	TArray<FVector> AttackLocations = StochasticHelpers::PickRandomClosestPair(ValidAttackLocations);
	if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
		EnemyAISettings::Debugging::StochasticDecisionTreeDebugging)
	{
		for (const auto& EachLocation : AttackLocations)
		{
			DebugPickedLocation(EachLocation, "Picked Att Mv Player Units");
		}
	}
	CreateAttackMoveFormation(AttackLocations, Blackboard);
}


void UStochasticDecisionTree::Exe_AttackMovePlayerHQ(const FStrategicAIBlackboard& Blackboard)
{
}

void UStochasticDecisionTree::Exe_AttackMovePlayerResourceBuildings(const FStrategicAIBlackboard& Blackboard)
{
}

void UStochasticDecisionTree::Exe_AttackMoveSpecificPoint(const UStrategicAISubAction* SubAction,
                                                          const FStrategicAIBlackboard& Blackboard)
{
}

void UStochasticDecisionTree::CreateAttackMoveFormation(TArray<FVector> AttackLocations,
                                                        const FStrategicAIBlackboard& Blackboard)
{
	FBlackboardIdleUnitsResult PickedUnits = M_DirectControlComponent->PickRandomMaxIdleBlackboardUnits(5);
	if (not EnsureHasNonZeroPickedUnits(PickedUnits,
	                                    "Stochastic: cannot create attack move formation as zero units were picked"
	                                    "from the blackboard"))
	{
		return;
	}
	TArray<FVector> WayPoints;
	FRotator FinalMoveRotation;
	const FVector StartLocation = StochasticHelpers::GetAverageLocationPickedBlackboardUnits(PickedUnits);
	FVector AverageSpawnLocation;
	FVector StartLocation_Projected = StartLocation;
	if (StochasticHelpers::CanProjectNavigable_AveragePickedUnitLocation(M_EnemyNavigationAIComponent.Get(),
	                                                                     StartLocation, StartLocation_Projected))
	{
		// We managed to project to the navigation mesh so start at our average unit location point.
		StochasticHelpers::SortArrayByDistanceToLocation(AttackLocations, StartLocation_Projected);
		WayPoints.Add(StartLocation_Projected);
		WayPoints.Append(AttackLocations);
		FinalMoveRotation = UKismetMathLibrary::FindLookAtRotation(StartLocation_Projected, AttackLocations.Last());
		AverageSpawnLocation = StartLocation_Projected;
	}
	else
	{
		// Projection failed; still sort by distance to average location but do not start with going there
		// as it may be unreachable.
		StochasticHelpers::SortArrayByDistanceToLocation(AttackLocations, StartLocation);
		WayPoints.Append(AttackLocations);
		FinalMoveRotation = UKismetMathLibrary::FindLookAtRotation(StartLocation, AttackLocations.Last());
		AverageSpawnLocation = StartLocation;
	}
	M_EnemyFormationController->MoveAttackMoveFormationToLocation(
		PickedUnits.SquadControllers,
		PickedUnits.TankMasters,
		WayPoints,
		FinalMoveRotation,
		GetMaxFormationWidthForPickedUnits(PickedUnits),
		M_AttackMoveWave_FormationOffsetMultiplier,
		M_AttackMoveWaveSettings,
		AverageSpawnLocation
	);
	if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
		EnemyAISettings::Debugging::StochasticDecisionTreeDebugging)
	{
		DebugPickedUnitsAndWayPoints(PickedUnits, WayPoints);
	}
}

bool UStochasticDecisionTree::EnsureHasNonZeroPickedUnits(const FBlackboardIdleUnitsResult& PickedUnits,
                                                          const FString& DebugContext)
{
	if (PickedUnits.SquadControllers.IsEmpty() && PickedUnits.TankMasters.IsEmpty())
	{
		if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
			EnemyAISettings::Debugging::StochasticDecisionTreeDebugging)
		{
			RTSFunctionLibrary::PrintString(
				DebugContext,
				FColor::Orange, EnemyAISettings::Debugging::StochasticActionsDebugTime);
		}
		return false;
	}
	return true;
}

int32 UStochasticDecisionTree::GetMaxFormationWidthForPickedUnits(const FBlackboardIdleUnitsResult& PickedUnits) const
{
	const int32 Amount = PickedUnits.SquadControllers.Num() + PickedUnits.TankMasters.Num();
	if (Amount <= 4)
	{
		return 2;
	}
	if (Amount <= 8)
	{
		return 3;
	}
	if (Amount > 14)
	{
		return 5;
	}
	return 4;
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

bool UStochasticDecisionTree::EnsureIsValidEnemyNavigationAIComponent() const
{
	if (not M_EnemyNavigationAIComponent.IsValid())
	{
		RTSFunctionLibrary::ReportError(TEXT(
			"Stochastic decision tree failed to ensure valid enemy navigation AI component because the enemy navigation AI component is null."));
		return false;
	}
	return true;
}

bool UStochasticDecisionTree::EnsureIsValidEnemyFormation() const
{
	if (not M_EnemyFormationController.IsValid())
	{
		RTSFunctionLibrary::ReportError(TEXT(
			"Stochastic decision tree failed to ensure valid enemy formation controller because the enemy formation controller is null."));
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

TArray<FVector> UStochasticDecisionTree::GetProjectedPlayerBulkLocations(const FStrategicAIBlackboard& Blackboard)
{
	TArray<FVector> ValidLocations;
	// Note that arrays of vectors are always checks for is nearly zero on the async processor!
	for (const auto& EachBulk : Blackboard.CurrentPlayerUnitBulkLocations.PlayerUnitBulks)
	{
		FVector OutProjected = FVector::ZeroVector;
		if (StochasticHelpers::CanProjectNavigable_BulkLocation(M_EnemyNavigationAIComponent.Get(),
		                                                        EachBulk.BulkLocation, OutProjected))
		{
			ValidLocations.Add(OutProjected);
		}
	}
	return ValidLocations;
}

TArray<FVector> UStochasticDecisionTree::GetProjectedPlayerAvgLocationAttackers(
	const FStrategicAIBlackboard& Blackboard) const
{
	TArray<FVector> ValidLocations;
	// Note that arrays of vectors are always checks for is nearly zero on the async processor!
	for (const auto& EachLocatonAttackerGroup : Blackboard.CurrentLocationsUnderPlayerAttack.LocationsUnderAttack)
	{
		FVector OutProjected = FVector::ZeroVector;
		if (StochasticHelpers::CanProjectNavigable_BulkLocation(M_EnemyNavigationAIComponent.Get(),
		                                                        EachLocatonAttackerGroup.AverageAttackerLocation,
		                                                        OutProjected))
		{
			ValidLocations.Add(OutProjected);
		}
	}
	return ValidLocations;
}

void UStochasticDecisionTree::DebugAllActionsPriorReqCheck() const
{
	FString DebugString = "Actions Prior Req Check:\n";
	for (auto EachMainAction : M_ActionDefinitions)
	{
		DebugString += EachMainAction.GetDebugString() + "\n";
	}
	RTSFunctionLibrary::PrintString(DebugString, FColor::Purple,
	                                EnemyAISettings::Debugging::StochasticActionsDebugTime);
}

void UStochasticDecisionTree::DebugValidActions(const TArray<const FStrategicAIAction*> ValidActions) const
{
	FString DebugString = "\nValid Actions:";
	for (auto EachValidAction : ValidActions)
	{
		DebugString += "\n-- " + EachValidAction->GetDebugString();
	}
	RTSFunctionLibrary::PrintString(DebugString, FColor::Green,
	                                EnemyAISettings::Debugging::StochasticActionsDebugTime);
}

void UStochasticDecisionTree::DebugAttackLocations(const TArray<FVector>& Locations, const FString& DebugContext) const
{
	using namespace EnemyAISettings::Debugging;
	for (const FVector& EachLocation : Locations)
	{
		DebugPoint(EachLocation, StochasticActionsAttackLocationDebugRadius, AttackLocationColor,
		           StochasticActionsDebugTime, DebugContext);
	}
}

void UStochasticDecisionTree::DebugPickedLocation(const FVector& PickedLocation, const FString& DebugContext) const
{
	using namespace EnemyAISettings::Debugging;
	DebugPoint(PickedLocation, PickedActionLocationRadius, PickedActionLocationColor, StochasticActionsDebugTime,
	           DebugContext);
}

void UStochasticDecisionTree::DebugPoint(const FVector& Point, const float Radius, const FColor& Color,
                                         const float Duration, const FString& Text) const
{
	DrawDebugSphere(GetWorld(), Point, Radius, 8,
	                Color, false, Duration, 0, 2.f);
	DrawDebugString(
		GetWorld(), Point + FVector(0, 0, Radius),
		Text, nullptr, Color, Duration,
		false);
}

void UStochasticDecisionTree::DebugPickedUnitsAndWayPoints(const FBlackboardIdleUnitsResult& Picked,
	TArray<FVector> Waypoints)
{
	for(auto EachSquad : Picked.SquadControllers)
	{
		if(not EachSquad)
		{
			continue;
		}
		DrawDebugLine(GetWorld(), EachSquad->GetActorLocation(), Waypoints[0], FColor::Green, false, 20.f, 0, 2.f);
	}
	for(auto EachTank : Picked.TankMasters)
	{
		if(not EachTank)
		{
			continue;
		}
		DrawDebugLine(GetWorld(), EachTank->GetActorLocation(), Waypoints[0], FColor::Green, false, 20.f, 0, 2.f);
	}
	// Now draw the path with thickness 8.
	for(int i = 0; i < Waypoints.Num() - 1; i++)
	{
		DrawDebugLine(GetWorld(), Waypoints[i], Waypoints[i + 1], FColor::Blue, false, 20.f, 0, 8.f);
	}
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
