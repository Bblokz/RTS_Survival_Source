// Copyright (C) Bas Blokzijl - All rights reserved.

#include "BehaviourWeapon.h"

#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "Engine/World.h"
#include "RTS_Survival/Weapons/BombComponent/BombComponent.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"

namespace BehaviourWeaponConstants
{
        constexpr float NextFrameDelaySeconds = 0.0f;
        constexpr int32 DefaultWeaponStackCount = 10;
}

UBehaviourWeapon::UBehaviourWeapon()
{
        BehaviourStackRule = EBehaviourStackRule::Stack;
        M_MaxStackCount = BehaviourWeaponConstants::DefaultWeaponStackCount;
}

void UBehaviourWeapon::OnAdded()
{
        if (bM_HasInitializedPostBeginPlayLogic)
        {
                return;
        }

        SetupInitializationForOwner();
}

void UBehaviourWeapon::OnRemoved()
{
        RemoveBehaviourFromTrackedWeapons();
        ClearTimers();
}

void UBehaviourWeapon::OnStack(UBehaviour* StackedBehaviour)
{
        static_cast<void>(StackedBehaviour);

        TArray<UWeaponState*> Weapons;
        if (not TryGetAircraftWeapons(Weapons) && not TryGetTankWeapons(Weapons) && not TryGetSquadWeapons(Weapons))
        {
                return;
        }

        for (UWeaponState* WeaponState : Weapons)
        {
                if (WeaponState == nullptr)
                {
                        continue;
                }

                if (not CheckRequirement(WeaponState))
                {
                        continue;
                }

                OnWeaponBehaviourStack(WeaponState);
        }
}

bool UBehaviourWeapon::CheckRequirement(UWeaponState* WeaponState) const
{
        return WeaponState != nullptr;
}

void UBehaviourWeapon::ApplyBehaviourToWeapon(UWeaponState* WeaponState)
{
}

void UBehaviourWeapon::RemoveBehaviourFromWeapon(UWeaponState* WeaponState)
{
}

void UBehaviourWeapon::OnWeaponBehaviourStack(UWeaponState* WeaponState)
{
}

void UBehaviourWeapon::PostBeginPlayLogicInitialized()
{
        if (bM_HasInitializedPostBeginPlayLogic)
        {
                return;
        }

        bM_HasInitializedPostBeginPlayLogic = true;
        TArray<UWeaponState*> Weapons;
        if (TryGetAircraftWeapons(Weapons) || TryGetTankWeapons(Weapons) || TryGetSquadWeapons(Weapons))
        {
                ApplyBehaviourToMountedWeapons(Weapons);
        }
}

void UBehaviourWeapon::ApplyBehaviourToMountedWeapons(const TArray<UWeaponState*>& Weapons)
{
        for (UWeaponState* WeaponState : Weapons)
        {
                if (WeaponState == nullptr)
                {
                        continue;
                }

                if (not CheckRequirement(WeaponState))
                {
                        continue;
                }

                ApplyBehaviourToWeapon(WeaponState);
                M_AffectedWeapons.Add(WeaponState);
        }
}

void UBehaviourWeapon::RemoveBehaviourFromTrackedWeapons()
{
        for (const TWeakObjectPtr<UWeaponState>& WeaponPtr : M_AffectedWeapons)
        {
                if (not WeaponPtr.IsValid())
                {
                        continue;
                }

                RemoveBehaviourFromWeapon(WeaponPtr.Get());
        }

        M_AffectedWeapons.Empty();
}

void UBehaviourWeapon::SetupInitializationForOwner()
{
        if (bM_HasInitializedPostBeginPlayLogic)
        {
                return;
        }

        ASquadController* SquadController = nullptr;
        if (TryGetSquadController(SquadController))
        {
                SetupInitializationForSquadController();
                return;
        }

        SchedulePostBeginPlayLogic();
}

