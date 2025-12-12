// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Environment/DestructableEnvActor/WpoTree/WpoTreeDebugger.h"
#include "WpoDistanceManager.generated.h"


struct FWpoTreeDebugger;

USTRUCT(Blueprintable)
struct FWpoSettings
{
	GENERATED_BODY()

	// by default set to the distance we consider a unit out of view for the unit optimization component.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly , Category = "WPO Settings")
	float WpoDistanceDisable = DeveloperSettings::Optimization::OutOfFOVConsideredFarAwayUnit;
};


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UWpoDistanceManager : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UWpoDistanceManager();

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SetupWpoDistance(UStaticMeshComponent* MeshComponent) const;	

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FWpoSettings WpoSettings;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FWpoTreeDebugger WpoTreeDebugger;

};
