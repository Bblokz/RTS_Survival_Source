// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Enemy/StrategicAI/Requests/StrategicAIRequests.h"
#include "RTS_Survival/Enemy/StrategicAI/StrategicAIBlackboard.h"
#include "EnemyStrategicAIComponent.generated.h"

class UStochasticDecisionTree;
struct FStochasticDecisionTree;
class AEnemyController;
class URTSGameInstance;
class UEnemyDirectControlComponent;

DECLARE_DELEGATE(FAIThinkStepDelegate);

USTRUCT()
struct FAIThinkingTimerData
{
	GENERATED_BODY()

	UPROPERTY()
	float LastTimeThought = 0.f;

	UPROPERTY()
	float ThinkingInterval = 1.f;

	FAIThinkStepDelegate ThinkStepDelegate;

	bool ShouldThink(const float CurrentTime) const
	{
		return (CurrentTime - LastTimeThought) >= ThinkingInterval;
	}

	bool TryExecuteThinkStep(const float CurrentTime)
	{
		if (not ShouldThink(CurrentTime))
		{
			return false;
		}

		if (not ThinkStepDelegate.IsBound())
		{
			return false;
		}

		ThinkStepDelegate.Execute();
		LastTimeThought = CurrentTime;
		return true;
	}
};

/**
 * @brief Orchestrates strategic AI request batching and async processing for the enemy controller.
 * There are a few components to this:
 * several _ThinkStep timers that execute once in every few think steps depending on their interval these execute automated
 * requests on the async processor for which the templates are editable through the AI controller.
 *
 * At the core is a decision tree that is configured per level in the enemy ai controller this one lives on the AEnemyController
 * which provides the pointer to this object at begin play.
 *
 * There is a blackboard that keeps track of various state of the game as well as of various results form the async processor
 * see FStrategicAIBlackboard. This board is provided to the decision tree as well.
 * 
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UEnemyStrategicAIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyStrategicAIComponent();

	// Called at begin play of enemy controller.
	void InitStrategicAIComponent(AEnemyController* EnemyController, UStochasticDecisionTree* StochasticDecisionTree);

	void QueueFindClosestFlankableEnemyHeavyRequest(const FFindClosestFlankableEnemyHeavy& Request);
	void QueueGetPlayerUnitCountsAndBaseRequest(const FGetPlayerUnitCountsAndBase& Request);
	void QueueFindAlliedTanksToRetreatRequest(const FFindAlliedTanksToRetreat& Request);
	void QueueFindEnemyBaseClustersRequest(const FFindEnemyBaseClusters& Request);
	void QueueFindLocationsUnderPlayerAttackRequest(const FFindLocationsUnderPlayerAttack& Request);
	void QueueFindPlayerUnitBulkLocationsRequest(const FFindPlayerUnitBulkLocations& Request);
	void QueueFindPlayerHeavyTankFlankLocationsRequest(const FFindClosestFlankableEnemyHeavy& Request);

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void RequestRetreatDamagedTanks(const FFindAlliedTanksToRetreat& Request);

	const FStrategicAIResultBatch& GetLatestStrategicAIResults() const;
	const FStrategicAIBlackboard& GetStrategicAIBlackboard() const;
	FStrategicAIBlackboard& GetEditableStrategicAIBlackboard();

	// This request is periodically used to find the bases owned by the AI.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FFindEnemyBaseClusters FindEnemyBase_TimerRequest;

	// This request is periodically used to find the counts of the player units and buildings.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGetPlayerUnitCountsAndBase FindPlayerCountBase_TimerRequest;

	// This request is periodically used to evaluate whether key locations are currently under player attack pressure.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FFindLocationsUnderPlayerAttack FindLocationsUnderAttack_TimerRequest;

	// This request is periodically used to find player squad and tank force concentrations.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FFindPlayerUnitBulkLocations FindPlayerUnitBulkLocations_TimerRequest;
	
	// This request is periodically used to find player heavy tanks to flank
	// there are several versions of this send to the async processor with different starting locations.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FFindClosestFlankableEnemyHeavy FindPlayerHeavyTankFlankLocations_TimerRequest;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY()
	TWeakObjectPtr<AEnemyController> M_EnemyController = nullptr;


	// Owned by the enemy controller for easy per-level adjustments in the instance.
	UPROPERTY()
	TWeakObjectPtr<UStochasticDecisionTree> M_StochasticDecisionTree;


	FStrategicAIRequestBatch M_PendingRequests;
	FStrategicAIResultBatch M_LatestResults;
	FStrategicAIBlackboard M_StrategicAIBlackboard;
	bool GetIsAllowedDirectControlUnits()const;

	FTimerHandle M_StrategicAIThinkingTimerHandle;

	void PreThinKStep_InitThinkingTimers(const float Now);

	FAIThinkingTimerData M_AIBaseLocationThinkTimer;
	void AIBaseLocation_ThinkStep();

	FAIThinkingTimerData M_PlayerUnitCountsBuildingCountsThinkTimer;
	void PlayerUnitCountsBuildingCounts_ThinkStep();

	FAIThinkingTimerData M_LocationsUnderAttackThinkTimer;
	void LocationsUnderAttack_ThinkStep();

	FAIThinkingTimerData M_PlayerUnitBulkLocationsThinkTimer;
	void PlayerUnitBulkLocations_ThinkStep();

	FAIThinkingTimerData M_PlayerHeavyTankFlankLocationsThinkTimer;
	void PlayerHeavyTankFlankLocations_ThinkStep();

	TArray<FAIThinkingTimerData*> M_AIThinkTimers;

	bool EnsureEnemyControllerIsValid() const;
	void CacheGenerationSeedFromGameInstance();
	int32 GetSeededIndex(const int32 OptionCount, const int32 DecisionSalt = 0) const;
	void StartStrategicAIThinkingTimer();
	void StopStrategicAIThinkingTimer();

	void StrategicAiThinkStep();
	void ProcessStrategicAIRequests();
	void OnStrategicAIResultsReceived(const FStrategicAIResultBatch& ResultBatch);
	void ProcessClosestFlankableEnemyHeavyResults(const TArray<FResultClosestFlankableEnemyHeavy>& ClosestFlankableEnemyHeavyResults);
	void ProcessEnemyBaseClusterResults(const TArray<FResultEnemyBaseClusters>& EnemyBaseClusterResults);
	void ProcessAlliedTanksToRetreatResults(const TArray<FResultAlliedTanksToRetreat>& AlliedTanksToRetreatResults);
	void ProcessPlayerUnitCountsAndBaseResults(const TArray<FResultPlayerUnitCounts>& PlayerUnitCountsAndBaseResults);
	void ProcessLocationsUnderPlayerAttackResults(
		const TArray<FResultLocationsUnderPlayerAttack>& LocationsUnderPlayerAttackResults);
	void ProcessPlayerUnitBulkLocationsResults(
		const TArray<FResultPlayerUnitBulkLocations>& PlayerUnitBulkLocationsResults);
	bool GetIsValidEnemyDirectControlComponent(UEnemyDirectControlComponent* EnemyDirectControlComponent) const;
	void FillRetreatRequestExcludedUnits(FFindAlliedTanksToRetreat& RequestToFill) const;
	int32 M_CachedGenerationSeed = 0;
	mutable int32 M_SeedDecisionCounter = 0;

	void ClearInvalidIdleUnitsFromBlackboard();

	// --------------------- Debugging ---------------------------
	void DebugBlackboardBasePoints() const;
	void DebugBlackboardLocationsUnderAttack() const;
	void DebugBlackboardBulkPlayerUnits()const;
	void DebugPoint(const FVector& Point, const float Radius,
	                const FColor& Color, const float Duration, const FString& Text)const;
	void DebugBlackboardUnitCounts() const;
};
