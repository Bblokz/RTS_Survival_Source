// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Behaviour.h"
#include "BehaviourComp.generated.h"

class UBehaviour;

/**
 * @brief Component that owns and manages lifecycle of behaviours.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UBehaviourComp : public UActorComponent
{
GENERATED_BODY()

public:
UBehaviourComp();

/**
 * @brief Add a new behaviour instance of the provided class.
 * @param BehaviourClass Class to instantiate and add.
 */
void AddBehaviour(TSubclassOf<UBehaviour> BehaviourClass);

/**
 * @brief Remove a behaviour instance matching the provided class.
 * @param BehaviourClass Class used to find a behaviour to remove.
 */
void RemoveBehaviour(TSubclassOf<UBehaviour> BehaviourClass);

/**
 * @brief Swap an existing behaviour class for a new one.
 * @param BehaviourClassToReplace Class used to find the behaviour that will be removed.
 * @param BehaviourClassToAdd Class used to create the replacing behaviour.
 */
void SwapBehaviour(TSubclassOf<UBehaviour> BehaviourClassToReplace, TSubclassOf<UBehaviour> BehaviourClassToAdd);

/**
 * @brief Debug draw current behaviours above the owning actor.
 * @param DurationSeconds How long the debug string should persist.
 */
void DebugDrawBehaviours(const float DurationSeconds) const;

protected:
virtual void BeginPlay() override;
virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
/**
 * @brief Execute all pending behaviour operations after a tick loop.
 */
void ProcessPendingOperations();
/**
 * @brief Apply queued behaviour additions deferred during ticking.
 */
void ProcessPendingAdds();
/**
 * @brief Apply queued behaviour removals deferred during ticking.
 */
void ProcessPendingRemovals();
/**
 * @brief Apply queued behaviour swaps deferred during ticking.
 */
void ProcessPendingSwaps();
/**
 * @brief Advance and queue expiry for timed behaviours.
 * @param DeltaTime Time step for lifetime decrement.
 * @param Behaviour Behaviour being evaluated.
 * @param RemovalQueue Queue where expired behaviours are appended.
 */
void HandleTimedExpiry(const float DeltaTime, UBehaviour& Behaviour, TArray<TObjectPtr<UBehaviour>>& RemovalQueue) const;
/**
 * @brief Invoke tick on behaviours that opt into ticking.
 * @param DeltaTime Time step for tick execution.
 * @param Behaviour Behaviour being evaluated.
 */
void HandleBehaviourTick(const float DeltaTime, const UBehaviour& Behaviour) const;
/**
 * @brief Determine whether ticking should be enabled based on behaviours.
 * @return true when ticking is required; false otherwise.
 */
bool ShouldComponentTick() const;
/**
 * @brief Enable or disable ticking depending on current behaviour needs.
 */
void UpdateComponentTickEnabled();
/**
 * @brief Queue a behaviour class for addition after ticking finishes.
 * @param BehaviourClass Behaviour class to add.
 */
void QueueAddBehaviour(TSubclassOf<UBehaviour> BehaviourClass);
/**
 * @brief Queue a behaviour class for removal after ticking finishes.
 * @param BehaviourClass Behaviour class to remove.
 */
void QueueRemoveBehaviour(TSubclassOf<UBehaviour> BehaviourClass);
/**
 * @brief Queue a behaviour swap after ticking finishes.
 * @param BehaviourClassToReplace Existing behaviour class to remove.
 * @param BehaviourClassToAdd New behaviour class to add.
 */
void QueueSwapBehaviour(TSubclassOf<UBehaviour> BehaviourClassToReplace, TSubclassOf<UBehaviour> BehaviourClassToAdd);
/**
 * @brief Handle interactions with existing behaviours based on stack rules.
 * @param NewBehaviour Newly created behaviour being added.
 * @return true if handled without direct addition; false otherwise.
 */
bool TryHandleExistingBehaviour(UBehaviour* NewBehaviour);
/**
 * @brief Initialize and register a new behaviour instance.
 * @param NewBehaviour Behaviour to initialize.
 */
void AddInitialisedBehaviour(UBehaviour* NewBehaviour);
/**
 * @brief Verify whether another behaviour can be stacked.
 * @param MatchingBehaviours Current behaviours considered identical.
 * @param NewBehaviour Behaviour being added.
 * @return true if stacking is permitted; false otherwise.
 */
bool CanStackBehaviour(const TArray<TObjectPtr<UBehaviour>>& MatchingBehaviours, const UBehaviour* NewBehaviour) const;
/**
 * @brief Find behaviours matching the provided instance semantics.
 * @param NewBehaviour Behaviour used for comparison.
 * @return Array of matching behaviours.
 */
TArray<TObjectPtr<UBehaviour>> FindMatchingBehaviours(const UBehaviour* NewBehaviour) const;
/**
 * @brief Remove and destroy a behaviour instance.
 * @param BehaviourInstance Behaviour slated for removal.
 */
void RemoveBehaviourInstance(UBehaviour* BehaviourInstance);
/**
 * @brief Spawn a behaviour object with this component as outer.
 * @param BehaviourClass Class used to create the behaviour instance.
 * @return New behaviour instance or nullptr when creation failed.
 */
UBehaviour* CreateBehaviourInstance(TSubclassOf<UBehaviour> BehaviourClass) const;
/**
 * @brief Remove all behaviours during cleanup.
 */
void ClearAllBehaviours();

UPROPERTY()
TArray<TObjectPtr<UBehaviour>> M_Behaviours;

UPROPERTY()
TArray<TSubclassOf<UBehaviour>> M_PendingAdds;

UPROPERTY()
TArray<TSubclassOf<UBehaviour>> M_PendingRemovals;

UPROPERTY()
TArray<TSubclassOf<UBehaviour>> M_PendingSwapRemove;

UPROPERTY()
TArray<TSubclassOf<UBehaviour>> M_PendingSwapAdd;

bool bM_IsTickingBehaviours = false;
};
