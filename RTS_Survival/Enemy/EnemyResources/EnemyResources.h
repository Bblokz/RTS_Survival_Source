#pragma once

#include "CoreMinimal.h"

#include "EnemyResources.generated.h"


USTRUCT(BlueprintType)
struct FEnemyResources
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 WaveSupply = 20;
};