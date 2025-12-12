// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "TimerManager.h"
#include "SquadReinforcementComponent.generated.h"

class ASquadController;
class ASquadUnit;
class UPlayerResourceManager;
class UReinforcementPoint;
class UTrainingMenuManager;

USTRUCT()
struct FReinforcementRequestState
{
        GENERATED_BODY()

        UPROPERTY()
        bool bM_IsInProgress = false;

        UPROPERTY()
        FTimerHandle M_TimerHandle;

        UPROPERTY()
        FVector M_CachedLocation = FVector::ZeroVector;

        UPROPERTY()
        TArray<TSubclassOf<ASquadUnit>> M_PendingClasses;
};

/**
 * @brief Handles reinforcement ability activation and spawning missing squad units.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API USquadReinforcementComponent : public UActorComponent
{
        GENERATED_BODY()

public:
        USquadReinforcementComponent();

        /**
         * @brief Toggle the reinforcement ability on the owning squad controller.
         * @param bActivate True to add ability, false to remove.
         */
        void ActivateReinforcements(const bool bActivate);

        /**
         * @brief Reinforce missing units at the provided reinforcement point.
         * @param ReinforcementPoint Point that provides the spawn location.
         */
        void Reinforce(UReinforcementPoint* ReinforcementPoint);

protected:
        virtual void BeginPlay() override;

private:
        static constexpr int32 ReinforcementAbilityIndex = 11;

        void BeginPlay_InitSquadDataSnapshot();
        bool GetIsValidSquadController();
        bool GetIsValidRTSComponent();
        bool EnsureAbilityArraySized();
        void AddReinforcementAbility();
        void RemoveReinforcementAbility();
        bool GetIsReinforcementAllowed(UReinforcementPoint* ReinforcementPoint);
        bool TryResolveReinforcementCost(const int32 MissingUnits, TMap<ERTSResourceType, int32>& OutCost);
        bool TryGetResourceManager(UPlayerResourceManager*& OutManager) const;
        bool TryGetTrainingMenuManager(UTrainingMenuManager*& OutMenuManager) const;
        float CalculateReinforcementTime(const int32 MissingUnits) const;
        bool GetMissingUnitClasses(TArray<TSubclassOf<ASquadUnit>>& OutMissingUnitClasses) const;
        void ScheduleReinforcement(const float ReinforcementTime, const FVector& SpawnLocation,
                                   const TArray<TSubclassOf<ASquadUnit>>& MissingUnitClasses);
        void SpawnMissingUnits();
        void MoveSpawnedUnitsToController(const TArray<ASquadUnit*>& SpawnedUnits) const;

        bool bM_IsActivated;

        UPROPERTY()
        TWeakObjectPtr<ASquadController> M_SquadController;

        UPROPERTY()
        int32 M_MaxSquadUnits;

        UPROPERTY()
        TArray<TSubclassOf<ASquadUnit>> M_InitialSquadUnitClasses;

        UPROPERTY()
        FReinforcementRequestState M_ReinforcementRequestState;
};
