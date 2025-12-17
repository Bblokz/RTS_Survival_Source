// Copyright (C) Bas Blokzijl

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Behaviours/Derived/BehaviourWeapon/BehaviourWeapon.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "BehaviourWeaponShellTypeConditional.generated.h"

class UWeaponState;

/**
 * @brief Holds a weak weapon state pointer plus its delegate handle.
 *
 * The weak pointer is a UPROPERTY so GC can track it safely.
 * The delegate handle is native (not exposed to reflection) but stored
 * inside the struct for unbinding later.
 */
USTRUCT()
struct RTS_SURVIVAL_API FShellTypeDelegateEntry
{
        GENERATED_BODY()

        /** Weak pointer to the weapon state (safe for GC) */
        UPROPERTY()
        TWeakObjectPtr<UWeaponState> WeaponState;

        /** Native delegate handle (not reflected) */
        FDelegateHandle DelegateHandle;
};

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

        /** Shell types for which this behaviour will apply its modifiers */
        UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behaviour|ShellType")
        TArray<EWeaponShellType> AllowedShellTypes;

private:
        bool DoesShellTypeAllowBehaviour(const EWeaponShellType ShellType) const;

        void RegisterShellTypeChangedDelegate(UWeaponState* WeaponState);
        void HandleShellTypeChanged(const TWeakObjectPtr<UWeaponState> WeaponPtr, const EWeaponShellType NewShellType);
        void UnregisterAllShellTypeDelegates();

        /** Weapons with modifiers currently active */
        UPROPERTY()
        TSet<TWeakObjectPtr<UWeaponState>> ActiveWeapons;

        /** Reflected list of delegate entries for safety */
        UPROPERTY()
        TArray<FShellTypeDelegateEntry> ShellTypeDelegates;
};
