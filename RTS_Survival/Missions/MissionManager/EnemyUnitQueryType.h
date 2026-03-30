#pragma once

#include "CoreMinimal.h"
#include "EnemyUnitQueryType.generated.h"

UENUM(BlueprintType)
enum class EEnemyUnitQueryType : uint8
{
	None UMETA(DisplayName = "None"),
	TrackTanks UMETA(DisplayName = "Track Tanks"),
	TrackSquads UMETA(DisplayName = "Track Squads"),
	TrackTanksAndSquads UMETA(DisplayName = "Track Tanks And Squads")
};
