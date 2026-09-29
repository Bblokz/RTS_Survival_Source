#pragma once

#include "CoreMinimal.h"
#include "FRTSFireEffectParams.generated.h"

/** Niagara user parameters applied whenever a pooled fire effect is activated. */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FRTSFireEffectParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire Effect")
	float Scale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire Effect")
	FVector FireColorMlt = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fire Effect")
	FVector SmokeColorMlt = FVector::OneVector;
};
