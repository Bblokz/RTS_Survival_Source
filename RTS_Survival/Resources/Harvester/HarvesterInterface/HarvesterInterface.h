// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Resources/ResourceStorageOwner/ResourceStorageOwner.h"
#include "UObject/Interface.h"
#include "HarvesterInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(Blueprintable)
class UHarvesterInterface : public UResourceStorageOwner
{
	GENERATED_BODY()
};

/**
 * 
 */
class RTS_SURVIVAL_API IHarvesterInterface: public IResourceStorageOwner
{
	GENERATED_BODY()


public:
	/** Blueprint event for playing the harvester animation. */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Harvester")
	void PlayHarvesterAnimation();

	virtual void OnResourceStorageChanged(int32 PercentageResourcesFilled, const ERTSResourceType ResourceType) override;

	/** Blueprint event for stopping the harvester animation. */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Harvester")
	void StopHarvesterAnimation();

	virtual void OnResourceStorageEmpty() override;
};
