// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Behaviours/Behaviour.h"

#include "BehaviourSquadMovement.generated.h"

class ASquadController;
class ASquadUnit;
class UCharacterMovementComponent;


USTRUCT(BlueprintType)
struct FBehaviourSquadMovementMultipliers
{
        GENERATED_BODY()

        UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Movement|Multipliers")
        float MaxWalkSpeedMultiplier = 1.0f;

        UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Movement|Multipliers")
        float MaxAccelerationMultiplier = 1.0f;

        UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Movement|Multipliers")
        float BrakingDecelerationWalkingMultiplier = 1.0f;

        UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Movement|Multipliers")
        float GroundFrictionMultiplier = 1.0f;

        UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Movement|Multipliers")
        float BrakingFrictionFactorMultiplier = 1.0f;

        UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Movement|Multipliers")
        float RotationRateYawMultiplier = 1.0f;
};

USTRUCT()
struct FBehaviourSquadMovementSettingsCache
{
        GENERATED_BODY()

        UPROPERTY()
        float BaseMaxWalkSpeed = 0.0f;

        UPROPERTY()
        float BaseMaxAcceleration = 0.0f;

        UPROPERTY()
        float BaseBrakingDecelerationWalking = 0.0f;

        UPROPERTY()
        float BaseGroundFriction = 0.0f;

        UPROPERTY()
        float BaseBrakingFrictionFactor = 0.0f;

        UPROPERTY()
        FRotator BaseRotationRate = FRotator::ZeroRotator;

        UPROPERTY()
        FBehaviourSquadMovementMultipliers CurrentAppliedMultiplier;

        bool bHasCachedValues = false;
};

/**
 * @brief Behaviour that adjusts squad movement parameters after units are loaded.
 * @note Uses CharacterMovementComponent multipliers so designers can tune relative to existing data.
 */
UCLASS()
class RTS_SURVIVAL_API UBehaviourSquadMovement : public UBehaviour
{
        GENERATED_BODY()

public:
        UBehaviourSquadMovement();

protected:
        virtual void OnAdded() override;
        virtual void OnRemoved() override;
        virtual void OnStack(UBehaviour* StackedBehaviour) override;

private:
        void SetupInitializationForOwner();
        void RegisterSquadFullyLoadedCallback(ASquadController* SquadController);
        void HandleSquadDataLoaded();
        void ApplyBehaviourToSquad();
        void ApplyBehaviourToSquadUnit(ASquadUnit* SquadUnit);
        void RemoveBehaviourFromSquadUnits();
        /**
         * @brief Cache original movement values before applying multipliers.
         * @param MovementComponent Character movement component that will be modified.
         * @param Cache Storage used to preserve and accumulate movement values.
         */
        void CacheOriginalSettings(UCharacterMovementComponent* MovementComponent,
                                   FBehaviourSquadMovementSettingsCache& Cache) const;
        /**
         * @brief Apply configured multipliers to a movement component using cached values.
         * @param MovementComponent Character movement component receiving the changes.
         * @param Cache Previously cached defaults and accumulated multipliers for this unit.
         */
        void ApplyMovementToMovementComponent(UCharacterMovementComponent* MovementComponent,
                                              FBehaviourSquadMovementSettingsCache& Cache) const;
        void RestoreMovementForUnit(ASquadUnit* SquadUnit);
        void ClearTrackedUnits();
        bool TryGetSquadUnits(TArray<ASquadUnit*>& OutSquadUnits);
        bool TryGetSquadController(ASquadController*& OutSquadController);
        bool GetIsValidSquadController() const;

        UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Behaviour|Movement", meta = (AllowPrivateAccess = "true"))
        FBehaviourSquadMovementMultipliers BehaviourMovementMultipliers;

        UPROPERTY()
        TWeakObjectPtr<ASquadController> M_SquadController;

        UPROPERTY()
        TArray<TWeakObjectPtr<ASquadUnit>> M_AffectedSquadUnits;

        UPROPERTY()
        TMap<TWeakObjectPtr<ASquadUnit>, FBehaviourSquadMovementSettingsCache> M_MovementCachePerSquadUnit;

        bool bM_HasRegisteredSquadLoadedCallback = false;
        bool bM_HasAppliedMovementAtLeastOnce = false;
        bool bM_IsBehaviourActive = false;
};
