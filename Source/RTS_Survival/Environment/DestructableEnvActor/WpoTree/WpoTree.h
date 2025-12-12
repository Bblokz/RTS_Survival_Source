// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Environment/DestructableEnvActor/DestructableEnvActor.h"
#include "WpoTree.generated.h"

class UWpoDistanceManager;

UCLASS()
class RTS_SURVIVAL_API AWpoTree : public ADestructableEnvActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AWpoTree(const FObjectInitializer& ObjectInitializer);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void PostInitializeComponents() override;

private:
	UPROPERTY()
	UWpoDistanceManager* WpoDistanceManager = nullptr;

	bool EnsureIsValidWpoDistanceManager() const;

};
