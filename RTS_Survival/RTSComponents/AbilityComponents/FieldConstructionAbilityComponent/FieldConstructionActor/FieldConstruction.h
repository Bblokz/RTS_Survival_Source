// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Environment/DestructableEnvActor/DestructableEnvActor.h"
#include "FieldConstruction.generated.h"

UCLASS()
class RTS_SURVIVAL_API AFieldConstruction : public ADestructableEnvActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AFieldConstruction(const FObjectInitializer& ObjectInitializer);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void PostInitializeComponents() override;
	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UStaticMeshComponent> FieldConstructionMesh;

	bool GetIsValidFieldConstructionMesh();

	UFUNCTION(BlueprintCallable, notNotBlueprintable)
	void SetupCollision(const int32 OwningPlayer, const bool bBlockPlayerProjectiles);
};
