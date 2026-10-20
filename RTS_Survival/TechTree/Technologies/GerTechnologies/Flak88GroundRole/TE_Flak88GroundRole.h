// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/TechTree/Technologies/TechnologyEffect/TechnologyEffect.h"
#include "TE_Flak88GroundRole.generated.h"

class ASquadController;
class UBehaviour;

/**
 * @brief Gives the 8.8 cm Flak team weapon a toggleable ground-fire mode: while active the mode behaviour
 * raises the gun's armour penetration against tanks.
 * @note GroundRoleModeBehaviour: set in the TEBP to the weapon behaviour the mode applies.
 */
UCLASS()
class RTS_SURVIVAL_API UTE_Flak88GroundRole : public UTechnologyEffect
{
	GENERATED_BODY()

public:
	UTE_Flak88GroundRole();

protected:
	virtual void ApplyOnSquad_Internal(ASquadController* Squad) override;

	// Applied while the ground-fire mode is active; removed again when it is switched off.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Technology|Flak 88")
	TSubclassOf<UBehaviour> GroundRoleModeBehaviour;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Technology|Flak 88")
	int32 PreferredModeAbilityIndex;
};
