// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "SquadUnit.h"
#include "SquadUnit_Medic.generated.h"

class UMedicComponent;

/**
 * @brief Derive the medic unit Blueprints from this class. It fights like any squad unit and, through its
 * medic component, heals injured allied squad units: by itself while its squad is idle, or on the squad's
 * IdHeal command. Adjust MedicComponent's HealSettings in the constructor of the derived Blueprint.
 */
UCLASS()
class RTS_SURVIVAL_API ASquadUnit_Medic : public ASquadUnit
{
	GENERATED_BODY()

public:
	ASquadUnit_Medic(const FObjectInitializer& ObjectInitializer);

protected:
	// Holds the heal settings (search radius and period, heal amount, follow distance) and runs the healing.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Medic")
	TObjectPtr<UMedicComponent> MedicComponent;
};
