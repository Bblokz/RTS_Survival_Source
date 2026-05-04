// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TacticalAIComp.generated.h"

// Component added to enemy units at beginplay.
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UTacticalAIComp : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UTacticalAIComp();

	virtual void OnUnitInCombat();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

private:


};
