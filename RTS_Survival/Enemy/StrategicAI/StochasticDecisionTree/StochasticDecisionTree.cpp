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
                                                     FStrategicAIBlackboard& Blackboard)
{
	// Filter top level actions to only get those of which we do have sub-actions to pick from that are not
	// blocked by requirements.
	TArray<FStrategicAIAction*> ValidActions =
		GetActionsWithValidSubActions(Blackboard, GameTimeSeconds);
	if (not EnsureHasAnyValidActions(ValidActions))
	{
		return;
	}
	FStrategicAIAction* PickedAction = StochasticHelpers::PickStrategicAction(
		ValidActions, bM_UseCachedGenerationSeed, M_CachedGenerationSeed, GameTimeSeconds);
	if (not EnsurePickedActionIsValid(PickedAction))
	{
		return;
	}
	TArray<UStrategicAISubAction*> SubActionsRequirementsMet = GetSubActionsThatHaveMetRequirements(
		*PickedAction, Blackboard, GameTimeSeconds);
	UStrategicAISubAction* PickedSubAction = StochasticHelpers::PickSubAction(SubActionsRequirementsMet,
		bM_UseCachedGenerationSeed, M_CachedGenerationSeed, GameTimeSeconds);
	if (not EnsurePickedSubActionIsValid(PickedSubAction))
	{
		return;
	}
	ExecuteSubAction(PickedSubAction, Blackboard, GameTimeSeconds);
}

bool UStochasticDecisionTree::EnsureHasAnyValidActions(const TArray<FStrategicAIAction*>& ValidActions) const
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


bool UStochasticDecisionTree::EnsurePickedActionIsValid(FStrategicAIAction* PickedAction) const
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

bool UStochasticDecisionTree::EnsurePickedSubActionIsValid(UStrategicAISubAction* PickedSubAction) const
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

void UStochasticDecisionTree::ExecuteSubAction(UStrategicAISubAction* SubAction,
                                               const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds)
{
	if (not EnsureIsValidDirectControlComponent() || not EnsureIsValidEnemyNavigationAIComponent()
		|| not EnsureIsValidEnemyFormation())
	{
		return;
	}
	SubAction->OnActionExecuted(GameTimeSeconds);
	switch (SubAction->SubtypeAction)
	{
	case ESubtypeAction::DEFAULT_OBJECT:
		RTSFunctionLibrary::ReportError(
			"Stochastic tree cannot execute sub-action as it is a default object, likely meaning it was not properly set up in the data assets.");
		break;
	case ESubtypeAction::AttackMoveToPlayerUnits:
		Exe_AttackMovePlayerUnits(SubAction, Blackboard);
		break;
	case ESubtypeAction::AttackMoveToPlayerHQ:
		Exe_AttackMovePlayerHQ(SubAction, Blackboard);
		break;
	case ESubtypeAction::AttackMoveToPlayerResourceBuildings:
		Exe_AttackMovePlayerResourceBuildings(SubAction, Blackboard);
		break;
	case ESubtypeAction::AttackMoveSpecificPoints:
		Exe_AttackMoveSpecificPoint(SubAction, Blackboard);
		break;
	case ESubtypeAction::DefendBase:
		break;
	case ESubtypeAction::DefendImportantMissionPoint:
		break;
	case ESubtypeAction::AttackMoveLightTanksToPlayerUnits:
		Exe_AttackMoveLightTanksToPlayerUnits(SubAction, Blackboard);
		break;
	case ESubtypeAction::HeavyTankPushPlayerBaseOrUnits:
		Exe_HeavyTankPushPlayerBaseOrUnits(SubAction, Blackboard);
		break;
	case ESubtypeAction::FlankPlayerHeavies:
		Exe_FlankPlayerHeavies(SubAction, Blackboard);
		break;
	}
}

void UStochasticDecisionTree::Exe_AttackMovePlayerUnits(
	UStrategicAISubAction* SubAction,
	const FStrategicAIBlackboard& Blackboard)
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
	CreateAttackMoveFormation(SubAction, AttackLocations, Blackboard);
}


