// Copyright (C) Bas Blokzijl - All rights reserved.

#include "BehaviourSquadMovement.h"

#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace BehaviourSquadMovementConstants
{
        constexpr int32 DefaultMovementStackCount = 10;
}

UBehaviourSquadMovement::UBehaviourSquadMovement()
{
        BehaviourStackRule = EBehaviourStackRule::Stack;
        M_MaxStackCount = BehaviourSquadMovementConstants::DefaultMovementStackCount;
}

void UBehaviourSquadMovement::OnAdded(AActor* BehaviourOwner)
{
        bM_IsBehaviourActive = true;
        SetupInitializationForOwner();
        // Make sure to call the bp event.
        Super::OnAdded(BehaviourOwner);
}

void UBehaviourSquadMovement::OnRemoved(AActor* BehaviourOwner)
{
        RemoveBehaviourFromSquadUnits();
        ClearTrackedUnits();
        bM_HasAppliedMovementAtLeastOnce = false;
        bM_IsBehaviourActive = false;
        // Make sure to call the bp event.
        Super::OnRemoved(BehaviourOwner);
}

void UBehaviourSquadMovement::OnStack(UBehaviour* StackedBehaviour)
{
        static_cast<void>(StackedBehaviour);

        SetupInitializationForOwner();

        if (not bM_HasAppliedMovementAtLeastOnce)
        {
                return;
        }

        ApplyBehaviourToSquad();
}

void UBehaviourSquadMovement::SetupInitializationForOwner()
{
        ASquadController* SquadController = nullptr;
        if (not TryGetSquadController(SquadController))
        {
                return;
        }

        if (SquadController->GetIsSquadFullyLoaded())
        {
                HandleSquadDataLoaded();
                return;
        }

        RegisterSquadFullyLoadedCallback(SquadController);
}

void UBehaviourSquadMovement::RegisterSquadFullyLoadedCallback(ASquadController* SquadController)
{
        if (bM_HasRegisteredSquadLoadedCallback)
        {
                return;
        }

        if (SquadController == nullptr)
        {
                return;
        }

        TWeakObjectPtr<UBehaviourSquadMovement> WeakThis(this);
        const TFunction<void()> SquadLoadedCallback = [WeakThis]()
        {
                if (not WeakThis.IsValid())
                {
                        return;
                }

                WeakThis->HandleSquadDataLoaded();
        };

        SquadController->SquadDataCallbacks.CallbackOnSquadDataLoaded(SquadLoadedCallback, this);
        bM_HasRegisteredSquadLoadedCallback = true;
}

void UBehaviourSquadMovement::HandleSquadDataLoaded()
{
        if (not bM_IsBehaviourActive)
        {
                return;
        }

        if (not GetIsValidSquadController())
        {
                return;
        }

        ApplyBehaviourToSquad();
        bM_HasAppliedMovementAtLeastOnce = true;
}

void UBehaviourSquadMovement::ApplyBehaviourToSquad()
{
        TArray<ASquadUnit*> SquadUnits;
        if (not TryGetSquadUnits(SquadUnits))
        {
                return;
        }

        for (ASquadUnit* SquadUnit : SquadUnits)
        {
                ApplyBehaviourToSquadUnit(SquadUnit);
        }
}

void UBehaviourSquadMovement::ApplyBehaviourToSquadUnit(ASquadUnit* SquadUnit)
{
        if (SquadUnit == nullptr)
        {
                return;
        }

        UCharacterMovementComponent* CharacterMovementComponent = SquadUnit->GetCharacterMovement();
        if (CharacterMovementComponent == nullptr)
        {
                return;
        }

        const TWeakObjectPtr<ASquadUnit> SquadUnitPtr = SquadUnit;
        FBehaviourSquadMovementSettingsCache& MovementCache =
                M_MovementCachePerSquadUnit.FindOrAdd(SquadUnitPtr, FBehaviourSquadMovementSettingsCache());

        ApplyMovementToMovementComponent(CharacterMovementComponent, MovementCache);
        M_AffectedSquadUnits.AddUnique(SquadUnitPtr);
}

void UBehaviourSquadMovement::RemoveBehaviourFromSquadUnits()
{
        for (const TWeakObjectPtr<ASquadUnit>& SquadUnitPtr : M_AffectedSquadUnits)
        {
                if (not SquadUnitPtr.IsValid())
                {
                        M_MovementCachePerSquadUnit.Remove(SquadUnitPtr);
                        continue;
                }

                RestoreMovementForUnit(SquadUnitPtr.Get());
        }
}

void UBehaviourSquadMovement::CacheOriginalSettings(UCharacterMovementComponent* MovementComponent,
        FBehaviourSquadMovementSettingsCache& Cache) const
{
        if (MovementComponent == nullptr)
        {
                return;
        }

        if (Cache.bHasCachedValues)
        {
                return;
        }

        Cache.BaseMaxWalkSpeed = MovementComponent->MaxWalkSpeed;
        Cache.BaseMaxAcceleration = MovementComponent->MaxAcceleration;
        Cache.BaseBrakingDecelerationWalking = MovementComponent->BrakingDecelerationWalking;
        Cache.BaseGroundFriction = MovementComponent->GroundFriction;
        Cache.BaseBrakingFrictionFactor = MovementComponent->BrakingFrictionFactor;
        Cache.BaseRotationRate = MovementComponent->RotationRate;
        Cache.bHasCachedValues = true;
}

