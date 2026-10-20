// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/TechTree/Technologies/TechnologyEffect/TechnologyEffect.h"
#include "TE_EntrenchingTools.generated.h"

class ASquadController;

/**
 * @brief Makes the building squads (Sturmpioniere and both scavenger squads) build field defences faster
 * through every field construction ability they carry.
 */
UCLASS()
class RTS_SURVIVAL_API UTE_EntrenchingTools : public UTechnologyEffect
{
	GENERATED_BODY()

public:
	UTE_EntrenchingTools();

protected:
	virtual void ApplyOnSquad_Internal(ASquadController* Squad) override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Technology|Construction")
	float M_ConstructionSpeedMultiplier;
};
