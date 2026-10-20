// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/TechTree/Technologies/TechnologyEffect/TechnologyEffect.h"
#include "TE_GhillieSuits.generated.h"

class ASquadController;

/**
 * @brief Gives snipers and Gebirgsjager more health; Vultures are left out on purpose.
 */
UCLASS()
class RTS_SURVIVAL_API UTE_GhillieSuits : public UTechnologyEffect
{
	GENERATED_BODY()

public:
	UTE_GhillieSuits();

protected:
	virtual void ApplyOnSquad_Internal(ASquadController* Squad) override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Technology|Health")
	float M_HealthMultiplier;
};
