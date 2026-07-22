// Copyright 2020 313 Studios. All Rights Reserved.

#include "Task_VehicleMoveTo.h"

#include "AIController.h"
#include "TimerManager.h"

#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/PathFollowingComponent/TrackPathFollowingComponent.h"

// To access finished movement command.

UTask_VehicleMoveTo::UTask_VehicleMoveTo(const FObjectInitializer &ObjectInitializer) : UBTTask_MoveTo(ObjectInitializer)
{
	NodeName = "Vehicle Move To";
	bCreateNodeInstance = true;
}

EBTNodeResult::Type UTask_VehicleMoveTo::ExecuteTask(UBehaviorTreeComponent &OwnerComp, uint8 *NodeMemory)
{
	AAIController *AIController = OwnerComp.GetAIOwner();

	if (AIController)
	{
		APawn* const ControlledPawn = AIController->GetPawn();
		ICommands* const MovedPawn = Cast<ICommands>(ControlledPawn);
		UCommandData* const CommandData = MovedPawn ? MovedPawn->GetIsValidCommandData() : nullptr;
		if (IsValid(CommandData))
		{
			M_CommandExecutionSerial = CommandData->GetCurrentCommandExecutionSerial();
			M_MovementAbility = bReverseTowardsTarget ? EAbilityID::IdReverseMove : EAbilityID::IdMove;
		}
		// const float acceptableRadius = VehiclePathComp->GetGoalAcceptanceRadius();
		// this->AcceptableRadius = acceptableRadius;
		// this->ObservedBlackboardValueTolerance = acceptableRadius * 0.95f;
	}

	UTrackPathFollowingComponent* const VehiclePathFollowingComponent =
		GetVehiclePathFollowingComponent(OwnerComp);
	if (bReverseTowardsTarget && IsValid(VehiclePathFollowingComponent))
	{
		// If the user wants to manually reverse towards the target, set that here
		VehiclePathFollowingComponent->SetReverse(true);
	}

	return Super::ExecuteTask(OwnerComp, NodeMemory);
}

EBTNodeResult::Type UTask_VehicleMoveTo::AbortTask(UBehaviorTreeComponent &OwnerComp, uint8 *NodeMemory)
{
	UTrackPathFollowingComponent* const VehiclePathFollowingComponent =
		GetVehiclePathFollowingComponent(OwnerComp);
	if (bReverseTowardsTarget && IsValid(VehiclePathFollowingComponent))
	{
		VehiclePathFollowingComponent->SetReverse(false);
	}
	return Super::AbortTask(OwnerComp, NodeMemory);
}

void UTask_VehicleMoveTo::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	if (M_CommandExecutionSerial != 0 && M_MovementAbility != EAbilityID::IdNoAbility)
	{
		AAIController* const AIController = OwnerComp.GetAIOwner();
		APawn* const ControlledPawn = IsValid(AIController) ? AIController->GetPawn() : nullptr;
		if (IsValid(ControlledPawn))
		{
			TWeakObjectPtr<APawn> WeakControlledPawn = ControlledPawn;
			const EAbilityID FinishedAbility = M_MovementAbility;
			const uint64 CommandExecutionSerial = M_CommandExecutionSerial;
			FTimerDelegate DeferredDoneDelegate;
			DeferredDoneDelegate.BindLambda([WeakControlledPawn, FinishedAbility, CommandExecutionSerial]()
			{
				if (not WeakControlledPawn.IsValid())
				{
					return;
				}
				ICommands* MovedPawn = Cast<ICommands>(WeakControlledPawn.Get());
				if (not MovedPawn)
				{
					return;
				}
				MovedPawn->TryDoneExecutingCommand(FinishedAbility, CommandExecutionSerial);
			});
			if (UWorld* World = ControlledPawn->GetWorld())
			{
				World->GetTimerManager().SetTimerForNextTick(DeferredDoneDelegate);
			}
		}
	}
	if (UTrackPathFollowingComponent* VehiclePathFollowingComponent =
		GetVehiclePathFollowingComponent(OwnerComp))
	{
		VehiclePathFollowingComponent->SetReverse(false);
	}
	M_CommandExecutionSerial = 0;
	M_MovementAbility = EAbilityID::IdNoAbility;
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}

UTrackPathFollowingComponent* UTask_VehicleMoveTo::GetVehiclePathFollowingComponent(
	const UBehaviorTreeComponent& OwnerComp) const
{
	const AAIController* const AIController = OwnerComp.GetAIOwner();
	return IsValid(AIController)
		       ? Cast<UTrackPathFollowingComponent>(AIController->GetPathFollowingComponent())
		       : nullptr;
}
