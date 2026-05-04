// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EnemyControllerBlueprintFunctionLibrary.generated.h"

/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UEnemyControllerBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void AddUnitToDirectControl(AActor* UnitToAdd) const;

};
