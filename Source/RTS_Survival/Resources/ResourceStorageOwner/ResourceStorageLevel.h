#pragma once

#include "CoreMinimal.h"

#include "ResourceStorageLevel.generated.h"

// Defines a pair of level and static mesh, this mesh will be placed once the percentage level is reached.
USTRUCT(BlueprintType)
struct FStorageMeshLevel
{
	GENERATED_BODY()

	// Limit to range 1 - 100, at n percentage for percentage >= LevelPercentage the mesh will be placed.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ClampMin = 1, ClampMax = 100))
	int32 Level = 0;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UStaticMesh* Mesh = nullptr;
	
};

// A wrapper around an array to expose the storage levels to blueprints. Make sure that the provided MeshLevels are
// sorted from high to low level.
USTRUCT(BlueprintType)
struct FResourceStorageLevels
{
	GENERATED_BODY()

	// Make sure that the storage levels are ordered from highest to lowest.	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FStorageMeshLevel> StorageLevels;
};
