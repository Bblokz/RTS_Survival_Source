// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InGameUpgradeActor.generated.h"

UCLASS()
class RTS_SURVIVAL_API AInGameUpgradeActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AInGameUpgradeActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
};
