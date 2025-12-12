#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/NavCollision/RTSNavCollision.h"
#include "RTSNavCollisionEnvObstacle.generated.h"


// Can update its bounds given the array of scene comps.
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

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void ScaleCurrentExtent(const float Scale);
};
