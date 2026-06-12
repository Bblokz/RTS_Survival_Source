// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/TechTree/Technologies/TechnologyEffect/TechnologyEffect.h"
#include "TE_RepairUpgrade.generated.h"

class ASquadController;

/**
 * @brief Applies stronger field repairs to eligible German repair-capable squads after research.
 */
UCLASS()
class RTS_SURVIVAL_API UTE_RepairUpgrade : public UTechnologyEffect
{
	GENERATED_BODY()

public:
	UTE_RepairUpgrade();

protected:
	virtual void ApplyOnSquad_Internal(ASquadController* Squad) override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Technology|Repair")
	float M_RepairMultiplier;
};
