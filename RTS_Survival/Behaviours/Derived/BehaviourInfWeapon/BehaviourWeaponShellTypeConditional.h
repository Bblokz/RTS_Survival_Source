// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Behaviours/Derived/BehaviourInfWeapon/BehaviourWeapon.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

#include "BehaviourWeaponShellTypeConditional.generated.h"

class UWeaponState;

/**
 * @brief Weapon behaviour that only activates while the owning weapon uses an allowed shell type.
 * @note Listens for shell-type changes to apply or remove its modifiers immediately.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API UBehaviourWeaponShellTypeConditional : public UBehaviourWeapon
{
        GENERATED_BODY()

public:
        UBehaviourWeaponShellTypeConditional();

protected:
        virtual void ApplyBehaviourToWeapon(UWeaponState* WeaponState) override;
        virtual void RemoveBehaviourFromWeapon(UWeaponState* WeaponState) override;
        virtual void OnRemoved() override;

        /** Shell types for which this behaviour will apply its modifiers. */
        UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behaviour|ShellType")
        TArray<EWeaponShellType> AllowedShellTypes;

private:
        bool DoesShellTypeAllowBehaviour(const EWeaponShellType ShellType) const;
        void RegisterShellTypeChangedDelegate(UWeaponState* WeaponState);
        void HandleShellTypeChanged(const TWeakObjectPtr<UWeaponState> WeaponPtr, const EWeaponShellType NewShellType);
        void UnregisterAllShellTypeDelegates();

        UPROPERTY()
        TSet<TWeakObjectPtr<UWeaponState>> M_WeaponsWithActiveModifiers;

        UPROPERTY()
        TMap<TWeakObjectPtr<UWeaponState>, FDelegateHandle> M_ShellTypeChangedHandles;
};
