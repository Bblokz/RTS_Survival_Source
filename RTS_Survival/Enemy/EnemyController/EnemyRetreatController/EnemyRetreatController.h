// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnemyRetreatData.h"
#include "EnemyRetreatController.generated.h"

class AEnemyController;
class ICommands;

/**
 * @brief Tracks retreating units so the enemy controller can regroup or clean them up safely.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UEnemyRetreatController : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyRetreatController();

	void InitRetreatController(AEnemyController* EnemyController);

	/**
	 * @brief Keeps retreat state centralized so counterattacks happen consistently.
	 * @param RetreatingSquadControllers Squads that are retreating using the retreat command.
	 * @param ReverseRetreatUnits Units that should reverse-move to their retreat location.
	 * @param CounterattackLocation Target location used for the follow-up attack.
	 * @param PostRetreatCounterStrategy Strategy that determines what to do after retreat.
	 * @param TimeTillCounterAttackAfterLastRetreatingUnitReached Delay after the last unit reaches retreat.
	 * @param MaxTimeWaitTillCounterAttack Hard timeout before triggering a counterattack.
	 */
	void StartRetreat(
		const TArray<FRetreatElement>& RetreatingSquadControllers,
		const TArray<FRetreatElement>& ReverseRetreatUnits,
		const FVector& CounterattackLocation,
		const EPostRetreatCounterStrategy PostRetreatCounterStrategy,
		const float TimeTillCounterAttackAfterLastRetreatingUnitReached,
		const float MaxTimeWaitTillCounterAttack);

private:
	TWeakObjectPtr<AEnemyController> M_EnemyController = nullptr;

	// Tracks active retreat waves and their timers for counterattacks.
	TArray<FEnemyRetreatData> M_ActiveRetreats;

	FTimerHandle M_RetreatCheckTimerHandle;
	int32 M_LastRetreatID = -1;

	bool EnsureEnemyControllerIsValid() const;

	void StartRetreatCheckTimer();
	void StopRetreatCheckTimerIfIdle();

	void CheckRetreatingUnits();

	void CleanupInvalidRetreatUnits(FEnemyRetreatData& RetreatData) const;
	void ClearRetreatTimers(FEnemyRetreatData& RetreatData) const;
	void DestroyRetreatUnit(ICommands* Commands) const;
	void DestroyRetreatElements(const TArray<FRetreatElement>& RetreatElements) const;
	bool GetHasReachedRetreatLocation(const ICommands* Commands, const FVector& RetreatLocation) const;
	bool TryReissueRetreatOrder(ICommands* Commands, const FVector& RetreatLocation, const bool bUseReverseMove) const;
	void DrawDebugTextAtLocation(const FString& Message, const FVector& Location, const FColor& Color) const;
	void ProcessRetreatElementsForCheck(
		FEnemyRetreatData& RetreatData,
		TArray<FRetreatElement>& RetreatElements,
		const bool bUseReverseMove,
		bool& bAllUnitsReached);

	void OnCounterAttackTimerExpired(const int32 RetreatID, const FString& TriggerReason);
	void TriggerCounterAttack(FEnemyRetreatData& RetreatData, const FString& TriggerReason);
	FVector GetAverageLocationFromRetreatData(const FEnemyRetreatData& RetreatData) const;

	int32 GenerateRetreatID();
};