void UStochasticDecisionTree::Exe_AttackMovePlayerHQ(
	UStrategicAISubAction* SubAction,
	const FStrategicAIBlackboard& Blackboard)
{
	TArray<FVector> AttackLocations = GetProjectedPlayerHQLocation(Blackboard);
	if (AttackLocations.IsEmpty())
	{
		if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
			EnemyAISettings::Debugging::StochasticDecisionTreeDebugging)
		{
			RTSFunctionLibrary::PrintString(
				"StochasticTree: could not attack move player HQ as no valid HQ location found / projected",
				FColor::Orange, EnemyAISettings::Debugging::StochasticActionsDebugTime);
		}
		return;
	}
	if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
		EnemyAISettings::Debugging::StochasticDecisionTreeDebugging)
	{
		DebugAttackLocations(AttackLocations, "Att Mv Player HQ");
	}
	CreateAttackMoveFormation(SubAction, AttackLocations, Blackboard);
}

void UStochasticDecisionTree::Exe_AttackMovePlayerResourceBuildings(
	UStrategicAISubAction* SubAction,
	const FStrategicAIBlackboard& Blackboard)
{
	TArray<FVector> AttackLocations = GetProjectedPlayerResourceBuildings(Blackboard);
	if (AttackLocations.IsEmpty())
	{
		if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
			EnemyAISettings::Debugging::StochasticDecisionTreeDebugging)
		{
			RTSFunctionLibrary::PrintString(
				"StochasticTree: could not attack move player resource buildings as no valid locations found / projected",
				FColor::Orange, EnemyAISettings::Debugging::StochasticActionsDebugTime);
		}
		return;
	}
	if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
		EnemyAISettings::Debugging::StochasticDecisionTreeDebugging)
	{
		DebugAttackLocations(AttackLocations, "Att Mv Player Resources");
	}
	CreateAttackMoveFormation(SubAction, AttackLocations, Blackboard);
}

void UStochasticDecisionTree::Exe_AttackMoveSpecificPoint(UStrategicAISubAction* SubAction,
                                                          const FStrategicAIBlackboard& Blackboard)
{
	USubAction_AttackSpecificPoints* AttackSpecificPoints =
		Cast<USubAction_AttackSpecificPoints>(SubAction);
	if (not IsValid(AttackSpecificPoints))
	{
		RTSFunctionLibrary::ReportError(
			"StochasticTree: cannot attack move specific points because sub-action is not "
			"USubAction_AttackSpecificPoints.");
		return;
	}

	const TArray<FVector>& TargetPoints = AttackSpecificPoints->GetTargetPoint();
	if (TargetPoints.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(
			"StochasticTree: cannot attack move specific points because no target points were configured.");
		return;
	}

	TArray<FVector> AttackLocations = GetProjectedAttackSpecificPoints(TargetPoints);
	if (AttackLocations.IsEmpty())
	{
		if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
			EnemyAISettings::Debugging::StochasticDecisionTreeDebugging)
		{
			RTSFunctionLibrary::PrintString(
				"StochasticTree: could not attack move specific points as no valid locations projected",
				FColor::Orange, EnemyAISettings::Debugging::StochasticActionsDebugTime);
		}
		return;
	}
	if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
		EnemyAISettings::Debugging::StochasticDecisionTreeDebugging)
	{
		DebugAttackLocations(AttackLocations, "Att Mv Specific Points");
	}
	CreateAttackMoveFormation(SubAction, AttackLocations, Blackboard);
}

