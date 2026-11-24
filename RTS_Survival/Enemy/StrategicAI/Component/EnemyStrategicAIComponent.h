// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Enemy/StrategicAI/Requests/StrategicAIRequests.h"
#include "RTS_Survival/Enemy/StrategicAI/StrategicAIBlackboard.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Enemy/TrainingAndUnitCreation/StrategicTrainingState/StrategicTrainingState.h"
#include "RTS_Survival/Enemy/TrainingAndUnitCreation/EnemyBuildingTrainer/EnemyTrainedUnitStandSpots.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"
#include "EnemyStrategicAIComponent.generated.h"

class UGlobalAbility;
class UGlobalAbilitiesManager;
class UGameUnitManager;
class UStochasticDecisionTree;
class UStrategicAISubAction;
struct FStochasticDecisionTree;
class AEnemyController;
class URTSGameInstance;
class UEnemyDirectControlComponent;
class ASquadController;
class UEnemyBuildingTrainerComponent;

DECLARE_DELEGATE(FAIThinkStepDelegate);
class ICommands;

/** Where a unit a building trained came out; read when the unit gets its first orders (a squad once loaded). */
struct FEnemyBuildingTrainedUnitExit
{
	FTransform ExitTransform = FTransform::Identity;
	FVector BuildingCenter = FVector::ZeroVector;
};

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


USTRUCT()
struct FEnemyStrategicAITrainingBatch
{
	GENERATED_BODY()

	bool IsEmpty() const
	{
		return TankSubtypes.IsEmpty() && SquadSubtypes.IsEmpty();
	}

	void Reset()
	{
		TankSubtypes.Empty();
		SquadSubtypes.Empty();
		TrainingPointCost = 0;
	}

	UPROPERTY()
	TArray<ETankSubtype> TankSubtypes;

	UPROPERTY()
	TArray<ESquadSubtype> SquadSubtypes;

	UPROPERTY()
	int32 TrainingPointCost = 0;
};

USTRUCT()
struct FEnemyStrategicAITrainingReservation
{
	GENERATED_BODY()

	bool HasReservation() const
	{
		return bHasReservedTrainingBatch && not ReservedTrainingBatch.IsEmpty();
	}

	void ReserveBatch(const FEnemyStrategicAITrainingBatch& TrainingBatch)
	{
		ReservedTrainingBatch = TrainingBatch;
		bHasReservedTrainingBatch = not ReservedTrainingBatch.IsEmpty();
	}

	void Reset()
	{
		bHasReservedTrainingBatch = false;
		ReservedTrainingBatch.Reset();
	}

	// Keeps an unaffordable strategic choice intact so cheap options do not consume points meant for heavier units.
	UPROPERTY()
	bool bHasReservedTrainingBatch = false;

	UPROPERTY()
	FEnemyStrategicAITrainingBatch ReservedTrainingBatch;
};


USTRUCT()
struct FEnemyStrategicAITrainingSpawnBatch
{
	GENERATED_BODY()

	bool HasMoreTrainingOptions() const
	{
		return TrainingOptionsToSpawn.IsValidIndex(NextTrainingOptionIndex);
	}

	UPROPERTY()
	TArray<FTrainingOption> TrainingOptionsToSpawn;

	UPROPERTY()
	FVector SpawnLocation = FVector::ZeroVector;

	UPROPERTY()
	int32 NextTrainingOptionIndex = 0;

	UPROPERTY()
	int32 BatchID = INDEX_NONE;

	FTimerHandle TimerHandle;
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
 * @note ---- Regarding Idle enemy Units ----
 * @note 'Idle' meaning no AI task in this context units are stored on the blackboard and added by the direct control component.
 * @note they are either added by explicitly providing their ptr to direct control or by executing their icommands ability (no CommandCard)
 * @note that adds them.
 * @note ---- On Selecting Idle Units ----
 * @note each subaction has a set of requirements either designer added or native.
 * @note each requirement attributes to the selection rules if applicable, for example, the requirement have at least 6 light tanks
 * @note will add a rule that upon the sub action being executed the selected units will be filtered for 6 light tanks first.
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
	void StartStrategicAIThinkingTimer();

