// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/MasterObjects/ActorObjectsMaster.h"

#include "RTSNavigator.generated.h"


class RTS_SURVIVAL_API UVehiclePathFollowingComponent;

USTRUCT()
struct FVehicleAgent
{
	GENERATED_BODY()

	UPROPERTY()
	UVehiclePathFollowingComponent* Vehicle = nullptr;

	UPROPERTY()
	float AgentRadius = 100.0f;

	UPROPERTY()
	float AgentQueryRange = 1000.0f;
	
};

class RTS_SURVIVAL_API UVehiclePathFollowingComponent;
UCLASS()
class RTS_SURVIVAL_API ARTSNavigator : public AActorObjectsMaster
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ARTSNavigator(const FObjectInitializer& ObjectInitializer);

	void RegisterWithNavigator(UVehiclePathFollowingComponent* Vehicle);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

private:
	TArray<UVehiclePathFollowingComponent*> M_TVehicles;
};
