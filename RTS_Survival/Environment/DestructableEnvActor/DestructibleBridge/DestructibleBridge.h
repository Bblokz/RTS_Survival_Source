// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Environment/DestructableEnvActor/DestructableEnvActor.h"
#include "Engine/EngineTypes.h"
#include "DestructibleBridge.generated.h"

/** @brief A destructible bridge that removes actors standing on it when it collapses. */
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

private:
	void HandleAsyncBridgeSweepComplete(const FTraceHandle& TraceHandle, FTraceDatum& TraceDatum);

	void TriggerKillOnActor(AActor* ActorToKill) const;


};
