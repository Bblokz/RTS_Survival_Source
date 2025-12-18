#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Behaviours/Derived/BehaviourWeapon/BehaviourWeapon.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponSystems.h"

#include "BehaviourRifleAccuracy.generated.h"

class UWeaponState;

/**
 * @brief Permanent rifle-specific behaviour that boosts weapon accuracy.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API UBehaviourRifleAccuracy : public UBehaviourWeapon
{
        GENERATED_BODY()

public:
        UBehaviourRifleAccuracy();


protected:
        virtual bool CheckRequirement(UWeaponState* WeaponState) const override;
};
