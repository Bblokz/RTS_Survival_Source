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
	// The controller that plays the unit's own player takes it under direct control.
	AEnemyController* EnemyController = FRTS_Statics::GetEnemyControllerOfUnit(UnitToAdd);
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
