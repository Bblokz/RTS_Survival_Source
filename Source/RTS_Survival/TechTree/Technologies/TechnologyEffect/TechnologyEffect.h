// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TechnologyEffect.generated.h"

/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UTechnologyEffect : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	
	virtual void ApplyTechnologyEffect(const UObject* WorldContextObject);

	
protected:
	/** Returns the class of actors to search for. Override in child classes. */
	virtual TSet<TSubclassOf<AActor>> GetTargetActorClasses() const { return {}; }

    /** Called when the actors have been found.*/
    virtual void OnApplyEffectToActor(AActor* ValidActor);

private:
    /** Finds actors synchronously and calls OnActorsFound */
    void FindActors(const UObject* WorldContextObject);

	void OnActorsFound(const TArray<AActor*>& FoundActors);

};