void UStochasticDecisionTree::Exe_AttackMoveLightTanksToPlayerUnits(UStrategicAISubAction* SubAction,
                                                                    const FStrategicAIBlackboard& Blackboard)
{
	TArray<FVector> ValidAttackLocations;
	ValidAttackLocations.Append(GetProjectedPlayerBulkLocations(Blackboard));
	ValidAttackLocations.Append(GetProjectedPlayerAvgLocationAttackers(Blackboard));
	if (ValidAttackLocations.IsEmpty())
	{
		if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
			EnemyAISettings::Debugging::StochasticDecisionTreeDebugging)
		{
			RTSFunctionLibrary::PrintString(
				"StochasticTree: could not light tank attack move player units as no valid locations found / projected",
				FColor::Orange, EnemyAISettings::Debugging::StochasticActionsDebugTime);
		}
		return;
	}
	if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
		EnemyAISettings::Debugging::StochasticDecisionTreeDebugging)
	{
		DebugAttackLocations(ValidAttackLocations, "Light Tank Att Mv Units");
	}

	const FVector PickedAttackLocation = StochasticHelpers::PickRandomLocation(ValidAttackLocations);
	ValidAttackLocations.RemoveSingleSwap(PickedAttackLocation, EAllowShrinking::No);

	TArray<FVector> AttackLocations;
	AttackLocations.Reserve(3);
	AttackLocations.Add(PickedAttackLocation);
	AttackLocations.Append(StochasticHelpers::PickPairOfPointsClosestTo(
		PickedAttackLocation, ValidAttackLocations));

	if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
		EnemyAISettings::Debugging::StochasticDecisionTreeDebugging)
	{
		for (const auto& EachLocation : AttackLocations)
		{
			DebugPickedLocation(EachLocation, "Picked Light Tank Att Mv");
		}
	}
	CreateAttackMoveFormation(SubAction, AttackLocations, Blackboard);
}

void UStochasticDecisionTree::Exe_HeavyTankPushPlayerBaseOrUnits(UStrategicAISubAction* SubAction,
                                                                 const FStrategicAIBlackboard& Blackboard)
{
	TArray<FVector> ValidAttackLocations = GetProjectedPlayerBaseLocations(Blackboard);
	// If we have no player base location data then go attack units instead.
	if (ValidAttackLocations.IsEmpty())
	{
		ValidAttackLocations.Append(GetProjectedPlayerBulkLocations(Blackboard));
		ValidAttackLocations.Append(GetProjectedPlayerAvgLocationAttackers(Blackboard));
	}
	if (ValidAttackLocations.IsEmpty())
	{
		if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
			EnemyAISettings::Debugging::StochasticDecisionTreeDebugging)
		{
			RTSFunctionLibrary::PrintString(
				"StochasticTree: could not heavy tank push player base or units as no valid locations found / projected",
				FColor::Orange, EnemyAISettings::Debugging::StochasticActionsDebugTime);
		}
		return;
	}
	if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
		EnemyAISettings::Debugging::StochasticDecisionTreeDebugging)
	{
		DebugAttackLocations(ValidAttackLocations, "Heavy Tank Push");
	}

	constexpr int32 MaxPick = 3;
	const TArray<FVector> AttackLocations = StochasticHelpers::ExhaustivePick(ValidAttackLocations, MaxPick);
	CreateAttackMoveFormation(SubAction, AttackLocations, Blackboard);
}

void UStochasticDecisionTree::Exe_FlankPlayerHeavies(UStrategicAISubAction* SubAction,
                                                     const FStrategicAIBlackboard& Blackboard)
{
	TArray<FWeakActorLocations> FlankingPositions = GetProjectedAggregatedHeavyTankFlankingPositions(Blackboard);
	if (FlankingPositions.IsEmpty())
	{
		if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
			EnemyAISettings::Debugging::StochasticDecisionTreeDebugging)
		{
			RTSFunctionLibrary::PrintString(
				"StochasticTree: could not flank player heavies as no valid flank locations projected",
				FColor::Orange, EnemyAISettings::Debugging::StochasticActionsDebugTime);
		}
		return;
	}
	if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
		EnemyAISettings::Debugging::StochasticDecisionTreeDebugging)
	{
		DebugFlankPositions(FlankingPositions, "Flank Player Heavies");
	}
	CreateFlankingAttack(SubAction, FlankingPositions, Blackboard);
}

void UStochasticDecisionTree::Exe_DefendBase(UStrategicAISubAction* SubAction,
	const FStrategicAIBlackboard& Blackboard)
{
	// Prioritize defending base points that are under attack, if any.
	TArray<FVector> LocationsToDefend = GetProjectedLocationsUnderAttack(Blackboard);
	
	
}

