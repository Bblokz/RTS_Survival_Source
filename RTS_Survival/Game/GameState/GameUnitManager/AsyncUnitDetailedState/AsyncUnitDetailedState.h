#pragma once

#include "CoreMinimal.h"


#include "AsyncUnitDetailedState.generated.h"

enum class EAbilityID : uint8;
enum class EAllUnitType : uint8;

USTRUCT()
struct FAsyncDetailedUnitState
{
	GENERATED_BODY()

	FAsyncDetailedUnitState();

	int32 OwningPlayer;

	TWeakObjectPtr<AActor> UnitActorPtr;
	
	EAllUnitType UnitType ;
	
	int32 UnitSubtypeRaw ;

	EAbilityID CurrentActiveAbility;

	bool bIsInCombat;

	float HealthRatio;

	FVector UnitLocation;

	FRotator UnitRotation;
	
};
