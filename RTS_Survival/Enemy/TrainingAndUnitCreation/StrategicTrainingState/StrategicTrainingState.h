#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Enemy/TrainingAndUnitCreation/EnemyAITechLevel/EnemyAITechLevel.h"

#include "StrategicTrainingState.generated.h"


enum EEnemyAITechLevel : uint8;

USTRUCT()
struct FEnemyStrategicTrainingState
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<EEnemyAITechLevel, bool> TechLevelUnlockedMap = {};

	 FEnemyLevelTraining EnemyLevelTraining;
};
