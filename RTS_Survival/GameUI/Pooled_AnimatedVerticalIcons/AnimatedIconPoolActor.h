#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AnimatedIconPoolActor.generated.h"

class UWidgetComponent;

/** @brief Provides a stable owner for every pooled icon component, even while attached to a gameplay actor. */
UCLASS(NotBlueprintable, Transient)
class RTS_SURVIVAL_API AAnimatedIconPoolActor : public AActor
{
	GENERATED_BODY()

public:
	AAnimatedIconPoolActor();
	UWidgetComponent* CreateIconComponent(UClass* WidgetClass);

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> M_PoolRoot;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidgetComponent>> M_IconComponents;

	bool GetIsValidPoolRoot() const;
};
