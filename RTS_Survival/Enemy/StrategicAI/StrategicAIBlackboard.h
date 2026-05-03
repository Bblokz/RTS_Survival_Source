#pragma once

#include "CoreMinimal.h"
#include "StrategicAIBlackboard.generated.h"

USTRUCT()
struct FStrategicAIBlackboard
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FVector> EnemyBasePoints;
};
