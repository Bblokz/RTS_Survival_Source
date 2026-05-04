#pragma once

#include "CoreMinimal.h"
#include "StrategicAIBlackboard.generated.h"

class ARoadSplineActor;

USTRUCT()
struct FStrategicAIBlackboard
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FVector> EnemyBasePoints;

	UPROPERTY()
	TArray<TWeakObjectPtr<ARoadSplineActor>> RoadSplineActors;
};
