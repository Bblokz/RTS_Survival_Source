#include "BehaviourRifleAccuracy.h"

#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

namespace BehaviourRifleAccuracyConstants
{
        constexpr int32 AccuracyBonus = 15;
        constexpr int32 MaxStackCount = 2;
}

UBehaviourRifleAccuracy::UBehaviourRifleAccuracy()
{
        BehaviourLifeTime = EBehaviourLifeTime::None;
        M_MaxStackCount = BehaviourRifleAccuracyConstants::MaxStackCount;
        BehaviourWeaponAttributes.Accuracy = BehaviourRifleAccuracyConstants::AccuracyBonus;
}

bool UBehaviourRifleAccuracy::CheckRequirement(UWeaponState* WeaponState) const
{
        if (not WeaponState)
        {
                return false;
        }

        const FWeaponData& WeaponData = WeaponState->GetRawWeaponData();
        return Global_IsRifle(WeaponData.WeaponName);
}
