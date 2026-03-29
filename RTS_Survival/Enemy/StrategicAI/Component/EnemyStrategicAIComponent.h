// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Enemy/StrategicAI/Requests/StrategicAIRequests.h"
#include "EnemyStrategicAIComponent.generated.h"

class AEnemyController;
class URTSGameInstance;

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

	const FStrategicAIResultBatch& GetLatestStrategicAIResults() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY()
	TWeakObjectPtr<AEnemyController> M_EnemyController = nullptr;

	FStrategicAIRequestBatch M_PendingRequests;
	FStrategicAIResultBatch M_LatestResults;

	FTimerHandle M_StrategicAIThinkingTimerHandle;

	bool EnsureEnemyControllerIsValid() const;
	void CacheGenerationSeedFromGameInstance();
	int32 GetSeededIndex(const int32 OptionCount, const int32 DecisionSalt = 0) const;
	void StartStrategicAIThinkingTimer();
	void StopStrategicAIThinkingTimer();

	void ProcessStrategicAIRequests();
	void OnStrategicAIResultsReceived(const FStrategicAIResultBatch& ResultBatch);
	int32 M_CachedGenerationSeed = 0;
	mutable int32 M_SeedDecisionCounter = 0;
};
