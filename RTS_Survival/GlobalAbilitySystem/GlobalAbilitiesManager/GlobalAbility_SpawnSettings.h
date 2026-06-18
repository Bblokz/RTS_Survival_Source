#pragma once
#include "CoreMinimal.h"

#include "GlobalAbility_SpawnSettings.generated.h"

USTRUCT()
struct FGlobalAbility_SpawnSettings
{
	GENERATED_BODY()
	
	FVector PlayerStartLocation = FVector(0.0f, 0.0f, 0.0f);
	FVector Fow_ManagerPosition= FVector(0.0f, 0.0f, 0.0f);
	float MapExtent = 0.f;
	float AircratHeightStart = 0.f;
	FVector AircraftStartLocation = FVector(0.0f, 0.0f, 0.0f);
		
};
