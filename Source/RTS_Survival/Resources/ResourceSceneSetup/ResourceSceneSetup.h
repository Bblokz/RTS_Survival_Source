#pragma once
#include "CoreMinimal.h"

#include "ResourceSceneSetup.generated.h"

/** Minimal struct for decal materials and their selection weight. */
USTRUCT(BlueprintType)
struct FWeightedDecalMaterial
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="SceneManipulation")
    UMaterialInterface* Material = nullptr;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="SceneManipulation")
    float Weight = 1.f;
};

/** Minimal struct for static meshes and their selection weight. */
USTRUCT(BlueprintType)
struct FWeightedStaticMesh
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="SceneManipulation")
    UStaticMesh* Mesh = nullptr;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="SceneManipulation")
    float Weight = 1.f;
};
