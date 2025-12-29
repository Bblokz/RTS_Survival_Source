// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Environment/DestructableEnvActor/DestructableEnvActor.h"
#include "DestructibleBridge.generated.h"

UCLASS()
class RTS_SURVIVAL_API ADestructibleBridge : public ADestructableEnvActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ADestructibleBridge(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void KillActorsOnBridge(UMeshComponent* BridgeMesh);


};
