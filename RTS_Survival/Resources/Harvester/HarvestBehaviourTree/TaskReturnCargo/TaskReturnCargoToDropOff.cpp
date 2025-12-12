// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "TaskReturnCargoToDropOff.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"

UTaskReturnCargoToDropOff::UTaskReturnCargoToDropOff()
{
	NodeName = TEXT("Return Cargo To Drop Off");
}

EBTNodeResult::Type UTaskReturnCargoToDropOff::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	RTSFunctionLibrary::PrintString("MOVE TO CARGO!", FColor::Blue);
	return Super::ExecuteTask(OwnerComp, NodeMemory);
}

void UTaskReturnCargoToDropOff::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory,
	EBTNodeResult::Type TaskResult)
{
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}
