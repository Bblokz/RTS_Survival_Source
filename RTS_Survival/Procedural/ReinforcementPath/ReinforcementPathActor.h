// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "ReinforcementPathActor.generated.h"

class USceneComponent;

/**
 * @brief Place this marker in a level to provide a reinforcement-road start transform to PCG.
 * Derived Blueprint classes are discovered by the Get Reinforcement Path Actor Data node as well.
 */
UCLASS(BlueprintType, Blueprintable)
class RTS_SURVIVAL_API AReinforcementPathActor : public AActor
{
	GENERATED_BODY()

public:
	AReinforcementPathActor();

private:
	UPROPERTY(VisibleAnywhere, Category = "Reinforcement Path")
	TObjectPtr<USceneComponent> M_RootComponent;
};
