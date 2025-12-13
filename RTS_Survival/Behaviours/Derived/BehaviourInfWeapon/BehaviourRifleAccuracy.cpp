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

void UBehaviourRifleAccuracy::ApplyBehaviourToWeapon(UWeaponState* WeaponState)
{
        if (not WeaponState)
        {
                return;
        }

        FWeaponData* WeaponData = WeaponState->GetWeaponDataToUpgrade();
        if (not WeaponData)
        {
                return;
        }

        WeaponData->Accuracy += BehaviourRifleAccuracyConstants::AccuracyBonus;
}

void UBehaviourRifleAccuracy::RemoveBehaviourFromWeapon(UWeaponState* WeaponState)
{
        if (not WeaponState)
        {
                return;
        }

        FWeaponData* WeaponData = WeaponState->GetWeaponDataToUpgrade();
        if (not WeaponData)
        {
                return;
        }

        WeaponData->Accuracy -= BehaviourRifleAccuracyConstants::AccuracyBonus;
}
