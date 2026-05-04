// Copyright (C) Bas Blokzijl - All rights reserved.


#include "EnemyControllerBlueprintFunctionLibrary.h"

#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

void UEnemyControllerBlueprintFunctionLibrary::AddUnitToDirectControl(AActor* UnitToAdd) const
{
	if(not IsValid(UnitToAdd))
	{
		return;
	}
	auto EnemyController = FRTS_Statics::GetEnemyController(UnitToAdd);
	if(not IsValid(EnemyController))
	{
		return;	
	}
	UEnemyDirectControlComponent* DirectControl = EnemyController->GetEnemyDirectControlComponent();
	if(not IsValid(DirectControl))
	{
		return;
	}
	DirectControl->RegisterDirectControlUnit(UnitToAdd);
}