	// Swaps the tree after init (AEnemyController::OverrideStrategicDecisionTree); think timers are untouched.
	void SetStochasticDecisionTree(UStochasticDecisionTree* NewTree);

	void QueueFindClosestFlankableEnemyHeavyRequest(const FFindClosestFlankableEnemyHeavy& Request);
	void QueueGetPlayerUnitCountsAndBaseRequest(const FGetPlayerUnitCountsAndBase& Request);
	void QueueFindAlliedTanksToRetreatRequest(const FFindAlliedTanksToRetreat& Request);
	void QueueFindEnemyBaseClustersRequest(const FFindEnemyBaseClusters& Request);
	void QueueFindLocationsUnderPlayerAttackRequest(const FFindLocationsUnderPlayerAttack& Request);
	void QueueFindPlayerUnitBulkLocationsRequest(const FFindPlayerUnitBulkLocations& Request);
	void QueueFindConstructionLocationsRequest(const FFindConstructionLocations& Request);
	void QueueFindMineLocationsRequest(const FFindMineLocations& Request);
	void QueueFindPlayerHeavyTankFlankLocationsRequest(const FFindClosestFlankableEnemyHeavy& Request);

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void RequestRetreatDamagedTanks(const FFindAlliedTanksToRetreat& Request);

	const FStrategicAIResultBatch& GetLatestStrategicAIResults() const;
	const FStrategicAIBlackboard& GetStrategicAIBlackboard() const;

	// Fires on the game thread after a result batch of the async thread was applied to the blackboard (tests).
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnStrategicAIResultsApplied, const FStrategicAIResultBatch&);
	FOnStrategicAIResultsApplied OnStrategicAIResultsApplied;

	// Also gates the base trenches the AI digs (UEnemyBaseTrenchComponent).
	bool GetIsAllowedBuilding() const;
	FStrategicAIBlackboard& GetEditableStrategicAIBlackboard();
	FEnemyStrategicTrainingState& GetEditableEnemyTrainingState();
	void RegisterDroppedUnitWithBlackboardWhenReady(AActor* DroppedUnit);

	/** Points of a unit a building trainer dropped before it trained go back to the AI. */
	void RefundTrainingPoints(const int32 TrainingPoints);
	/**
	 * @brief Training points earned outside the per-minute income, e.g. a fuel truck unloaded at one of this AI's
	 * fuel depots (UFuelDeliveryDepotComponent).
	 * @param TrainingPoints Added to the pool the AI spends on training; zero or less does nothing.
	 */
	void AddBonusTrainingPoints(const int32 TrainingPoints);
	/**
	 * @brief Building points for the AI's base building (plan: Docs/EnemyAI/EnemyBaseBuilding_Plan.md section 3).
	 * Only the pool exists so far: supply deliveries (ASovietAirSupplyPad) fill it and nothing spends it yet.
	 * @param BuildingPoints Added to the pool; zero or less does nothing.
	 */
	void AddBuildingPoints(const int32 BuildingPoints);
	int32 GetBuildingPoints() const { return M_BuildingPoints; }
	/**
	 * @brief A building trainer of this AI's player finished a unit: it joins the fog hidden until seen, joins this
	 * controller's direct-control pool (blackboard idle) and moves to a base defense position, like every trained
	 * unit; without a defense position it moves to a stand spot near the exit that blocks no unit of this side.
	 * @param Trainer The building's trainer for its exit; null when the building is gone (the unit then waits
	 * around TrainingLocation).
	 */
	void OnBuildingTrainerUnitSpawned(const FTrainingOption& TrainingOption, AActor* SpawnedActor,
	                                  const FVector& TrainingLocation,
	                                  const UEnemyBuildingTrainerComponent* Trainer = nullptr);

	// A unit trained at one of this AI's buildings got its first orders (stand spot or defense position). A mission
	// can take it over: deregister it from the direct-control pool, then give its own orders.
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnBuildingTrainedUnitReady, AActor* /*Unit*/);
	FOnBuildingTrainedUnitReady OnBuildingTrainedUnitReady;

	/** @return Units trained at this AI's buildings that are still alive (squads with a living man). */
	int32 GetAliveBuildingTrainedUnitCount();

	/** @return True while FEnemyAIMissionSettings::MaxAliveBuildingTrainedUnits is set and reached. */
	bool GetIsBuildingTrainedUnitCapReached();