void UBehaviourWeapon::SetupInitializationForSquadController()
{
        if (bM_HasInitializedPostBeginPlayLogic)
        {
                return;
        }

        ASquadController* SquadController = nullptr;
        if (not TryGetSquadController(SquadController))
        {
                return;
        }

        if (SquadController->GetIsSquadFullyLoaded())
        {
                SchedulePostBeginPlayLogic();
                return;
        }

        RegisterSquadFullyLoadedCallback(SquadController);
}

void UBehaviourWeapon::SchedulePostBeginPlayLogic()
{
        if (bM_HasInitializedPostBeginPlayLogic)
        {
                return;
        }

        UWorld* World = GetWorld();
        if (World == nullptr)
        {
                return;
        }

        World->GetTimerManager().SetTimer(M_PostBeginPlayTimerHandle, this,
                &UBehaviourWeapon::PostBeginPlayLogicInitialized, BehaviourWeaponConstants::NextFrameDelaySeconds, false);
}

bool UBehaviourWeapon::TryGetAircraftWeapons(TArray<UWeaponState*>& OutWeapons) const
{
        AAircraftMaster* AircraftMaster = nullptr;
        if (not TryGetAircraftMaster(AircraftMaster))
        {
                return false;
        }

        UBombComponent* BombComponent = nullptr;
        OutWeapons = FRTSWeaponHelpers::GetWeaponsMountedOnAircraft(AircraftMaster, BombComponent);
        return true;
}

bool UBehaviourWeapon::TryGetTankWeapons(TArray<UWeaponState*>& OutWeapons) const
{
        ATankMaster* TankMaster = nullptr;
        if (not TryGetTankMaster(TankMaster))
        {
                return false;
        }

        OutWeapons = FRTSWeaponHelpers::GetWeaponsMountedOnTank(TankMaster);
        return true;
}

bool UBehaviourWeapon::TryGetSquadWeapons(TArray<UWeaponState*>& OutWeapons) const
{
        ASquadController* SquadController = nullptr;
        if (not TryGetSquadController(SquadController))
        {
                return false;
        }

        OutWeapons = SquadController->GetWeaponsOfSquad();
        return SquadController->GetIsSquadFullyLoaded();
}

bool UBehaviourWeapon::TryGetAircraftMaster(AAircraftMaster*& OutAircraftMaster) const
{
        OutAircraftMaster = Cast<AAircraftMaster>(GetOwningActor());
        return OutAircraftMaster != nullptr;
}

bool UBehaviourWeapon::TryGetTankMaster(ATankMaster*& OutTankMaster) const
{
        OutTankMaster = Cast<ATankMaster>(GetOwningActor());
        return OutTankMaster != nullptr;
}

bool UBehaviourWeapon::TryGetSquadController(ASquadController*& OutSquadController) const
{
        OutSquadController = Cast<ASquadController>(GetOwningActor());
        return OutSquadController != nullptr;
}

void UBehaviourWeapon::ClearTimers()
{
        UWorld* World = GetWorld();
        if (World == nullptr)
        {
                return;
        }

        FTimerManager& TimerManager = World->GetTimerManager();
        if (TimerManager.TimerExists(M_PostBeginPlayTimerHandle))
        {
                TimerManager.ClearTimer(M_PostBeginPlayTimerHandle);
        }
}

void UBehaviourWeapon::RegisterSquadFullyLoadedCallback(ASquadController* SquadController)
{
        if (bM_HasInitializedPostBeginPlayLogic)
        {
                return;
        }

        if (SquadController == nullptr)
        {
                return;
        }

        const TFunction<void()> SquadLoadedCallback = [this]()
        {
                if (bM_HasInitializedPostBeginPlayLogic)
                {
                        return;
                }

                ASquadController* LocalSquadController = nullptr;
                if (not TryGetSquadController(LocalSquadController))
                {
                        return;
                }

                if (not LocalSquadController->GetIsSquadFullyLoaded())
                {
                        return;
                }

                SchedulePostBeginPlayLogic();
        };

        SquadController->SquadDataCallbacks.CallbackOnSquadDataLoaded(SquadLoadedCallback, this);
}
