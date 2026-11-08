// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Behaviours/Derived/Heal/TickingHealBehaviour.h"
#include "FieldWorkshopRepairBehaviour.generated.h"

/**
 * @brief Repair the field workshop's repair aura (URadiusRepairAura) gives every damaged vehicle in its radius.
 * The aura re-applies it every interval, which refreshes its lifetime; the aura removes it when the vehicle leaves
 * and when the workshop packs up or dies (UFieldWorkshopComponent tears the aura down). Set as the aura's behaviour
 * type in the workshop Blueprint.
 */
UCLASS()
class RTS_SURVIVAL_API UFieldWorkshopRepairBehaviour : public UTickingHealBehaviour
{
	GENERATED_BODY()

public:
	UFieldWorkshopRepairBehaviour();
};
