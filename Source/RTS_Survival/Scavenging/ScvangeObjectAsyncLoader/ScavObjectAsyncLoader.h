// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/MasterObjects/ActorObjectsMaster.h"
#include "ScavObjectAsyncLoader.generated.h"

class AScavengeableObject;

UCLASS()
class RTS_SURVIVAL_API AScavObjectAsyncLoader : public AActorObjectsMaster
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AScavObjectAsyncLoader(const FObjectInitializer& ObjectInitializer);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	void OnScavObjectLoaded();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Scavenging")
	TSoftClassPtr<AScavengeableObject> ScavObjectToLoad;

};
