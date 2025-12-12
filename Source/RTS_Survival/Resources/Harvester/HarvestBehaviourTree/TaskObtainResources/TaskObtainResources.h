// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "TaskObtainResources.generated.h"

/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UTaskObtainResources : public UBTTaskNode
{
	GENERATED_BODY()

protected:
	/** The blackboard key to use for obtaining the harvester component. */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	struct FBlackboardKeySelector HarvesterKeySelector;

	/** The blackboard key that determines whether we need to return cargo. */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	struct FBlackboardKeySelector ReturnCargoKeySelector;

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
