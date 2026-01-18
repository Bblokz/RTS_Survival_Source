// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Behaviours/Behaviour.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "TimerManager.h"

#include "BehaviourWeapon.generated.h"

class UWeaponState;
class AAircraftMaster;
class ASquadController;
class ATankMaster;
class UBombComponent;

USTRUCT(BlueprintType)
struct FBehaviourWeaponMultipliers
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    float DamageMlt = 0.0f;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    float RangeMlt = 0.0f;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    float WeaponCalibreMlt = 0.0f;

    // Base cooldown time between individual shots, measured in seconds.
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    float BaseCooldownMlt = 0.0f;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    float ReloadSpeedMlt = 0.0f;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    float AccuracyMlt = 0.0f;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    float MagSizeMlt = 0.0f;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    float ArmorPenetrationMlt = 0.0f;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    float TnTGramsMlt = 0.0f;

    // Range of the Area of Effect (AOE) explosion in centimeters, applicable for AOE-enabled projectiles.
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    float ShrapnelRangeMlt = 0.0f;

    // Damage dealt by each projectile in an AOE attack, relevant for AOE-enabled projectiles.
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    float ShrapnelDamageMlt = 0.0f;

    // Number of shrapnel particles generated in an AOE attack, applicable for AOE-enabled projectiles.
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    float ShrapnelParticlesMlt = 0.0f;

    // Factor used for armor penetration calculations before damage application in AOE attacks.
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    float ShrapnelPenMlt = 0.0f;

    // The angle cone of the flame weapon in degrees.
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    float FlameAngleMlt = 0.0f;

    // Used by flame and laser weapons for damage per burst (one full iteration).
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    float DamageTicksMlt = 0.0f;
};


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
        const FBehaviourWeaponMultipliers& GetBehaviourWeaponMultipliers() const;

protected:
        virtual void OnAdded(AActor* BehaviourOwner) override;
        virtual void OnRemoved(AActor* BehaviourOwner) override;
        virtual void OnStack(UBehaviour* StackedBehaviour) override;

        /** @brief Determine whether this behaviour should be applied to the provided weapon. */
        virtual bool CheckRequirement(UWeaponState* WeaponState) const;

        /** @brief Apply the behaviour to the target weapon. */
        virtual void ApplyBehaviourToWeapon(UWeaponState* WeaponState);

        /** @brief Remove the behaviour from the target weapon. */
        virtual void RemoveBehaviourFromWeapon(UWeaponState* WeaponState);

        /** @brief Hook executed when a new stack is added; override for stack-specific logic. */
        virtual void OnWeaponBehaviourStack(UWeaponState* WeaponState);

        UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Behaviour Attributes")
        FBehaviourWeaponAttributes BehaviourWeaponAttributes;
        UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Behaviour Attributes")
        FBehaviourWeaponMultipliers BehaviourWeaponMultipliers;

private:
        FBehaviourWeaponAttributes CalculateBehaviourAttributesWithMultipliers(UWeaponState* WeaponState) const;
        void CacheAppliedAttributes(UWeaponState* WeaponState, const FBehaviourWeaponAttributes& AppliedAttributes);
        bool TryGetCachedAttributes(UWeaponState* WeaponState, FBehaviourWeaponAttributes& OutCachedAttributes) const;
        void ClearCachedAttributesForWeapon(UWeaponState* WeaponState);

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

        UPROPERTY()
        TMap<TWeakObjectPtr<UWeaponState>, FBehaviourWeaponAttributes> M_AppliedAttributesPerWeapon;

        FTimerHandle M_PostBeginPlayTimerHandle;
        bool bM_HasInitializedPostBeginPlayLogic = false;
};
