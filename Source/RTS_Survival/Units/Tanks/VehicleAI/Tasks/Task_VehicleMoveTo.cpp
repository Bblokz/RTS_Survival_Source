// Copyright 2020 313 Studios. All Rights Reserved.

#include "Task_VehicleMoveTo.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"

#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/PathFollowingComponent/TrackPathFollowingComponent.h"

// To access finished movement command.

UTask_VehicleMoveTo::UTask_VehicleMoveTo(const FObjectInitializer &ObjectInitializer) : UBTTask_MoveTo(ObjectInitializer)
{
	NodeName = "Vehicle Move To";
}

EBTNodeResult::Type UTask_VehicleMoveTo::ExecuteTask(UBehaviorTreeComponent &OwnerComp, uint8 *NodeMemory)
{
	AAIController *AIController = OwnerComp.GetAIOwner();

	if (AIController)
	{
		MyBlackboard = OwnerComp.GetBlackboardComponent();
		VehiclePathComp = Cast<UTrackPathFollowingComponent>(AIController->GetPathFollowingComponent());
		// const float acceptableRadius = VehiclePathComp->GetGoalAcceptanceRadius();
		// this->AcceptableRadius = acceptableRadius;
		// this->ObservedBlackboardValueTolerance = acceptableRadius * 0.95f;
	}

	if (bReverseTowardsTarget && VehiclePathComp)
	{
		// If the user wants to manually reverse towards the target, set that here
		VehiclePathComp->SetReverse(true);
	}

	return Super::ExecuteTask(OwnerComp, NodeMemory);
}

EBTNodeResult::Type UTask_VehicleMoveTo::AbortTask(UBehaviorTreeComponent &OwnerComp, uint8 *NodeMemory)
{
	if (bReverseTowardsTarget && VehiclePathComp)
	{
		VehiclePathComp->SetReverse(false);
	}
	return Super::AbortTask(OwnerComp, NodeMemory);
}

void UTask_VehicleMoveTo::OnTaskFinished(UBehaviorTreeComponent &OwnerComp, uint8 *NodeMemory, EBTNodeResult::Type TaskResult)
{
	const bool bwasReverse = bReverseTowardsTarget && VehiclePathComp;
	// Allow task to finish if it succeeded or if it failed but we are in the goal acceptance radius.
	// (TaskResult == EBTNodeResult::Failed && (VehiclePathComp && VehiclePathComp->GetIsInGoalAcceptanceRadius())))
	if (TaskResult == EBTNodeResult::Succeeded)
	{
		ICommands *movedPawn = Cast<ICommands>(OwnerComp.GetAIOwner()->GetPawn());
		if (movedPawn)
		{
			if (bwasReverse)
			{
				movedPawn->DoneExecutingCommand(EAbilityID::IdReverseMove);
			}
			else
			{
				movedPawn->DoneExecutingCommand(EAbilityID::IdMove);
			}
		}
	}
	if (IsValid(VehiclePathComp))
	{
		VehiclePathComp->SetReverse(false);
	}
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}