#if RTS_WITH_MAP_TESTS
	// Routes the units through the building trainers like a paid batch (no points are spent); returns how many
	// were queued at a building.
	int32 MapTest_QueueTrainingAtBuildings(const TArray<ETankSubtype>& TankSubtypes,
	                                       const TArray<ESquadSubtype>& SquadSubtypes);
	TArray<UEnemyBuildingTrainerComponent*> MapTest_GetBuildingTrainers();
	int32 MapTest_GetTrainingPoints() const { return M_TrainingState.TrainingPoints; }
	void MapTest_SetTrainingPoints(const int32 TrainingPoints) { M_TrainingState.TrainingPoints = TrainingPoints; }
	// Runs one real training think (reservation, pick, pay, route) even while the periodic one is suspended.
	void MapTest_RunTrainingThinkStep()
	{
		bM_MapTest_IsForcingTrainingThink = true;
		Training_ThinkStep();
		bM_MapTest_IsForcingTrainingThink = false;
	}
	// Keeps the live AI from queueing its own units while a test owns the buildings' queues.
	void MapTest_SetTrainingThinkSuspended(const bool bSuspended) { bM_MapTest_IsTrainingThinkSuspended = bSuspended; }
	int32 MapTest_GetRefundedTrainingPoints() const { return M_MapTest_RefundedTrainingPoints; }
	// Runs the pick filter alone: what the AI may pick now that it knows its standing trainers.
	void MapTest_CollectAvailableTrainingOptions(TArray<ETankSubtype>& OutTankSubtypes,
	                                             TArray<ESquadSubtype>& OutSquadSubtypes);
	FEnemyStrategicTrainingState& MapTest_GetEditableTrainingState() { return M_TrainingState; }
#endif

#if defined(RTS_WITH_ENEMY_AI_SHIPPING_TESTS) && RTS_WITH_ENEMY_AI_SHIPPING_TESTS
	void ShippingTest_RunTrainingPressureThinkStep();
	void ShippingTest_AccumulateTrainingPointIncomeForElapsedSeconds(const float ElapsedSeconds);
	void ShippingTest_EnablePressureOnlyTrainingMode()
	{
		bM_ShippingTest_PressureOnlyTraining = true;
	}

	int32 ShippingTest_GetTrainingSpawnRequestCount() const
	{
		return M_ShippingTest_TrainingSpawnRequestCount;
	}

	int32 ShippingTest_GetTrainingSpawnSuccessCount() const
	{
		return M_ShippingTest_TrainingSpawnSuccessCount;
	}

	int32 ShippingTest_GetActiveTrainingSpawnBatchCount() const
	{
		return M_ActiveTrainingSpawnBatches.Num();
	}