void UStochasticDecisionTree::CreateAttackMoveFormation(
	UStrategicAISubAction* SubAction,
	TArray<FVector> AttackLocations,
	const FStrategicAIBlackboard& Blackboard)
{
	if (not IsValid(SubAction))
	{
		return;
	}

	const FIdleUnitSelectionPolicy SelectionPolicy = SubAction->BuildIdleUnitSelectionPolicy(Blackboard);
	// NOTE: if not enough units could be picked because one of the requirement rules cannot be satisfied then all of those
	// units that were picked prior to the failure are returned to the blackboard (picking simulation happens on copy data)
	FBlackboardIdleUnitsResult PickedUnits = M_DirectControlComponent->PickIdleBlackboardUnitsByPolicy(SelectionPolicy);
	if (not EnsureHasNonZeroPickedUnits(PickedUnits,
	                                    "Stochastic: cannot create attack move formation as zero units were picked"
	                                    "from the blackboard"))
	{
		return;
	}

	const FVector StartLocation = StochasticHelpers::GetAverageLocationPickedBlackboardUnits(PickedUnits);
	FVector AverageSpawnLocation = StartLocation;
	FVector StartLocation_Projected = StartLocation;
	bool bStartLocationIsProjected = false;
	if (StochasticHelpers::CanProjectNavigable_AveragePickedUnitLocation(M_EnemyNavigationAIComponent.Get(),
	                                                                     StartLocation, StartLocation_Projected))
	{
		// We managed to project to the navigation mesh so start at our average unit location point that is now on the
		// nav mesh.
		AverageSpawnLocation = StartLocation_Projected;
		bStartLocationIsProjected = true;
	}

	TArray<FVector> WayPoints = BuildAttackMoveWayPoints(
		AverageSpawnLocation, bStartLocationIsProjected, AttackLocations);
	if (WayPoints.IsEmpty())
	{
		return;
	}

	const FRotator FinalMoveRotation = UKismetMathLibrary::FindLookAtRotation(AverageSpawnLocation, WayPoints.Last());
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
		// DebugPickedUnitsAndWayPoints(PickedUnits, WayPoints);
	}
}


TArray<FVector> UStochasticDecisionTree::BuildAttackMoveWayPoints(
	const FVector& StartLocation,
	const bool bStartLocationIsProjected,
	TArray<FVector> AttackLocations) const
{
	TArray<FVector> WayPoints;
	if (AttackLocations.IsEmpty())
	{
		return WayPoints;
	}

	StochasticHelpers::SortArrayByDistanceToLocation(AttackLocations, StartLocation);
	if (PathFindAttackPath(StartLocation, AttackLocations[0], bStartLocationIsProjected, WayPoints))
	{
		// Remove first entry as it is already part of path.
		AttackLocations.RemoveAt(0, 1, EAllowShrinking::No);
		WayPoints.Append(AttackLocations);
		return WayPoints;
	}

	if (bStartLocationIsProjected)
	{
		WayPoints.Add(StartLocation);
	}

	WayPoints.Append(AttackLocations);
	return WayPoints;
}

bool UStochasticDecisionTree::PathFindAttackPath(
	const FVector& StartLocation,
	const FVector& TargetLocation,
	const bool bStartLocationIsProjected,
	TArray<FVector>& OutWayPoints) const
{
	const FStochasticPathFindingParams PathFindingParams{
		this,
		M_EnemyNavigationAIComponent.Get(),
		StartLocation,
		TargetLocation,
		M_AttackMovePathFindingSettings,
		bStartLocationIsProjected
	};

	TArray<FVector> PathPoints = StochasticHelpers::BuildNavigablePathPoints(PathFindingParams);
	if (PathPoints.IsEmpty())
	{
		return false;
	}

	OutWayPoints = MoveTemp(PathPoints);
	return true;
}

