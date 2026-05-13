#pragma once

#include "CoreMinimal.h"


enum class ESquadSubtype : uint8;
enum class ETankSubtype : uint8;

namespace EnemyTrainingHelpers
{
	int32 GetSquadTrainingPointCost(const ESquadSubtype SquadType);
	int32 GetTankTrainingPointsCost(const ETankSubtype TankType);
}