#endif

	// This request is periodically used to find the bases owned by the AI.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FFindEnemyBaseClusters FindEnemyBase_TimerRequest;

	// This request is periodically used to find the counts of the player units and buildings.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGetPlayerUnitCountsAndBase FindPlayerCountBase_TimerRequest;

	// This request is periodically used to evaluate whether key locations are currently under player attack pressure.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FFindLocationsUnderPlayerAttack FindLocationsUnderAttack_TimerRequest;

	// Periodically evaluates the AI's own base points for player pressure so damaged vehicles avoid bases in combat.
	// LocationsToEvaluate and RequestID are overwritten with the current base points on each think step.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FFindLocationsUnderPlayerAttack FindBasesInCombat_TimerRequest;

	// This request is periodically used to find player squad and tank force concentrations.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FFindPlayerUnitBulkLocations FindPlayerUnitBulkLocations_TimerRequest;

	// This request is periodically used to build construction locations from base defense points.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FFindConstructionLocations FindConstructionLocations_TimerRequest;

	// This request periodically finds road-bound and farther outer-arc mine locations.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FFindMineLocations FindMineLocations_TimerRequest;
	
	// This request is periodically used to find combat-active player heavy tanks to flank.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FFindClosestFlankableEnemyHeavy FindPlayerHeavyTankFlankLocations_TimerRequest;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	void BeginPlay_CacheUnitManager();

	UPROPERTY()
	TWeakObjectPtr<UGameUnitManager> M_GameUnitManager = nullptr;
	bool EnsureIsValidUnitManager();
	UPROPERTY()
	TWeakObjectPtr<AEnemyController> M_EnemyController = nullptr;


	// Owned by the enemy controller for easy per-level adjustments in the instance.
	UPROPERTY()
	TWeakObjectPtr<UStochasticDecisionTree> M_StochasticDecisionTree;


	FStrategicAIRequestBatch M_PendingRequests;
	FStrategicAIResultBatch M_LatestResults;
	FStrategicAIBlackboard M_Blackboard;
	// The starting amount of training points is set when the enemy ai controller propagates the settings.
	// See void AEnemyController::BeginPlay_MoveAISettingsToStrategicAIBlackboard() const
	FEnemyStrategicTrainingState M_TrainingState;

	// See AddBuildingPoints: filled by supply deliveries, not spent yet.
	int32 M_BuildingPoints = 0;
	// Keeps an unaffordable picked batch active until enough training points have accumulated.
	FEnemyStrategicAITrainingReservation M_TrainingReservation;
	// Each paid batch owns its own timer so overlapping training batches do not interrupt each other.
	// Only the legacy path uses these: enemy UTrainerComponents on the blackboard (no Soviet building has one).
	TArray<FEnemyStrategicAITrainingSpawnBatch> M_ActiveTrainingSpawnBatches;
	// Standing buildings of this AI's player with a UEnemyBuildingTrainerComponent; refreshed every training think.
	TArray<TWeakObjectPtr<UEnemyBuildingTrainerComponent>> M_BuildingTrainers;
	// Building-trained units that have not had their first orders yet (squads wait until they are loaded).
	TMap<TWeakObjectPtr<AActor>, FEnemyBuildingTrainedUnitExit> M_BuildingTrainedUnitExits;
	// Stand spots building-trained units were sent to; one stays taken while its unit walks there or stands on it.
	TArray<FEnemyStandSpotReservation> M_StandSpotReservations;
	/** Stand spot by the exit (then a defense position later), or straight to a defense position without one. */
	void IssueFirstOrdersToBuildingTrainedUnit(AActor* SpawnedActor, ICommands& Commands,
	                                           const FEnemyBuildingTrainedUnitExit& BuildingExit);
	/** Sends a building-trained unit to a stand spot near its exit (see EnemyTrainedUnitStandSpots); false if none. */
	bool Training_MoveToStandSpot(AActor* Unit, ICommands& Commands, const FEnemyBuildingTrainedUnitExit& Exit);
	// Building-trained units waiting at their stand spot before they go to a base defense position.
	TArray<FEnemyStandSpotWait> M_StandSpotWaits;
	// Every unit trained at this AI's buildings; weak, they die. Counted for MaxAliveBuildingTrainedUnits.
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> M_BuildingTrainedUnits;
	/** Sends every waiting unit whose time is up, and that is still idle in the AI's pool, to a defense position. */
	void Training_ReleaseStandSpotWaiters();
	/** Moves the unit to a random base defense position; false if the AI has none. */
	bool MoveUnitToRandomDefensePosition(AActor* Unit, ICommands& Commands, const TCHAR* LogTag,
	                                     const FVector& LogFromLocation);
	/** The faction's units and reserved spots around an exit, without the unit itself (or its own soldiers). */
	void Training_GatherStandSpotOccupied(const AActor* Unit, const FVector& ExitLocation,
	                                      TArray<FEnemyStandSpotOccupied>& OutOccupied);
	/** The unit and its soldiers join the fog now, hidden until the human player can see them. */
	static void Training_StartFogNowHiddenUntilSeen(AActor* Unit);
#if RTS_WITH_MAP_TESTS
	bool bM_MapTest_IsTrainingThinkSuspended = false;
	bool bM_MapTest_IsForcingTrainingThink = false;
	int32 M_MapTest_RefundedTrainingPoints = 0;
#endif
	int32 M_NextTrainingSpawnBatchID = 1;
#if defined(RTS_WITH_ENEMY_AI_SHIPPING_TESTS) && RTS_WITH_ENEMY_AI_SHIPPING_TESTS
	int32 M_ShippingTest_TrainingSpawnRequestCount = 0;
	int32 M_ShippingTest_TrainingSpawnSuccessCount = 0;
	bool bM_ShippingTest_PressureOnlyTraining = false;
