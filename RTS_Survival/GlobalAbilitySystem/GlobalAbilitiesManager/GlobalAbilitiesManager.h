// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GlobalAbilitiesManager.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UGlobalAbilitiesManager : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UGlobalAbilitiesManager();
	
	void InitGlobalAbilitiesManager(const int32 OwningPlayer);

protected:
	// Called when the game starts
	virtual auto BeginPlay() -> void override;
	
	
	private:
	UPROPERTY()
	int32 M_OwningPlayer;

};
