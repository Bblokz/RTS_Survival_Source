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
	int32 M_CachedGenerationSeed = 0;
	mutable int32 M_SeedDecisionCounter = 0;
};
