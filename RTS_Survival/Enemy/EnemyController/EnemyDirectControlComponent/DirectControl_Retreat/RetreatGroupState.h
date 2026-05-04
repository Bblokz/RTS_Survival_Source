#pragma once

#include "CoreMinimal.h"

#include "RetreatGroupState.generated.h"

UENUM()
enum class EEnemyRetreatGroupState : uint8
{
	NotInitialized,
	HazmatsMoveAndRepairTanks,
	TanksOnlyRetreating,
	TanksOnlyWaiting
};
