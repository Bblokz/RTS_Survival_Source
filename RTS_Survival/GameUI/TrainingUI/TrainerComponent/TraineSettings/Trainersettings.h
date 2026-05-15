#pragma once

#include "CoreMinimal.h"

#include "Trainersettings.generated.h"
USTRUCT(Blueprintable)
struct FTrainerSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float TimeNotSelectable = 2.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	UMeshComponent* TrainingMesh = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FName SpawnSocketName = "None";

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FName DefaultWayPointSocketName = "None";

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	bool StartEnabled = false;

	// Not blueprint configurable; set depending on enemy trainer component setup call.
	UPROPERTY()
	bool bUsesProgressionBar = false;
	
	// Not blueprint configurable; set depending on enemy trainer component setup call.
	UPROPERTY()
	bool bIsEnemyTrainer = false;
};
