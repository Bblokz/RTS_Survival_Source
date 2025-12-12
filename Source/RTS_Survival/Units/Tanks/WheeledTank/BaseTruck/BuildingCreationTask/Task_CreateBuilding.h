// Copyright (C) 2020-2023 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "Task_CreateBuilding.generated.h"

/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UTask_CreateBuilding : public UBTTask_MoveTo
{
	GENERATED_BODY()

	UTask_CreateBuilding(const FObjectInitializer& ObjectInitializer);

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;
};