namespace BehaviourSquadMovementHelpers
{
        FBehaviourSquadMovementMultipliers CombineMultipliers(const FBehaviourSquadMovementMultipliers& Current,
                const FBehaviourSquadMovementMultipliers& Additional)
        {
                FBehaviourSquadMovementMultipliers Result;
                Result.MaxWalkSpeedMultiplier = Current.MaxWalkSpeedMultiplier * Additional.MaxWalkSpeedMultiplier;
                Result.MaxAccelerationMultiplier =
                        Current.MaxAccelerationMultiplier * Additional.MaxAccelerationMultiplier;
                Result.BrakingDecelerationWalkingMultiplier =
                        Current.BrakingDecelerationWalkingMultiplier * Additional.BrakingDecelerationWalkingMultiplier;
                Result.GroundFrictionMultiplier = Current.GroundFrictionMultiplier * Additional.GroundFrictionMultiplier;
                Result.BrakingFrictionFactorMultiplier =
                        Current.BrakingFrictionFactorMultiplier * Additional.BrakingFrictionFactorMultiplier;
                Result.RotationRateYawMultiplier = Current.RotationRateYawMultiplier * Additional.RotationRateYawMultiplier;
                return Result;
        }
}

void UBehaviourSquadMovement::ApplyMovementToMovementComponent(UCharacterMovementComponent* MovementComponent,
        FBehaviourSquadMovementSettingsCache& Cache) const
{
        if (MovementComponent == nullptr)
        {
                return;
        }

        CacheOriginalSettings(MovementComponent, Cache);

        Cache.CurrentAppliedMultiplier =
                BehaviourSquadMovementHelpers::CombineMultipliers(Cache.CurrentAppliedMultiplier,
                        BehaviourMovementMultipliers);

        MovementComponent->MaxWalkSpeed = Cache.BaseMaxWalkSpeed * Cache.CurrentAppliedMultiplier.MaxWalkSpeedMultiplier;
        MovementComponent->MaxAcceleration =
                Cache.BaseMaxAcceleration * Cache.CurrentAppliedMultiplier.MaxAccelerationMultiplier;
        MovementComponent->BrakingDecelerationWalking = Cache.BaseBrakingDecelerationWalking *
                Cache.CurrentAppliedMultiplier.BrakingDecelerationWalkingMultiplier;
        MovementComponent->GroundFriction = Cache.BaseGroundFriction *
                Cache.CurrentAppliedMultiplier.GroundFrictionMultiplier;
        MovementComponent->BrakingFrictionFactor = Cache.BaseBrakingFrictionFactor *
                Cache.CurrentAppliedMultiplier.BrakingFrictionFactorMultiplier;

        const float NewYawRotationRate = Cache.BaseRotationRate.Yaw * Cache.CurrentAppliedMultiplier.RotationRateYawMultiplier;
        MovementComponent->RotationRate.Yaw = NewYawRotationRate;
}

void UBehaviourSquadMovement::RestoreMovementForUnit(ASquadUnit* SquadUnit)
{
        if (SquadUnit == nullptr)
        {
                return;
        }

        UCharacterMovementComponent* CharacterMovementComponent = SquadUnit->GetCharacterMovement();
        if (CharacterMovementComponent == nullptr)
        {
                return;
        }

        const TWeakObjectPtr<ASquadUnit> SquadUnitPtr = SquadUnit;
        FBehaviourSquadMovementSettingsCache* MovementCache = M_MovementCachePerSquadUnit.Find(SquadUnitPtr);
        if (MovementCache == nullptr)
        {
                return;
        }

        if (MovementCache->bHasCachedValues)
        {
                CharacterMovementComponent->MaxWalkSpeed = MovementCache->BaseMaxWalkSpeed;
                CharacterMovementComponent->MaxAcceleration = MovementCache->BaseMaxAcceleration;
                CharacterMovementComponent->BrakingDecelerationWalking = MovementCache->BaseBrakingDecelerationWalking;
                CharacterMovementComponent->GroundFriction = MovementCache->BaseGroundFriction;
                CharacterMovementComponent->BrakingFrictionFactor = MovementCache->BaseBrakingFrictionFactor;
                CharacterMovementComponent->RotationRate = MovementCache->BaseRotationRate;
        }

        M_MovementCachePerSquadUnit.Remove(SquadUnitPtr);
}

void UBehaviourSquadMovement::ClearTrackedUnits()
{
        M_AffectedSquadUnits.Empty();
        M_MovementCachePerSquadUnit.Empty();
        M_SquadController.Reset();
        bM_HasRegisteredSquadLoadedCallback = false;
}

bool UBehaviourSquadMovement::TryGetSquadUnits(TArray<ASquadUnit*>& OutSquadUnits)
{
        OutSquadUnits.Reset();

        ASquadController* SquadController = nullptr;
        if (not TryGetSquadController(SquadController))
        {
                return false;
        }

        if (not SquadController->GetIsSquadFullyLoaded())
        {
                return false;
        }

        OutSquadUnits = SquadController->GetSquadUnitsChecked();
        return OutSquadUnits.Num() > 0;
}

bool UBehaviourSquadMovement::TryGetSquadController(ASquadController*& OutSquadController)
{
        OutSquadController = M_SquadController.Get();
        if (OutSquadController != nullptr)
        {
                return true;
        }

        OutSquadController = Cast<ASquadController>(GetOwningActor());
        if (OutSquadController == nullptr)
        {
                return false;
        }

        M_SquadController = OutSquadController;
        return true;
}

bool UBehaviourSquadMovement::GetIsValidSquadController() const
{
        if (not M_SquadController.IsValid())
        {
                RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, TEXT("M_SquadController") 
                        TEXT("GetIsValidSquadController"), __func__);
                return false;
        }

        return true;
}
