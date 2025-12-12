
#pragma once
#include "CoreMinimal.h"

#include "DamageReduction.generated.h"


USTRUCT(BlueprintType, Blueprintable)
struct FDamageReductionSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite , Category = "DamageReduction")
	float DamageReduction = 0.0f;

	// Multiplier for the incoming damage, if the unit has 20% damage reduction set this to 0.8.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite , Category = "DamageReduction")
	float DamageReductionMlt = 1.0f;

	
	// Keep aggregate-friendly defaults, and add a constexpr convenience ctor.
	constexpr FDamageReductionSettings() = default;
	constexpr FDamageReductionSettings(const float DamageReductionPoints, const float DamageReductionMultiplier)
		: DamageReduction(DamageReductionPoints), DamageReductionMlt(DamageReductionMultiplier) {}
	
};
