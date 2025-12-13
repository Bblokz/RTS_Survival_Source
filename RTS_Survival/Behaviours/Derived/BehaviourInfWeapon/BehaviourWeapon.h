// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Behaviours/Behaviour.h"
#include "TimerManager.h"

#include "BehaviourWeapon.generated.h"

class UWeaponState;
class AAircraftMaster;
class ASquadController;
class ATankMaster;
class UBombComponent;

/**
 * @brief Weapon behaviour base that applies changes to mounted weapons once owners are ready.
 * @note Uses async-aware initialisation so weapons that load after BeginPlay still receive behaviour changes.
 */
UCLASS()
class RTS_SURVIVAL_API UBehaviourWeapon : public UBehaviour
{
        GENERATED_BODY()

public:
        UBehaviourWeapon();

protected:
        virtual void OnAdded() override;
        virtual void OnRemoved() override;
        virtual void OnStack(UBehaviour* StackedBehaviour) override;

        /** @brief Determine whether this behaviour should be applied to the provided weapon. */
        virtual bool CheckRequirement(UWeaponState* WeaponState) const;

        /** @brief Apply the behaviour to the target weapon. */
        virtual void ApplyBehaviourToWeapon(UWeaponState* WeaponState);

        /** @brief Remove the behaviour from the target weapon. */
        virtual void RemoveBehaviourFromWeapon(UWeaponState* WeaponState);

        /** @brief Hook executed when a new stack is added; override for stack-specific logic. */
        virtual void OnWeaponBehaviourStack(UWeaponState* WeaponState);

private:
        void PostBeginPlayLogicInitialized();
        void ApplyBehaviourToMountedWeapons(const TArray<UWeaponState*>& Weapons);
        void RemoveBehaviourFromTrackedWeapons();
        void SetupInitializationForOwner();
        void SetupInitializationForSquadController();
        void RegisterSquadFullyLoadedCallback(ASquadController* SquadController);
        void SchedulePostBeginPlayLogic();
        bool TryGetAircraftWeapons(TArray<UWeaponState*>& OutWeapons) const;
        bool TryGetTankWeapons(TArray<UWeaponState*>& OutWeapons) const;
        bool TryGetSquadWeapons(TArray<UWeaponState*>& OutWeapons) const;
        bool TryGetAircraftMaster(AAircraftMaster*& OutAircraftMaster) const;
        bool TryGetTankMaster(ATankMaster*& OutTankMaster) const;
        bool TryGetSquadController(ASquadController*& OutSquadController) const;
        void ClearTimers();

        UPROPERTY()
        TArray<TWeakObjectPtr<UWeaponState>> M_AffectedWeapons;

        FTimerHandle M_PostBeginPlayTimerHandle;
        bool bM_HasInitializedPostBeginPlayLogic = false;
};
