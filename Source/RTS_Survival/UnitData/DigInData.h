#pragma once

#include "CoreMinimal.h"

#include "DigInData.generated.h"


USTRUCT(BlueprintType)
struct FDigInData
{
	GENERATED_BODY()
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DigIn")
	float DigInTime = 5.0f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DigIn")
	float DigInWallHealth = 100.0f;
};
