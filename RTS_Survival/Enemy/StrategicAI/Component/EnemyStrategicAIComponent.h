// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Enemy/StrategicAI/Requests/StrategicAIRequests.h"
#include "RTS_Survival/Enemy/StrategicAI/StrategicAIBlackboard.h"
#include "EnemyStrategicAIComponent.generated.h"

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
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UEnemyStrategicAIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyStrategicAIComponent();

	void InitStrategicAIComponent(AEnemyController* EnemyController);

	void QueueFindClosestFlankableEnemyHeavyRequest(const FFindClosestFlankableEnemyHeavy& Request);
	void QueueGetPlayerUnitCountsAndBaseRequest(const FGetPlayerUnitCountsAndBase& Request);
	void QueueFindAlliedTanksToRetreatRequest(const FFindAlliedTanksToRetreat& Request);
	void QueueFindEnemyBaseClustersRequest(const FFindEnemyBaseClusters& Request);

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
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY()
	TWeakObjectPtr<AEnemyController> M_EnemyController = nullptr;

	FStrategicAIRequestBatch M_PendingRequests;
	FStrategicAIResultBatch M_LatestResults;
	FStrategicAIBlackboard M_StrategicAIBlackboard;

	FTimerHandle M_StrategicAIThinkingTimerHandle;

	void BeginPlay_PreThinKStep_InitThinkingTimers(const float Now);

	FAIThinkingTimerData M_AIBaseLocationThinkTimer;
	void AIBaseLocation_ThinkStep();

	FAIThinkingTimerData M_PlayerUnitCountsBuildingCountsThinkTimer;
	void PlayerUnitCountsBuildingCounts_ThinkStep();

	TArray<FAIThinkingTimerData*> M_AIThinkTimers;

	bool EnsureEnemyControllerIsValid() const;
	void CacheGenerationSeedFromGameInstance();
	int32 GetSeededIndex(const int32 OptionCount, const int32 DecisionSalt = 0) const;
	void StartStrategicAIThinkingTimer();
	void StopStrategicAIThinkingTimer();

	void StrategicAiThinkStep();
	void ProcessStrategicAIRequests();
	void OnStrategicAIResultsReceived(const FStrategicAIResultBatch& ResultBatch);
	void ProcessEnemyBaseClusterResults(const TArray<FResultEnemyBaseClusters>& EnemyBaseClusterResults);
	void ProcessAlliedTanksToRetreatResults(const TArray<FResultAlliedTanksToRetreat>& AlliedTanksToRetreatResults);
	bool GetIsValidEnemyDirectControlComponent(UEnemyDirectControlComponent* EnemyDirectControlComponent) const;
	void FillRetreatRequestExcludedUnits(FFindAlliedTanksToRetreat& RequestToFill) const;
	int32 M_CachedGenerationSeed = 0;
	mutable int32 M_SeedDecisionCounter = 0;
};
