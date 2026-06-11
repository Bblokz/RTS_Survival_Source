// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Behaviours/Derived/BehaviourWeapon/BehaviourWeapon.h"
#include "BehaviourWeaponAutoLoader.generated.h"

/**
 * @brief Permanent weapon behaviour used by Auto Loader tech blueprints to improve sustained fire.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API UBehaviourWeaponAutoLoader : public UBehaviourWeapon
{
	GENERATED_BODY()

public:
	UBehaviourWeaponAutoLoader();
};
