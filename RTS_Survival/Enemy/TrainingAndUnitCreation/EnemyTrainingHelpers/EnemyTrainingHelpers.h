#pragma once

#include "CoreMinimal.h"


enum class ESquadSubtype : uint8;
enum class ETankSubtype : uint8;
enum class EBuildingExpansionType : uint8;
struct FEnemyTrainingOptionsForTechLevel;
enum EEnemyAITechLevel : uint8;

namespace EnemyTrainingHelpers
{
	int32 GetSquadTrainingPointCost(const ESquadSubtype SquadType);
	int32 GetTankTrainingPointsCost(const ETankSubtype TankType);
	bool GetIsTechLevelUnlockedByBxpCounts(
		const FEnemyTrainingOptionsForTechLevel& TrainingOptions,
		const TMap<EBuildingExpansionType, int32>& BxpCountsByType);
	void UpdateTechLevelUnlockedMapForOptions(
		TMap<EEnemyAITechLevel, bool>& TechLevelUnlockedMap,
		const FEnemyTrainingOptionsForTechLevel& TrainingOptions,
		const TMap<EBuildingExpansionType, int32>& BxpCountsByType);
}
