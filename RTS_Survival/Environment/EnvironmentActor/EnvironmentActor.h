// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/MasterObjects/ActorObjectsMaster.h"
#include "EnvironmentActor.generated.h"

struct FWeightedStaticMesh;
struct FWeightedDecalMaterial;

UCLASS()
class RTS_SURVIVAL_API AEnvironmentActor : public AActorObjectsMaster
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AEnvironmentActor(const FObjectInitializer& ObjectInitializer);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	
	/** @brief Add random degrees of rotation to the component; relatively to the current scene component rotation.
	 * @return The chosen rotation. 
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="SceneManipulation")
	float SetCompRandomRotation(
		const float MinRotXY,
		const float MaxRotXY,
		const float MinRotZ,
		const float MaxRotZ,
		USceneComponent* Component);

	/** @brief Randomly, uniformly scale the component.
	 * @return The chosen scale.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="SceneManipulation")
	float SetCompRandomScale(
		const float MinScale,
		const float MaxScale,
		USceneComponent* Component);

	
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="SceneManipulation")
	float SetComponentScaleOnAxis(
		const float MinScale,
		const float MaxScale,
		const bool bScaleX,
		const bool bScaleY,
		const bool bScaleZ,
		USceneComponent* Component);

	/** @brief Add Radom relatlive offset on X and Y to the relative location of the component.
	 * @return The chosen offset.*/
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="SceneManipulation")
	float SetCompRandomOffset(
		const float MinOffsetXY,
		const float MaxOffsetXY,
		USceneComponent* Component);

	/**
	 * Picks random materials for each decal based on weighted chances.
	 * @return indices chosen from the provided array
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="SceneManipulation")
	TArray<int32> SetRandomDecalsMaterials(
		const TArray<FWeightedDecalMaterial>& DecalMaterials,
		TArray<UDecalComponent*> DecalComponents
	);

	/**
	 * Picks random meshes for each component based on weighted chances.
	 * @return indices chosen from the provided array
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="SceneManipulation")
	TArray<int32> SetRandomStaticMesh(
		const TArray<FWeightedStaticMesh>& Meshes,
		TArray<UStaticMeshComponent*> Components
	);


};