void UStochasticDecisionTree::CreateFlankingAttack(UStrategicAISubAction* SubAction,
                                                   const TArray<FWeakActorLocations>& FlankingPositions,
                                                   const FStrategicAIBlackboard& Blackboard)
{
	if (not IsValid(SubAction))
	{
		return;
	}

	const FIdleUnitSelectionPolicy SelectionPolicy = SubAction->BuildIdleUnitSelectionPolicy(Blackboard);
	// NOTE: if not enough units could be picked because one of the requirement rules cannot be satisfied then all of those
	// units that were picked prior to the failure are returned to the blackboard (picking simulation happens on copy data)
	FBlackboardIdleUnitsResult PickedUnits = M_DirectControlComponent->PickIdleBlackboardUnitsByPolicy(SelectionPolicy);
	if (not EnsureHasNonZeroPickedUnits(PickedUnits,
	                                    "Stochastic: cannot FLANK player heavies as zero units were picked"
	                                    "from the blackboard"))
	{
		return;
	}
	TArray<ICommands*> CommandUnits = GetCommandsArrayOfUnits(PickedUnits);
	const TArray<TPair<FVector, TWeakObjectPtr<AActor>>> TotalLocations = GetTotalAggregatedWeakActorLocations(
		FlankingPositions);
	const int32 MaxIndex = FMath::Min(CommandUnits.Num(), TotalLocations.Num()) - 1;
	// The first order given needs to overwrite whatever else was in the queue.
	bool bRestOrderQueue = true;
	for (int32 i = 0; i <= MaxIndex; ++i)
	{
		ICommands* CommandUnit = CommandUnits[i];
		const FVector TargetLocation = TotalLocations[i].Key;
		const bool bIsValidTarget = TotalLocations[i].Value.IsValid();
		if (bIsValidTarget)
		{
			const FRotator FinalRotation = UKismetMathLibrary::FindLookAtRotation(
				TargetLocation, TotalLocations[i].Value->GetActorLocation());
			(void)IssueMoveOrder(CommandUnit, TargetLocation, bRestOrderQueue, FinalRotation);
			bRestOrderQueue = false;
			// The to flank actor is valid so we move, attack and then add back to blackboard.
			(void)IssueAttackOrder(CommandUnit, TotalLocations[i].Value, bRestOrderQueue);
		}
		if (not IssueRegisterWithBlackboardOrder(CommandUnit, bRestOrderQueue))
		{
			RTSFunctionLibrary::ReportError("Failed to register unit with blackboard after attack-move orders"
				"for flanking attack");
		}
	}
	const int32 MaxIndexUnits = CommandUnits.Num() - 1;
	if(MaxIndex < MaxIndexUnits)
	{
		// There were more picked units that locations to engage from; provide these units back to the blackboard.
		for(int j = MaxIndex + 1; j <= MaxIndexUnits; ++j)
		{
			ICommands* CommandUnit = CommandUnits[j];
			if (not IssueRegisterWithBlackboardOrder(CommandUnit, bRestOrderQueue))
			{
				RTSFunctionLibrary::ReportError("Failed to register unit with blackboard after attack-move orders"
					"for flanking attack for excess units that had no flank location");
			}
		}
	}
}

