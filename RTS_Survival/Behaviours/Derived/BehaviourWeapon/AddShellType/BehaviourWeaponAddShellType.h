// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Behaviours/Derived/BehaviourWeapon/AddShellTypes/BehaviourAddShellTypes.h"

#include "BehaviourWeaponAddShellType.generated.h"

/**
 * @brief Weapon behaviour used by designers to grant optional shell choices without forcing selection.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API UBehaviourWeaponAddShellType : public UBehaviourAddShellTypes
{
	GENERATED_BODY()

public:
	UBehaviourWeaponAddShellType();
};
