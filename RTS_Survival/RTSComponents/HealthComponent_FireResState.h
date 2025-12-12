#pragma once

#include "CoreMinimal.h"

#include "HealthComponent_FireResState.generated.h"

USTRUCT()
struct FHealthComp_FireThresholdState
{
	GENERATED_BODY()

	// Amount of FireThreshold that has build up.
	UPROPERTY()
	float CurrentFireThreshold = 0.f;
	
	UPROPERTY()
	float FireRecoveryPerSec = 25.f;

	// Max amount of the fire threshold that can build up before the unit takes fire damage.
	UPROPERTY()
	float MaxFireThreshold = 50;

	inline FString GetDebugString()const
	{
		FString DebugString = TEXT("Max Fire Thresh. / Curr Fire Thresh. : ") + FString::SanitizeFloat(MaxFireThreshold) + TEXT(" / ") + FString::SanitizeFloat(CurrentFireThreshold);
		DebugString += TEXT("\n Fire Recovery Per Sec : ") + FString::SanitizeFloat(FireRecoveryPerSec);
		return DebugString;
	}
};
