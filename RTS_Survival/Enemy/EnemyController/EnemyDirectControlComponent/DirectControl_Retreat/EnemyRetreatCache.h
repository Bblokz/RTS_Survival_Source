#pragma once

#include "CoreMinimal.h"

#include "EnemyRetreatCache.generated.h"

struct FDamagedTanksRetreatGroup;



USTRUCT()
struct FDirectControlRetreatCache
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FDamagedTanksRetreatGroup> M_CachedRetreatGroups;

	UPROPERTY()
	int32 M_LastRequestID = INDEX_NONE;

};

