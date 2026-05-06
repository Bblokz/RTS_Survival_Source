#pragma once

#include "CoreMinimal.h"
#include "BlackboardIdleUnitEntry.h"
#include "RTS_Survival/Enemy/EnemyAIBehaviour/EnemyAIBehaviour.h"
#include "RTS_Survival/Enemy/StrategicAI/Requests/StrategicAIRequests.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "StrategicAIBlackboard.generated.h"

class ARoadSplineActor;

USTRUCT()
struct FStrategicAIBlackboard
{
	GENERATED_BODY()

	FStrategicAIBlackboard();
	UPROPERTY()
	TArray<FBlackboardIdleUnitEntry> IdleDirectControlUnits;

	UPROPERTY()
	TArray<FVector> EnemyBasePoints;

	UPROPERTY()
	FResultPlayerUnitCounts CurrentPlayerUnitCounts;

	UPROPERTY()
	TArray<FResultClosestFlankableEnemyHeavy> AgreggatedHeavyTankFlankingResults;

	UPROPERTY()
	FResultLocationsUnderPlayerAttack CurrentLocationsUnderPlayerAttack;

	UPROPERTY()
	FResultPlayerUnitBulkLocations CurrentPlayerUnitBulkLocations;
	
	UPROPERTY()
	TArray<TWeakObjectPtr<ARoadSplineActor>> RoadSplineActors;

	UPROPERTY()
	FEnemyAIMissionSettings StrategicAIMissionSettings;

	// This is set by the mission manager after it was initialized there either with the game instance or with
	// the widget override.
	UPROPERTY()
	FRTSGameDifficulty GameDifficulty;
};
