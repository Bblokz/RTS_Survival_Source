// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/TechTree/Technologies/TechnologyEffect/TechnologyEffect.h"
#include "TE_PzJager.generated.h"

/**
 * 
 */
UCLASS()
class RTS_SURVIVAL_API UTE_PzJager : public UTechnologyEffect
{
	GENERATED_BODY()

public:
	virtual void ApplyTechnologyEffect(const UObject* WorldContextObject) override;


protected:
	virtual void OnApplyEffectToActor(AActor* ValidActor) override;
};
