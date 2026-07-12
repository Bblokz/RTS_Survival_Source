#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/NavCollision/RTSNavCollision.h"
#include "RTSNavCollisionEnvObstacle.generated.h"


/**
 * @brief Configures navigation obstacles from component unions or an exact oriented box.
 * Use the exact-box setup when a generated object already has authoritative bounds.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API URTSNavCollisionEnvObstacle : public URTSNavCollision
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	URTSNavCollisionEnvObstacle();

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SetUpNavModifierVolumeAsUnionOfComponents(TArray<USceneComponent*> ComponentsInSceneWithBounds,
	                                               USceneComponent* RootComponent);

	/**
	 * @brief Preserves authoritative oriented bounds without inflating them through a world AABB.
	 * @param WorldCenter Center of the modifier in world space.
	 * @param LocalExtent Unrotated half-size of the modifier.
	 * @param WorldRotation World-space orientation of the modifier.
	 */
	void SetUpNavModifierVolume(
		const FVector& WorldCenter,
		const FVector& LocalExtent,
		const FQuat& WorldRotation);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void ScaleCurrentExtent(const float Scale);
};
