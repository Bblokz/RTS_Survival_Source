// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ActorObjectsMaster.generated.h"

class UGameUnitManager;

UCLASS()
class RTS_SURVIVAL_API AActorObjectsMaster : public AActor
{
	GENERATED_BODY()

public:
	AActorObjectsMaster(const FObjectInitializer& ObjectInitializer);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// To register/ deregister the unit with the GameUnitManager.
	UPROPERTY()
	TObjectPtr<UGameUnitManager> M_GameUnitManager;

	bool GetIsValidGameUnitManger()const;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
