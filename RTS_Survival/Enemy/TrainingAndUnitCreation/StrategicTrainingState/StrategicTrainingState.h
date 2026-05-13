#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Enemy/TrainingAndUnitCreation/EnemyAITechLevel/EnemyAITechLevel.h"

#include "StrategicTrainingState.generated.h"


enum EEnemyAITechLevel : uint8;

USTRUCT()
struct FEnemyStrategicTrainingState
{
	GENERATED_BODY()
	// The start value is set by the Enemy controller when propagating the settings to the strategic ai component.
	UPROPERTY()
	int32 TrainingPoints = 0;
	
	UPROPERTY()
	TMap<EEnemyAITechLevel, bool> TechLevelUnlockedMap = {};
	
	UPROPERTY()
	 FEnemyLevelTraining EnemyLevelTraining = {};
	
};
