// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/FactionSystem/FactionSelection/FactionPlayerController.h"
#include "WorldProfileAndUIManager.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UWorldProfileAndUIManager : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UWorldProfileAndUIManager();
	
	void OnWorldLoaded(const bool bGeneratedNewWorld, ERTSFaction PlayerFaction);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

};
