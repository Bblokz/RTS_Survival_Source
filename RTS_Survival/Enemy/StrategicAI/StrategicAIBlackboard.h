#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Enemy/EnemyAIBehaviour/EnemyAIBehaviour.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "StrategicAIBlackboard.generated.h"

class ARoadSplineActor;

USTRUCT()
struct FStrategicAIBlackboard
{
	GENERATED_BODY()

	FStrategicAIBlackboard();

	UPROPERTY()
	TArray<FVector> EnemyBasePoints;

	UPROPERTY()
	FResultPlayerUnitCounts CurrentPlayerUnitCounts;
	
	UPROPERTY()
	TArray<TWeakObjectPtr<ARoadSplineActor>> RoadSplineActors;

	UPROPERTY()
	FEnemyAIMissionSettings StrategicAIMissionSettings;

	// This is set by the mission manager after it was initialized there either with the game instance or with
	// the widget override.
	UPROPERTY()
	FRTSGameDifficulty GameDifficulty;
};
