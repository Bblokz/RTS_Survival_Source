// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Behaviours/Derived/BehaviourWeapon/BehaviourWeapon.h"
#include "BehaviourWeaponWurfrahmenShop.generated.h"

/**
 * @brief Faster rocket reload on a Panzerwerfer while its owner has a Wurfrahmen shop standing. Added and removed only by
 * UPanzerwerferReloadBonusSubsystem; exclusive, so a second shop never stacks a second instance.
 */
UCLASS()
class RTS_SURVIVAL_API UBehaviourWeaponWurfrahmenShop : public UBehaviourWeapon
{
	GENERATED_BODY()

public:
	UBehaviourWeaponWurfrahmenShop();
};
