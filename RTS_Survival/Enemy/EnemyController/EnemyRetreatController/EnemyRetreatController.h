// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnemyRetreatData.h"
#include "EnemyRetreatController.generated.h"

class AEnemyController;
class ICommands;

#if !UE_BUILD_SHIPPING
// Test hook: every unit of a retreat reached its retreat location, after this many seconds.
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnRetreatAllUnitsReachedForTest, int32 /*RetreatID*/, float /*SecondsTaken*/);
#endif
class URTSGameInstance;

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

	// True while the unit belongs to a retreat that has not turned into its counterattack yet.
	bool GetIsUnitRetreating(const AActor* UnitActor) const;

#if !UE_BUILD_SHIPPING
	FOnRetreatAllUnitsReachedForTest OnRetreatAllUnitsReachedForTest;
#endif

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TWeakObjectPtr<AEnemyController> M_EnemyController = nullptr;

	// Tracks active retreat waves and their timers for counterattacks.
	TArray<FEnemyRetreatData> M_ActiveRetreats;

	FTimerHandle M_RetreatCheckTimerHandle;
	int32 M_LastRetreatID = -1;
	int32 M_CachedGenerationSeed = 0;
	mutable int32 M_SeedDecisionCounter = 0;

	bool EnsureEnemyControllerIsValid() const;
	void CacheGenerationSeedFromGameInstance();
	int32 GetSeededIndex(const int32 OptionCount, const int32 DecisionSalt = 0) const;

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
