// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "TaskReturnCargoToDropOff.generated.h"

/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UTaskReturnCargoToDropOff : public UBTTaskNode
{
	GENERATED_BODY()

	UTaskReturnCargoToDropOff();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;
};
