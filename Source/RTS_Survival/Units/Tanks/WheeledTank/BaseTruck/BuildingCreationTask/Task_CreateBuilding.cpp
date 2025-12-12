// Copyright Bas Blokzijl - All rights reserved.

#include "Task_CreateBuilding.h"

#include "AIController.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"

UTask_CreateBuilding::UTask_CreateBuilding(const FObjectInitializer &ObjectInitializer)
{
	NodeName = "Create Building";
}

EBTNodeResult::Type UTask_CreateBuilding::ExecuteTask(UBehaviorTreeComponent &OwnerComp, uint8 *NodeMemory)
{
	if (AAIController *AIController = OwnerComp.GetAIOwner())
	{
		if (AAINomadicVehicle *NomadicController = Cast<AAINomadicVehicle>(AIController))
		{
			this->AcceptableRadius = NomadicController->GetConstructionAcceptanceRad();
		}
	}
	return Super::ExecuteTask(OwnerComp, NodeMemory);
}

void UTask_CreateBuilding::OnTaskFinished(UBehaviorTreeComponent &OwnerComp, uint8 *NodeMemory,
										  EBTNodeResult::Type TaskResult)
{
	if (TaskResult == EBTNodeResult::Succeeded)
	{
		ANomadicVehicle *movedPawn = Cast<ANomadicVehicle>(OwnerComp.GetAIOwner()->GetPawn());
		if (IsValid(movedPawn))
		{
			movedPawn->StartBuildingConstruction();
		}
	}
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}
