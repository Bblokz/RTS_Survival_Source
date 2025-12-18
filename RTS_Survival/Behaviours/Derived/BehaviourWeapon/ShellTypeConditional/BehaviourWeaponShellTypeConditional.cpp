// Copyright (C) Bas Blokzijl

#include "BehaviourWeaponShellTypeConditional.h"

UBehaviourWeaponShellTypeConditional::UBehaviourWeaponShellTypeConditional() = default;

void UBehaviourWeaponShellTypeConditional::ApplyBehaviourToWeapon(UWeaponState* WeaponState)
{
        if (!IsValid(WeaponState))
        {
                return;
        }

        RegisterShellTypeChangedDelegate(WeaponState);

        const EWeaponShellType CurrentShellType = WeaponState->GetRawWeaponData().ShellType;
        if (!DoesShellTypeAllowBehaviour(CurrentShellType))
        {
                return;
        }

        const TWeakObjectPtr<UWeaponState> WeaponPtr = WeaponState;
        if (ActiveWeapons.Contains(WeaponPtr))
        {
                return;
        }

        Super::ApplyBehaviourToWeapon(WeaponState);
        ActiveWeapons.Add(WeaponPtr);
}

void UBehaviourWeaponShellTypeConditional::RemoveBehaviourFromWeapon(UWeaponState* WeaponState)
{
        if (!IsValid(WeaponState))
        {
                return;
        }

        const TWeakObjectPtr<UWeaponState> WeaponPtr = WeaponState;
        if (!ActiveWeapons.Contains(WeaponPtr))
        {
                return;
        }

        Super::RemoveBehaviourFromWeapon(WeaponState);
        ActiveWeapons.Remove(WeaponPtr);
}

void UBehaviourWeaponShellTypeConditional::OnRemoved(AActor* BehaviourOwner)
{
        UnregisterAllShellTypeDelegates();
        ActiveWeapons.Empty();

        // make sure to call bp event.
        Super::OnRemoved(BehaviourOwner);
}

void UBehaviourWeaponShellTypeConditional::OnAdded(AActor* BehaviourOwner)
{
        // make sure to call bp event.
        Super::OnAdded(BehaviourOwner);
}

bool UBehaviourWeaponShellTypeConditional::DoesShellTypeAllowBehaviour(const EWeaponShellType ShellType) const
{
        return AllowedShellTypes.Contains(ShellType);
}

void UBehaviourWeaponShellTypeConditional::RegisterShellTypeChangedDelegate(UWeaponState* WeaponState)
{
        if (!IsValid(WeaponState))
        {
                return;
        }

        const TWeakObjectPtr<UWeaponState> WeaponPtr = WeaponState;

        // If already registered, do nothing
        for (const FShellTypeDelegateEntry& Entry : ShellTypeDelegates)
        {
                if (Entry.WeaponState == WeaponPtr)
                {
                        return;
                }
        }

        FShellTypeDelegateEntry NewEntry;
        NewEntry.WeaponState = WeaponState;

        NewEntry.DelegateHandle = WeaponState->OnWeaponShellTypeChanged.AddLambda(
                [this, WeaponPtr](const EWeaponShellType NewShellType)
                {
                        HandleShellTypeChanged(WeaponPtr, NewShellType);
                });

        ShellTypeDelegates.Add(MoveTemp(NewEntry));
}

void UBehaviourWeaponShellTypeConditional::HandleShellTypeChanged(
        const TWeakObjectPtr<UWeaponState> WeaponPtr,
        const EWeaponShellType NewShellType)
{
        if (!WeaponPtr.IsValid())
        {
                // Cleanup stale entry
                ActiveWeapons.Remove(WeaponPtr);

                ShellTypeDelegates.RemoveAll(
                        [WeaponPtr](const FShellTypeDelegateEntry& Entry)
                        {
                                return Entry.WeaponState == WeaponPtr;
                        });

                return;
        }

        UWeaponState* State = WeaponPtr.Get();
        const bool bIsAllowed        = DoesShellTypeAllowBehaviour(NewShellType);
        const bool bHasModifiers     = ActiveWeapons.Contains(WeaponPtr);

        if (bIsAllowed && !bHasModifiers)
        {
                Super::ApplyBehaviourToWeapon(State);
                ActiveWeapons.Add(WeaponPtr);
        }
        else if (!bIsAllowed && bHasModifiers)
        {
                Super::RemoveBehaviourFromWeapon(State);
                ActiveWeapons.Remove(WeaponPtr);
        }
}

void UBehaviourWeaponShellTypeConditional::UnregisterAllShellTypeDelegates()
{
        for (const FShellTypeDelegateEntry& Entry : ShellTypeDelegates)
        {
                if (Entry.WeaponState.IsValid())
                {
                        Entry.WeaponState->OnWeaponShellTypeChanged.Remove(Entry.DelegateHandle);
                }
        }

        ShellTypeDelegates.Empty();
}
