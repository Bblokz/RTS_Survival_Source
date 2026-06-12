// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Behaviours/Behaviour.h"
#include "BehaviourRepairUpgrade.generated.h"

/**
 * @brief Permanent cosmetic marker for units affected by the Repair Upgrade technology.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API UBehaviourRepairUpgrade : public UBehaviour
{
	GENERATED_BODY()

public:
	UBehaviourRepairUpgrade();
};
