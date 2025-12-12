// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PlayerStartLocation.generated.h"

class UFowComp;
class UPlayerStartLocationManager;

UCLASS()
class RTS_SURVIVAL_API APlayerStartLocation : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	APlayerStartLocation();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// The vision radius used on the FOW component.
	UPROPERTY(EditDefaultsOnly)
	float VisionRadiusInFOW = 12000.0f;



private:
	void RegisterWithPlayerStartLocationManager();

	bool GetIsValidFOWComponent() const;

	void Debug_RegisterWithPlayerStartLocationManager() const;

	UPROPERTY()
	TObjectPtr<UFowComp> M_FowComponent;
};
