// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Environment/EnvironmentActor/EnvironmentActor.h"
#include "GroundEnvActor.generated.h"

UCLASS()
class RTS_SURVIVAL_API AGroundEnvActor : public AEnvironmentActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AGroundEnvActor(const FObjectInitializer& ObjectInitializer);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	
	UFUNCTION(BlueprintCallable)
	void SetupCollision(TArray<UMeshComponent*> Components, const bool bAllowBuilding = false, const bool bAffectNavMesh = false);

};
