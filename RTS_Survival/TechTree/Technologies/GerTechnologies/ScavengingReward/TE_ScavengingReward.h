// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/TechTree/Technologies/TechnologyEffect/TechnologyEffect.h"
#include "TE_ScavengingReward.generated.h"

class ASquadController;

/**
 * @brief Applies increased scavenging payouts to eligible German scavenger squads when researched.
 */
UCLASS()
class RTS_SURVIVAL_API UTE_ScavengingReward : public UTechnologyEffect
{
	GENERATED_BODY()

public:
	UTE_ScavengingReward();

protected:
	virtual void ApplyOnSquad_Internal(ASquadController* Squad) override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Technology|Scavenging")
	float M_ScavengingRewardMultiplier;
};
