// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "RallyPoint.generated.h"

/**
 * @brief Lightweight rally point marker for a Trainer. Does not tick.
 * Hidden by default after spawn; used as a target for newly trained units.
 */
UCLASS()
class RTS_SURVIVAL_API ARTSRallyPointActor : public AActor
{
	GENERATED_BODY()

public:
	ARTSRallyPointActor();

protected:
	virtual void BeginPlay() override;

private:
	// Root scene to ensure a stable transform parent.
	UPROPERTY()
	TObjectPtr<USceneComponent> M_SimpleRoot;
};
