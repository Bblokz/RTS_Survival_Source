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

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bAllowBuilding = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="bAllowBuilding"))
	TArray<FVector> StartBuildLocations;
};
