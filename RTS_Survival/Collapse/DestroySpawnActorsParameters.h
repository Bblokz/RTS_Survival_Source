#pragma once

#include "CoreMinimal.h"

#include "DestroySpawnActorsParameters.generated.h"

USTRUCT(BlueprintType)
struct FDestroySpawnActor
{
	GENERATED_BODY()
	// Soft pointer to the actor to spawn.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftClassPtr<AActor> ActorToSpawn = nullptr;

	// Offset location , rotation and possible scale to spawn the actor at.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FTransform SpawnTransform = FTransform::Identity;
	
};

USTRUCT(BlueprintType)
struct FDestroySpawnActorsParameters
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FDestroySpawnActor> ActorsToSpawn;

	// After spawning the actors, how long to wait before destroying the owner actor?
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DelayTillDestroyActor = 1.f;
};