#endif
	bool GetIsAllowedDirectControlUnits()const;
	bool GetIsAllowedUnitTraining()const;

	FTimerHandle M_StrategicAIThinkingTimerHandle;
	FTimerHandle M_StrategicAIStartDelayTimerHandle;

	void PreThinKStep_InitThinkingTimers(const float Now);
	void StartStrategicAIThinkingTimerAfterDelay(const float DelaySeconds);

	FAIThinkingTimerData M_AIBaseLocationThinkTimer;
	void AIBaseLocation_ThinkStep();
	void AddPlayerUnitLocationsForDefensePositionsOfEnemyBases(FFindEnemyBaseClusters& OutFindBaseRequest) ;

	FAIThinkingTimerData M_PlayerUnitCountsBuildingCountsThinkTimer;
	void PlayerUnitCountsBuildingCounts_ThinkStep();

	FAIThinkingTimerData M_LocationsUnderAttackThinkTimer;
	void LocationsUnderAttack_ThinkStep();

	FAIThinkingTimerData M_BasesInCombatThinkTimer;
	void BasesInCombat_ThinkStep();

	FAIThinkingTimerData M_PlayerUnitBulkLocationsThinkTimer;
	void PlayerUnitBulkLocations_ThinkStep();

	FAIThinkingTimerData M_ConstructionLocationsThinkTimer;
	void ConstructionLocations_ThinkStep();
	void FillConstructionLocationsTimerRequest(FFindConstructionLocations& RequestToFill) const;

	FAIThinkingTimerData M_MineLocationsThinkTimer;
	void MineLocations_ThinkStep();
	void FillMineLocationsTimerRequest(FFindMineLocations& RequestToFill) const;
	void FillMineBaseLocationPayload(FFindMineLocations& RequestToFill) const;
	void FillMinePlayerBulkLocationPayload(FFindMineLocations& RequestToFill) const;
	void FillMineRoadSplineSamplePayload(FFindMineLocations& RequestToFill) const;

	FAIThinkingTimerData M_PlayerHeavyTankFlankLocationsThinkTimer;
	void PlayerHeavyTankFlankLocations_ThinkStep();
	void RemoveExpiredHeavyTankFlankingResults(const float CurrentTimeSeconds);

	
