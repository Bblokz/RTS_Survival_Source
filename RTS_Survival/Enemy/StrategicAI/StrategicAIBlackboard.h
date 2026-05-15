#pragma once

#include "CoreMinimal.h"
#include "BlackboardIdleUnitEntry.h"
#include "RTS_Survival/Enemy/EnemyAIBehaviour/EnemyAIBehaviour.h"
#include "RTS_Survival/Enemy/StrategicAI/Requests/StrategicAIRequests.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "StrategicAIBlackboard.generated.h"

class UTrainerComponent;
class ARoadSplineActor;

USTRUCT()
struct FStrategicAIBlackboard
{
	GENERATED_BODY()

	FStrategicAIBlackboard();
	UPROPERTY()
	TArray<FBlackboardIdleUnitEntry> IdleDirectControlUnits;

	UPROPERTY()
	TArray<FEnemyBasePointCoreBuildings> EnemyBasePoints;

	UPROPERTY()
	TArray<FDefensePositions> CurrentBaseDefensePositions;

	UPROPERTY()
	FResultPlayerUnitCounts CurrentPlayerUnitCounts;

	UPROPERTY()
	TArray<FResultClosestFlankableEnemyHeavy> AgreggatedHeavyTankFlankingResults;

	UPROPERTY()
	FResultLocationsUnderPlayerAttack CurrentLocationsUnderPlayerAttack;

	UPROPERTY()
	FResultPlayerUnitBulkLocations CurrentPlayerUnitBulkLocations;

	UPROPERTY()
	FResultConstructionLocations CurrentConstructionLocations;
	
	UPROPERTY()
	TArray<TWeakObjectPtr<ARoadSplineActor>> RoadSplineActors;

	UPROPERTY()
	FEnemyAIMissionSettings StrategicAIMissionSettings;

	// Automatically filled by those trainer components that are set to init as enemy trainers with InitTrainerAsEnemyTrainerComponent.
	// At init through EnemyController those trainers are added here.
	UPROPERTY()
	TArray<TWeakObjectPtr<UTrainerComponent>> TrainerComponents;
	
	// This is set by the mission manager after it was initialized there either with the game instance or with
	// the widget override.
	UPROPERTY()
	FRTSGameDifficulty GameDifficulty;
};
