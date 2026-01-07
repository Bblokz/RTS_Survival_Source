// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "TaskObtainResources.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Resources/Harvester/Harvester.h"
#include "RTS_Survival/Resources/ResourceComponent/ResourceComponent.h"

EBTNodeResult::Type UTaskObtainResources::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// Get the harvester from the blackboard
	UHarvester* Harvester = Cast<UHarvester>(
		OwnerComp.GetBlackboardComponent()->GetValueAsObject(HarvesterKeySelector.SelectedKeyName));
	if (IsValid(Harvester))
	{
		// // if (Harvester->HarvestAIAction_HarvestTargetResource())
		// {
		// 	// Cargo is full after adding the resources!
		// 	OwnerComp.GetBlackboardComponent()->SetValueAsBool(ReturnCargoKeySelector.SelectedKeyName, true);
		// 	if constexpr (DeveloperSettings::Debugging::GHarvestResources_Compile_DebugSymbols)
		// 	{
		// 		RTSFunctionLibrary::PrintString("Cargo is full after harvest; return cargo!", FColor::Green);
		// 	}
		// }
		// else
		// {
		// 	// Cargo not full.
		// 	OwnerComp.GetBlackboardComponent()->SetValueAsBool(ReturnCargoKeySelector.SelectedKeyName, false);
		// 	if constexpr (DeveloperSettings::Debugging::GHarvestResources_Compile_DebugSymbols)
		// 	{
		// 		RTSFunctionLibrary::PrintString("Cargo not full after harvest; continue harvesting!", FColor::Green);
		// 	}
		// }
		// return EBTNodeResult::Succeeded;
	}
	OwnerComp.GetBlackboardComponent()->SetValueAsBool(ReturnCargoKeySelector.SelectedKeyName, true);
	return EBTNodeResult::Failed;
}
