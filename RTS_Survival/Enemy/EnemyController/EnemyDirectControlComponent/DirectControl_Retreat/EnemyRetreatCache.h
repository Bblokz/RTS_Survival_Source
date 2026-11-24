#pragma once

#include "CoreMinimal.h"
// The cache stores retreat groups by value, so its generated constructor needs the full struct, not a declaration.
#include "RTS_Survival/Enemy/StrategicAI/Requests/StrategicAIRequests.h"

#include "EnemyRetreatCache.generated.h"


USTRUCT()
struct FDirectControlRetreatCache
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FDamagedTanksRetreatGroup> M_CachedRetreatGroups;

	UPROPERTY()
	int32 M_LastRequestID = INDEX_NONE;

};

