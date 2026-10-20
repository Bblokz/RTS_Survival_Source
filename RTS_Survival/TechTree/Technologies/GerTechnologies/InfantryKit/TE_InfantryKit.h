// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/TechTree/Technologies/TechnologyEffect/TechnologyEffect.h"
#include "TE_InfantryKit.generated.h"

class ASquadController;

/**
 * @brief Gives the line infantry squads more health; the bonus survives reinforcements and stacks with Ghillie Suits.
 */
UCLASS()
class RTS_SURVIVAL_API UTE_InfantryKit : public UTechnologyEffect
{
	GENERATED_BODY()

public:
	UTE_InfantryKit();

protected:
	virtual void ApplyOnSquad_Internal(ASquadController* Squad) override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Technology|Health")
	float M_HealthMultiplier;
};