/**
-AI chooses a batch.
-AI cannot afford it.
-AI stores it as M_TrainingReservation.
-Every later Training_ThinkStep() tries to pay for that exact batch first.
-If still too expensive, the reservation blocks new random training choices.
-Training points accumulate elsewhere over time.
-Eventually the AI has enough points.
-Training_TrySpendTrainingPoints() succeeds.
-The reserved batch is created.
-M_TrainingReservation.Reset() clears the reservation.
-The next training think step can evaluate fresh pressure again.
 */
	void Training_ThinkStep();
	bool Training_HandleReservedTrainingBatchIfAny();
	/**
	 * @brief Builds the currently unlocked candidate pool before pressure filtering so reserved batches only use legal tech.
	 * @param OutTankSubtypes Filled with unlocked tank options from every available tech level.
	 * @param OutSquadSubtypes Filled with unlocked squad options from every available tech level.
	 */
	void Training_CollectAvailableTrainingOptions(
		TArray<ETankSubtype>& OutTankSubtypes,
		TArray<ESquadSubtype>& OutSquadSubtypes) const;
	/**
	 * @brief Adds one tech-level option set only when its cached unlock state says it can be trained.
	 * @param TrainingOptions Mission-authored options for a single tech level.
	 * @param OutTankSubtypes Accumulates unlocked tank options.
	 * @param OutSquadSubtypes Accumulates unlocked squad options.
	 */
	void Training_AddUnlockedTrainingOptions(
		const FEnemyTrainingOptionsForTechLevel& TrainingOptions,
		TArray<ETankSubtype>& OutTankSubtypes,
		TArray<ESquadSubtype>& OutSquadSubtypes) const;

	/**
	 * @brief A mission can keep the AI to a few units (FEnemyAIMissionSettings::RestrictTrainingTo*), such as Mosin
	 * squads only in an early mission; drops every unlocked option outside those lists.
	 * @param InOutTankSubtypes Unlocked tank options, filtered in place.
	 * @param InOutSquadSubtypes Unlocked squad options, filtered in place.
	 */
	void Training_ApplyMissionTrainingRestriction(
		TArray<ETankSubtype>& InOutTankSubtypes,
		TArray<ESquadSubtype>& InOutSquadSubtypes) const;
	/**
	 * @brief Narrows unlocked options to what can satisfy the broad pressure buckets before random batch selection.
	 * @param Selection Focus and specialty picked from accumulated training pressure.
	 * @param TankSubtypes Unfiltered unlocked tank options.
	 * @param SquadSubtypes Unfiltered unlocked squad options.
	 * @param OutTankSubtypes Filled with tanks matching the selection.
	 * @param OutSquadSubtypes Filled with squads matching the selection.
	 */
	void Training_FilterTrainingOptionsForSelection(
		const FEnemyStrategicTrainingSelection& Selection,
		const TArray<ETankSubtype>& TankSubtypes,
		const TArray<ESquadSubtype>& SquadSubtypes,
		TArray<ETankSubtype>& OutTankSubtypes,
		TArray<ESquadSubtype>& OutSquadSubtypes) const;
	bool Training_GetDoesTankFitSelection(
		const ETankSubtype TankSubtype,
		const FEnemyStrategicTrainingSelection& Selection) const;
	bool Training_GetDoesSquadFitSelection(
		const ESquadSubtype SquadSubtype,
		const FEnemyStrategicTrainingSelection& Selection) const;
	/**
	 * @brief Randomly samples with replacement so a reservation can represent multiple copies of one useful option.
	 * @param TankSubtypes Filtered tank options.
	 * @param SquadSubtypes Filtered squad options.
	 * @return Batch with selected options and their total training-point cost.
	 */
	FEnemyStrategicAITrainingBatch Training_BuildRandomTrainingBatch(
		const TArray<ETankSubtype>& TankSubtypes,
		const TArray<ESquadSubtype>& SquadSubtypes) const;
	/**
	 * @brief Sums helper-defined costs so point reservation uses the same prices as immediate training.
	 * @param TankSubtypes Tanks selected for the batch.
	 * @param SquadSubtypes Squads selected for the batch.
	 * @return Total training points needed for the batch.
	 */
	int32 Training_GetTrainingBatchCost(
		const TArray<ETankSubtype>& TankSubtypes,
		const TArray<ESquadSubtype>& SquadSubtypes) const;
	bool Training_TrySpendTrainingPoints(const int32 TrainingPointCost);
	// Collects the trainer components of the buildings this AI's player owns from the unit manager.
	void Training_RefreshBuildingTrainers();
	// A standing building trainer (or a legacy enemy UTrainerComponent) can take this unit type now.
	bool Training_GetCanTrainUnitType(const EAllUnitType UnitType) const;
	bool Training_GetHasLegacyEnemyTrainer() const;
	// Every unit of the batch has a building that can train it.
	bool Training_GetCanTrainBatch(const FEnemyStrategicAITrainingBatch& TrainingBatch) const;
	// The least busy standing building that trains this option (equally busy ones take turns); null when none can.
	UEnemyBuildingTrainerComponent* Training_PickBuildingTrainer(const FTrainingOption& TrainingOption) const;
	int32 Training_GetTrainingOptionCost(const FTrainingOption& TrainingOption) const;
	/**
	 * @brief Hands each option to the least busy building that trains it; the building trains it over time.
	 * @param TrainingOptions Paid options.
	 * @param OutOptionsWithoutBuilding Options no standing building could take.
	 * @return How many options were queued at a building.
	 */
	int32 Training_QueueOptionsAtBuildingTrainers(const TArray<FTrainingOption>& TrainingOptions,
	                                              TArray<FTrainingOption>& OutOptionsWithoutBuilding);
	/**
	 * @brief Trains a paid batch: each unit goes to the least busy standing building of its kind (barracks/tent for
	 * squads, vehicle buildings for tanks) and appears there after its training time. Units no building can take get
	 * their points back; a level with legacy enemy UTrainerComponents still spawns those there.
	 * @param TankSubtypes Tank subtypes to create.
	 * @param SquadSubtypes Squad subtypes to create.
	 */
	void CreateTrainingBatch(
		const TArray<ETankSubtype>& TankSubtypes,
		const TArray<ESquadSubtype>& SquadSubtypes);
	/**
	 * @brief Converts paid subtype picks into async-spawner options while dropping invalid placeholder subtypes.
	 * @param TankSubtypes Tank subtypes selected by strategic training.
	 * @param SquadSubtypes Squad subtypes selected by strategic training.
	 * @param OutTrainingOptions Filled in spawn order for a single batch timer.
	 */
	void CreateTrainingOptionsToSpawn(
		const TArray<ETankSubtype>& TankSubtypes,
		const TArray<ESquadSubtype>& SquadSubtypes,
		TArray<FTrainingOption>& OutTrainingOptions) const;
	/**
	 * @brief Projects the trainer/base location before spawning so async units start on navigable ground.
	 * @param TrainingLocation Desired spawn location from trainer or base fallback.
	 * @param OutProjectedTrainingLocation Filled with the navmesh location when projection succeeds.
	 * @return True when the location can be used for spawning.
	 */
	bool TryProjectTrainingLocationToNavmesh(
		const FVector& TrainingLocation,
		FVector& OutProjectedTrainingLocation) const;
	/**
	 * @brief Creates an independent timer for one paid batch so other batches can overlap safely.
	 * @param TrainingOptionsToSpawn Options this timer will spawn one-by-one.
	 * @param SpawnLocation Projected spawn location shared by the batch.
	 */
	void StartTrainingSpawnBatchTimer(
		const TArray<FTrainingOption>& TrainingOptionsToSpawn,
		const FVector& SpawnLocation);
	void TrainingSpawnBatchTimerTick(const int32 BatchID);
	void StopTrainingSpawnBatchTimer(const int32 BatchID);
	void OnBlackboardUnitSpawned(
		const FTrainingOption& TrainingOption,
		AActor* SpawnedActor,
		const int32 ID,
		const FVector& TrainingLocation);
	void OnBlackboardSquadFullyLoaded(ASquadController* SquadController);
	void IssueOrdersToSpawnedBlackboardUnit(AActor* SpawnedActor);
	void OnDroppedSquadFullyLoaded(ASquadController* SquadController);
	void RegisterDroppedUnitWithBlackboard(AActor* DroppedUnit);
	bool TryGetRandomBlackboardDefensePosition(FDefensePositions& OutDefensePosition) const;
	void DebugTrainedUnit(const FTrainingOption& TrainingOption, const FVector& TrainingLocation) const;

	bool GetValidTrainerComponentLocationFromBlackboard(FTransform& OutSpawnTransform) const;
	

	FAIThinkingTimerData M_TrainingPressureThinkTimer;
	/**
	 * @brief Updates pressure on its own cadence so unit production demand is not tied to order execution frequency.
	 */
	void TrainingPressure_ThinkStep();
	/**
	 * @brief Converts one strategic sub-action into pressure without allowing target/time-gated actions to bias training.
	 * @param SubAction Strategic option that can emit missing-unit or general-purpose training pressure.
	 * @param GameTimeSeconds Current world time used to age sub-action pressure.
	 */
	void AccumulateTrainingPressureFromSubAction(
		const UStrategicAISubAction* SubAction,
		const float GameTimeSeconds);
	
	FAIThinkingTimerData M_TrainingRequirementsThinkTimer;
	// Updates the requirements mapping in the training state depending on the building expansions the AI owns.
	void TrainingRequirements_ThinkStep();

	FAIThinkingTimerData M_TrainingPointsThinkTimer;
	void GetTrainingPoints_ThinkStep();
	void AccumulateTrainingPointIncomeForElapsedSeconds(const float ElapsedSeconds);

	TArray<FAIThinkingTimerData*> M_AIThinkTimers;

	bool EnsureEnemyControllerIsValid() const;
	// The AI player of the owning controller: whose units this component counts, trains and asks the async
	// thread about. The first AI player when the component has no enemy controller as owner.
	int32 GetControlledPlayer() const;
	bool EnsureStochasticDecisionTreeIsValid() const;
	void CacheGenerationSeedFromGameInstance();
	int32 GetSeededIndex(const int32 OptionCount, const int32 DecisionSalt = 0) const;
	void StopStrategicAIThinkingTimer();

	FAIThinkingTimerData M_EnemyGlobalAbilityThinkTimer;
	float M_EnemyGlobalAbilityAIStartTimeSeconds = 0.0f;
	float GetEnemyGlobalAbilityThinkStepInterval() const;
	void EnemyGlobalAbility_ThinkStep();
	bool IsEnemyReinforcementAbility(EGlobalAbility AbilityType) const;
	bool IsEnemyBarrageAbility(EGlobalAbility AbilityType) const;
	bool IsEnemyLightMediumAirStrikeAbility(EGlobalAbility AbilityType) const;
	bool IsEnemyHeavyCarpetBombingAbility(EGlobalAbility AbilityType) const;
	bool GetCanUseReinforcementAbilities() const;
	bool GetCanUseBarrages() const;
	bool GetCanUseLightMediumAirStrikes() const;
	bool GetCanUseHeavyCarpetBombing() const;
	TArray<FVector> GetReinforcementAbilityTargetLocations() const;
	TArray<FVector> GetBarrageTargetLocations() const;
	TArray<FVector> GetLightMediumAirStrikeTargetLocations() const;
	TArray<FVector> GetHeavyCarpetBombingTargetLocations() const;
	bool TryPickEnemyGlobalAbilityAndTarget(UGlobalAbility*& OutAbility, FVector& OutTargetLocation) const;
	void AddAbilityBucketOptions(const TArray<UGlobalAbility*>& AvailableAbilities, TArray<UGlobalAbility*>& OutBucket, bool (UEnemyStrategicAIComponent::*Predicate)(EGlobalAbility) const) const;

	void StrategicAiThinkingLoop();
	void ProcessStrategicAIRequests();
	void OnStrategicAIResultsReceived(const FStrategicAIResultBatch& ResultBatch);
	void ProcessClosestFlankableEnemyHeavyResults(const TArray<FResultClosestFlankableEnemyHeavy>& ClosestFlankableEnemyHeavyResults);
	void ProcessEnemyBaseClusterResults(const TArray<FResultEnemyBaseClusters>& EnemyBaseClusterResults);
	void ProcessAlliedTanksToRetreatResults(const TArray<FResultAlliedTanksToRetreat>& AlliedTanksToRetreatResults);
	void ProcessPlayerUnitCountsAndBaseResults(const TArray<FResultPlayerUnitCounts>& PlayerUnitCountsAndBaseResults);
	void ProcessLocationsUnderPlayerAttackResults(
		const TArray<FResultLocationsUnderPlayerAttack>& LocationsUnderPlayerAttackResults);
	// Both the designer under-attack request and the base-combat request share one result array; split by RequestID.
	bool GetIsBasesInCombatResult(const FResultLocationsUnderPlayerAttack& Result) const;
	void ProcessPlayerUnitBulkLocationsResults(
		const TArray<FResultPlayerUnitBulkLocations>& PlayerUnitBulkLocationsResults);
	void ProcessConstructionLocationsResults(
		const TArray<FResultConstructionLocations>& ConstructionLocationsResults);
	void ProcessMineLocationsResults(const TArray<FResultMineLocations>& MineLocationsResults);
	bool GetIsValidEnemyDirectControlComponent(UEnemyDirectControlComponent* EnemyDirectControlComponent) const;
	void FillRetreatRequestExcludedUnits(FFindAlliedTanksToRetreat& RequestToFill) const;
	int32 M_CachedGenerationSeed = 0;
	mutable int32 M_SeedDecisionCounter = 0;

	void ClearInvalidIdleUnitsFromBlackboard();

	// --------------------- Debugging ---------------------------
	void DebugBlackboardBasePoints() const;
	void DebugBlackboardLocationsUnderAttack() const;
	void DebugBlackboardBulkPlayerUnits()const;
	void DebugConstructionLocations() const;
	void DebugMineLocations() const;
	void DebugPoint(const FVector& Point, const float Radius,
	                const FColor& Color, const float Duration, const FString& Text)const;
	void DebugBlackboardUnitCounts() const;

	void DebugFlankingPositiions()const;
};