TArray<TPair<FVector, TWeakObjectPtr<AActor>>> UStochasticDecisionTree::GetTotalAggregatedWeakActorLocations(
	const TArray<FWeakActorLocations>& WeakActorLocations) const
{
	TArray<TPair<FVector, TWeakObjectPtr<AActor>>> Locations;
	for (auto Each : WeakActorLocations)
	{
		for (auto EachLocation : Each.Locations)
		{
			TPair<FVector, TWeakObjectPtr<AActor>> Pair;
			Pair.Key = EachLocation;
			Pair.Value = Each.Actor;
			Locations.Add(Pair);
		}
	}
	return Locations;
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


TArray<ICommands*> UStochasticDecisionTree::GetCommandsArrayOfUnits(const FBlackboardIdleUnitsResult& PickedUnits) const
{
	TArray<ICommands*> CommandUnits;
	for (auto EachSquad : PickedUnits.SquadControllers)
	{
		CommandUnits.Add(EachSquad.Get());
	}
	for (auto EachTank : PickedUnits.TankMasters)
	{
		CommandUnits.Add(EachTank.Get());
	}
	return CommandUnits;
}

bool UStochasticDecisionTree::IssueMoveOrder(ICommands* UnitToCommand, const FVector& TargetLocation,
                                             const bool bResetOrderQueue,
                                             const FRotator& FinishedMovementRotation) const
{
	if (not UnitToCommand)
	{
		return false;
	}
	return UnitToCommand->MoveToLocation(TargetLocation, bResetOrderQueue, FinishedMovementRotation,
	                                     true) == ECommandQueueError::NoError;
}

bool UStochasticDecisionTree::IssueAttackOrder(ICommands* UnitToCommand, const TWeakObjectPtr<AActor> TargetActor,
                                               const bool bResetOrderQueue) const
{
	if (not UnitToCommand)
	{
		return false;
	}
	if (not TargetActor.IsValid())
	{
		return false;
	}
	return UnitToCommand->AttackActor(TargetActor.Get(), bResetOrderQueue) == ECommandQueueError::NoError;
}

bool UStochasticDecisionTree::IssueRegisterWithBlackboardOrder(ICommands* UnitToCommand,
                                                               const bool bResetOrderQueue) const
{
	if (not UnitToCommand)
	{
		return false;
	}
	return UnitToCommand->RegisterAsBlackboardIdle(bResetOrderQueue) == ECommandQueueError::NoError;
}

TArray<FStrategicAIAction*> UStochasticDecisionTree::GetActionsWithValidSubActions(
	const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds)
{
	TArray<FStrategicAIAction*> ValidActions;

	if constexpr (DeveloperSettings::Debugging::GEnemyController_StrategicAI_Compile_DebugSymbols &&
		EnemyAISettings::Debugging::StochasticDecisionTreeDebugging)
	{
		DebugAllActionsPriorReqCheck();
	}

	if (M_ActionDefinitions.IsEmpty())
	{
		return ValidActions;
	}

	for (FStrategicAIAction& Action : M_ActionDefinitions)
	{
		TArray<UStrategicAISubAction*> ValidSubActions =
			GetSubActionsThatHaveMetRequirements(Action, Blackboard, GameTimeSeconds);

		if (ValidSubActions.IsEmpty())
		{
			continue;
		}

		ValidActions.Add(&Action);
	}

	return ValidActions;
}


TArray<UStrategicAISubAction*> UStochasticDecisionTree::GetSubActionsThatHaveMetRequirements(
	FStrategicAIAction& Action,
	const FStrategicAIBlackboard& Blackboard, const float GameTimeSeconds) const
{
	TArray<UStrategicAISubAction*> ValidSubActions;

	TArray<TObjectPtr<UStrategicAISubAction>>& SubActions = Action.GetSubActions();
	if (SubActions.IsEmpty())
	{
		return ValidSubActions;
	}

	for (TObjectPtr<UStrategicAISubAction>& SubAction : SubActions)
	{
		if (not IsValid(SubAction))
		{
			continue;
		}

		if (not SubAction->GetAreRequirementsMet(Blackboard, GameTimeSeconds))
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

TArray<FVector> UStochasticDecisionTree::GetProjectedLocationsUnderAttack(
	const FStrategicAIBlackboard& Blackboard) const
{
	TArray<FVector> ValidLocations;
	for (const auto& EachLocatonUnderAttack : Blackboard.CurrentLocationsUnderPlayerAttack.LocationsUnderAttack)
	{
		FVector OutProjected = FVector::ZeroVector;
		if (StochasticHelpers::CanProjectNavigable_BulkLocation(M_EnemyNavigationAIComponent.Get(),
		                                                              EachLocatonUnderAttack.LocationUnderAttack,
		                                                              OutProjected))
		{
			ValidLocations.Add(OutProjected);
		}
	}
	return ValidLocations;
}

TArray<FVector> UStochasticDecisionTree::GetProjectedPlayerHQLocation(const FStrategicAIBlackboard& Blackboard) const
{
	TArray<FVector> ValidLocations;
	FVector OutProjected = FVector::ZeroVector;
	if (StochasticHelpers::CanProjectNavigable_PlayerHQLocation(M_EnemyNavigationAIComponent.Get(),
	                                                            Blackboard.CurrentPlayerUnitCounts.PlayerHQLocation,
	                                                            OutProjected))
	{
		ValidLocations.Add(OutProjected);
	}
	return ValidLocations;
}

TArray<FVector> UStochasticDecisionTree::GetProjectedPlayerResourceBuildings(
	const FStrategicAIBlackboard& Blackboard) const
{
	TArray<FVector> ValidLocations;
	for (const FVector& EachResourceBuildingLocation : Blackboard.CurrentPlayerUnitCounts.PlayerResourceBuildings)
	{
		FVector OutProjected = FVector::ZeroVector;
		if (StochasticHelpers::CanProjectNavigable_PlayerResourceBuildingLocation(M_EnemyNavigationAIComponent.Get(),
			EachResourceBuildingLocation,
			OutProjected))
		{
			ValidLocations.Add(OutProjected);
		}
	}
	return ValidLocations;
}

TArray<FVector> UStochasticDecisionTree::GetProjectedPlayerBaseLocations(
	const FStrategicAIBlackboard& Blackboard) const
{
	TArray<FVector> ValidLocations;
	ValidLocations.Append(GetProjectedPlayerHQLocation(Blackboard));
	ValidLocations.Append(GetProjectedPlayerResourceBuildings(Blackboard));
	return ValidLocations;
}

TArray<FVector> UStochasticDecisionTree::GetProjectedAttackSpecificPoints(const TArray<FVector>& TargetPoints) const
{
	TArray<FVector> ValidLocations;
	for (const FVector& EachTargetPoint : TargetPoints)
	{
		if (EachTargetPoint.IsNearlyZero())
		{
			continue;
		}
		FVector OutProjected = FVector::ZeroVector;
		if (StochasticHelpers::CanProjectNavigable_AttackSpecificPoint(M_EnemyNavigationAIComponent.Get(),
		                                                               EachTargetPoint, OutProjected))
		{
			ValidLocations.Add(OutProjected);
		}
	}
	return ValidLocations;
}

TArray<FWeakActorLocations> UStochasticDecisionTree::GetProjectedAggregatedHeavyTankFlankingPositions(
	const FStrategicAIBlackboard& Blackboard) const
{
	TArray<FWeakActorLocations> ValidFlankingPositions;
	for (const FResultClosestFlankableEnemyHeavy& EachFlankingResult : Blackboard.AgreggatedHeavyTankFlankingResults)
	{
		for (const FWeakActorLocations& EachFlankLocations : EachFlankingResult.FlankLocationsAroundHeavyTank)
		{
			FWeakActorLocations ProjectedFlankLocations = ProjectHeavyTankFlankLocations(EachFlankLocations);
			if (ProjectedFlankLocations.Locations.IsEmpty())
			{
				continue;
			}

			ValidFlankingPositions.Add(MoveTemp(ProjectedFlankLocations));
		}
	}
	return ValidFlankingPositions;
}

FWeakActorLocations UStochasticDecisionTree::ProjectHeavyTankFlankLocations(
	const FWeakActorLocations& FlankLocations) const
{
	FWeakActorLocations ProjectedFlankLocations;
	ProjectedFlankLocations.Actor = FlankLocations.Actor;
	for (const FVector& EachFlankLocation : FlankLocations.Locations)
	{
		FVector OutProjected = FVector::ZeroVector;
		if (StochasticHelpers::CanProjectNavigable_FlankLocation(M_EnemyNavigationAIComponent.Get(),
		                                                         EachFlankLocation, OutProjected))
		{
			ProjectedFlankLocations.Locations.Add(OutProjected);
		}
	}
	return ProjectedFlankLocations;
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

void UStochasticDecisionTree::DebugValidActions(const TArray<FStrategicAIAction*>& ValidActions) const
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

void UStochasticDecisionTree::DebugFlankPositions(const TArray<FWeakActorLocations>& FlankPositions,
                                                  const FString& DebugContext) const
{
	using namespace EnemyAISettings::Debugging;
	for (const FWeakActorLocations& EachFlankPositions : FlankPositions)
	{
		for (const FVector& EachLocation : EachFlankPositions.Locations)
		{
			DebugPoint(EachLocation, StochasticActionsAttackLocationDebugRadius, FlankLocationColor,
			           StochasticActionsDebugTime, DebugContext);
		}
	}
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
	for (auto EachSquad : Picked.SquadControllers)
	{
		if (not EachSquad)
		{
			continue;
		}
		DrawDebugLine(GetWorld(), EachSquad->GetActorLocation(), Waypoints[0], FColor::Green, false, 20.f, 0, 2.f);
	}
	for (auto EachTank : Picked.TankMasters)
	{
		if (not EachTank)
		{
			continue;
		}
		DrawDebugLine(GetWorld(), EachTank->GetActorLocation(), Waypoints[0], FColor::Green, false, 20.f, 0, 2.f);
	}
	// Now draw the path with thickness 8.
	for (int i = 0; i < Waypoints.Num() - 1; i++)
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
