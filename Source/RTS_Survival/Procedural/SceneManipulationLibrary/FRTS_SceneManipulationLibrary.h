#pragma once

#include "CoreMinimal.h"

struct FWeightedStaticMesh;
struct FWeightedDecalMaterial;

class RTS_SURVIVAL_API FRTS_SceneManipulationLibrary
{
public:
	/** @brief Add random degrees of rotation to the component; relatively to the current scene component rotation.
	 * @return The chosen rotation. 
	 */
	static float SetCompRandomRotation(
		const float MinRotXY,
		const float MaxRotXY,
		const float MinRotZ,
		const float MaxRotZ,
		USceneComponent* Component);

	/** @brief Randomly, uniformly scale the component.
	 * @return The chosen scale.
	 */
	static float SetCompRandomScale(
		const float MinScale,
		const float MaxScale,
		USceneComponent* Component);


	static float SetComponentScaleOnAxis(
		const float MinScale,
		const float MaxScale,
		const bool bScaleX,
		const bool bScaleY,
		const bool bScaleZ,
		USceneComponent* Component);

	/** @brief Add Radom relatlive offset on X and Y to the relative location of the component.
	 * @return The chosen offset.*/
	static float SetCompRandomOffset(
		const float MinOffsetXY,
		const float MaxOffsetXY,
		USceneComponent* Component);

	/**
	 * Picks random materials for each decal based on weighted chances.
	 * @return indices chosen from the provided array
	 */
	static TArray<int32> SetRandomDecalsMaterials(
		const TArray<FWeightedDecalMaterial>& DecalMaterials,
		TArray<UDecalComponent*> DecalComponents
	);

	/**
     * Picks random meshes for each component based on weighted chances.
     * @return indices chosen from the provided array
     */
	static TArray<int32> SetRandomStaticMesh(
		const TArray<FWeightedStaticMesh>& Meshes,
		TArray<UStaticMeshComponent*> Components
	);

	static int32 PickWeightedIndex(const TArray<float>& Weights);
	
};
