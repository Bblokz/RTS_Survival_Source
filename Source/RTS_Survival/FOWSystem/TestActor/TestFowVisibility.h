// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/FOWSystem/FowVisibilityInterface/FowVisibility.h"
#include "TestFowVisibility.generated.h"

class UFowComp;

UCLASS()
class RTS_SURVIVAL_API ATestFowVisibility : public AActor, public IFowVisibility
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ATestFowVisibility(const FObjectInitializer& ObjectInitializer);

	virtual void OnFowVisibilityUpdated(const float Visibility) override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;

	virtual void PostInitializeComponents() override;

	UPROPERTY(BlueprintReadWrite)
	FVector DrawTextLocation;

	// Fow component set for player units.
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UFowComp* FowComponent;
};
