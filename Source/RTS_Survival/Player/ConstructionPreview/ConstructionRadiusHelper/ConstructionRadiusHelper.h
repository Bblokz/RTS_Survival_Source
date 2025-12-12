// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ConstructionRadiusHelper.generated.h"

class URadiusComp;
class UExpansionRadiusComp;

UCLASS()
class RTS_SURVIVAL_API AConstructionRadiusHelper : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AConstructionRadiusHelper(const FObjectInitializer& ObjectInitializer);

	URadiusComp* GetRadiusComp();
	void SetConstructionRadius(float Radius);
	void SetRadiusVisibility(bool bVisible);
	

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Components")
	UExpansionRadiusComp* ExpansionRadiusComp;

	bool GetIsValidExpansionRadiusComp();

};
