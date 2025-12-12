// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LandscapeDeformComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API ULandscapeDeformComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	ULandscapeDeformComponent();

	float GetDeformRadius() const { return Radius; }
protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// The radius in which the component affects the landscape.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float Radius = 100.0f;

	

};
