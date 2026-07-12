// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "ReinforcementPathBackupActor.generated.h"

class USceneComponent;

/**
 * @brief Place this marker where a reinforcement road may end when no primary connection is viable.
 * Derived Blueprint classes are discovered by the Get Reinforcement Path Backup Actor Data node as well.
 */
UCLASS(BlueprintType, Blueprintable)
class RTS_SURVIVAL_API AReinforcementPathBackupActor : public AActor
{
	GENERATED_BODY()

public:
	AReinforcementPathBackupActor();

private:
	UPROPERTY(VisibleAnywhere, Category = "Reinforcement Path")
	TObjectPtr<USceneComponent> M_RootComponent;
};
