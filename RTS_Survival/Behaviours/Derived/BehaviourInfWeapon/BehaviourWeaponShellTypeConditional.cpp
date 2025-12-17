// Copyright (C) Bas Blokzijl - All rights reserved.

#include "BehaviourWeaponShellTypeConditional.h"

#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

UBehaviourWeaponShellTypeConditional::UBehaviourWeaponShellTypeConditional()
{
}

void UBehaviourWeaponShellTypeConditional::ApplyBehaviourToWeapon(UWeaponState* WeaponState)
{
        if (WeaponState == nullptr)
        {
                return;
        }

        RegisterShellTypeChangedDelegate(WeaponState);

        const EWeaponShellType CurrentShellType = WeaponState->GetRawWeaponData().ShellType;
        if (not DoesShellTypeAllowBehaviour(CurrentShellType))
        {
                return;
        }

        const TWeakObjectPtr<UWeaponState> WeaponPtr = WeaponState;
        if (M_WeaponsWithActiveModifiers.Contains(WeaponPtr))
        {
                return;
        }

        UBehaviourWeapon::ApplyBehaviourToWeapon(WeaponState);
        M_WeaponsWithActiveModifiers.Add(WeaponPtr);
}

void UBehaviourWeaponShellTypeConditional::RemoveBehaviourFromWeapon(UWeaponState* WeaponState)
{
        if (WeaponState == nullptr)
        {
                return;
        }

        const TWeakObjectPtr<UWeaponState> WeaponPtr = WeaponState;
        if (not M_WeaponsWithActiveModifiers.Contains(WeaponPtr))
        {
                return;
        }

        UBehaviourWeapon::RemoveBehaviourFromWeapon(WeaponState);
        M_WeaponsWithActiveModifiers.Remove(WeaponPtr);
}

void UBehaviourWeaponShellTypeConditional::OnRemoved()
{
        UnregisterAllShellTypeDelegates();
        Super::OnRemoved();
        M_WeaponsWithActiveModifiers.Empty();
}

bool UBehaviourWeaponShellTypeConditional::DoesShellTypeAllowBehaviour(const EWeaponShellType ShellType) const
{
        return AllowedShellTypes.Contains(ShellType);
}

void UBehaviourWeaponShellTypeConditional::RegisterShellTypeChangedDelegate(UWeaponState* WeaponState)
{
        if (WeaponState == nullptr)
        {
                return;
        }

        const TWeakObjectPtr<UWeaponState> WeaponPtr = WeaponState;
        if (M_ShellTypeChangedHandles.Contains(WeaponPtr))
        {
                return;
        }

        const FDelegateHandle DelegateHandle = WeaponState->OnWeaponShellTypeChanged.AddLambda(
                [this, WeaponPtr](const EWeaponShellType NewShellType)
                {
                        HandleShellTypeChanged(WeaponPtr, NewShellType);
                });

        M_ShellTypeChangedHandles.Add(WeaponPtr, DelegateHandle);
}

void UBehaviourWeaponShellTypeConditional::HandleShellTypeChanged(
        const TWeakObjectPtr<UWeaponState> WeaponPtr,
        const EWeaponShellType NewShellType)
{
        if (not WeaponPtr.IsValid())
        {
                M_WeaponsWithActiveModifiers.Remove(WeaponPtr);
                M_ShellTypeChangedHandles.Remove(WeaponPtr);
                return;
        }

        UWeaponState* WeaponState = WeaponPtr.Get();
        const bool bShellTypeSupported = DoesShellTypeAllowBehaviour(NewShellType);
        const bool bHasActiveModifiers = M_WeaponsWithActiveModifiers.Contains(WeaponPtr);

        if (not bShellTypeSupported)
        {
                if (not bHasActiveModifiers)
                {
                        return;
                }

                UBehaviourWeapon::RemoveBehaviourFromWeapon(WeaponState);
                M_WeaponsWithActiveModifiers.Remove(WeaponPtr);
                return;
        }

        if (bHasActiveModifiers)
        {
                return;
        }

        UBehaviourWeapon::ApplyBehaviourToWeapon(WeaponState);
        M_WeaponsWithActiveModifiers.Add(WeaponPtr);
}

void UBehaviourWeaponShellTypeConditional::UnregisterAllShellTypeDelegates()
{
        for (const TPair<TWeakObjectPtr<UWeaponState>, FDelegateHandle>& DelegatePair : M_ShellTypeChangedHandles)
        {
                const TWeakObjectPtr<UWeaponState> WeaponPtr = DelegatePair.Key;
                if (not WeaponPtr.IsValid())
                {
                        continue;
                }

                WeaponPtr->OnWeaponShellTypeChanged.Remove(DelegatePair.Value);
        }

        M_ShellTypeChangedHandles.Empty();
}
