// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/TechTree/Technologies/TechnologyEffect/TechnologyEffect.h"
#include "TE_ScavengingSpeed.generated.h"

class ASquadController;

/**
 * @brief Applies faster scavenging to eligible German scavenger squads when the tech manager resolves them.
 */
UCLASS()
class RTS_SURVIVAL_API UTE_ScavengingSpeed : public UTechnologyEffect
{
	GENERATED_BODY()

public:
	UTE_ScavengingSpeed();

protected:
	virtual void ApplyOnSquad_Internal(ASquadController* Squad) override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Technology|Scavenging")
	float M_ScavengingSpeedMultiplier;
};
