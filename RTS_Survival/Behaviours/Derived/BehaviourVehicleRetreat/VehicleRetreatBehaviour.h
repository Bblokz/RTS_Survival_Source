// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Behaviours/Behaviour.h"

#include "VehicleRetreatBehaviour.generated.h"

/**
 * @brief Shows repeated "Retreating" text above an enemy vehicle while direct control reverses it to a base.
 * Added and removed by UEnemyDirectControlComponent; it changes no stats.
 */
UCLASS()
class RTS_SURVIVAL_API UVehicleRetreatBehaviour : public UBehaviour
{
	GENERATED_BODY()

public:
	UVehicleRetreatBehaviour();
};
