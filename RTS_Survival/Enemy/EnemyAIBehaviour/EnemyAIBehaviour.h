#pragma once

#include "CoreMinimal.h"


#include "EnemyAIBehaviour.generated.h"

UENUM(Blueprintable)
enum class EEnemyStrategicAIBehaviour :uint8
{
	NotInitialized,
	PreferDefensive,
	PreferAggressive,
};


USTRUCT(Blueprintable)
struct FEnemyAIMissionSettings
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EEnemyStrategicAIBehaviour StrategicAIBehaviour = EEnemyStrategicAIBehaviour::NotInitialized;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FVector> PointsReactDefensivelyTo;

	// This is used to evaluate whether the AI can use its decision tree to manually control units that are 'idle' on the blackboard
	// which makes them eligible for strategic AI commands that take direct control of units.
	// This can be turned on and off during runtime using the EnemyAIController @see AEnemyController::SetAllowDirectControlStochasticDecisionTree
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bAllowDirectControlStochasticDecisionTree = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bAllowBuilding = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="bAllowBuilding"))
	TArray<FVector> StartBuildLocations;
};
