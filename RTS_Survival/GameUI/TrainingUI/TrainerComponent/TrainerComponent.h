// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Containers/Queue.h"
#include "TrainingQueueItem/QueueItem.h"

#include "TrainerComponent.generated.h"


class USelectionComponent;
class ARTSRallyPointActor;
enum class ERTSResourceType : uint8;
class UPlayerResourceManager;
class UTimeProgressBarWidget;
class UProgressBar;
class ITrainer;
class ARTSAsyncSpawner;
class ACPPController;
class UTrainingMenuManager;

USTRUCT(Blueprintable)
struct FTrainerSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float TimeNotSelectable = 2.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	UMeshComponent* TrainingMesh = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FName SpawnSocketName = "None";

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FName DefaultWayPointSocketName = "None";

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	bool StartEnabled = false;
};

USTRUCT()
struct FTrainingEnabled
{
	GENERATED_BODY()
	bool bIsAbleToTrain = true;
	bool bIsQueueOnPause = false;
};

UENUM()
enum class EStopTrainingQueueTimerReason : uint8
{
	QueueCompleted_StopProgressBar,
	QueuePaused_PauseProgressBar,
	QueuePaused_RemovedActive_ResetProgressBar_QueueNotEmpty,
};

/**
 * Contains a queue that can dynamically be filled with training options.
 *
 * @note ------- On Spawning the Training Option -------
 * @note 1) The timed function UpdateQueue calls DetermineSpawnActiveItem(ActiveItem) which starts async spawning the unit
 * if the time remaining is low enough.
 * @note 2) The async spawner adds the trainer & option to the queue.
 * @note 3) Async loading completes and calls HandleAsyncTrainingLoadComplete which will attempt to spawn the asset.
 * in here, we also spawn the controller if the asset is of the pawn class.
 * @note 4) OnTrainingOption spawned is called next which calls the OnRTSUnitSpawned with a disable flag on the spawned asset.
 * @note 5) We call OnOption spawned in this component which will teleport the actor to the correct location.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent), BlueprintType)
class RTS_SURVIVAL_API UTrainerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UTrainerComponent();

	bool GetUseTrainingPreview() const { return bM_UseTrainingPreview; }

	/**
	 *  @brief IF there are any queued items go through them and check if their requirements are still met. If a requirement
	 *  fails the item will be removed. IF this check changed the queue the timer will be restarted if needed and the UI will
	 *  be updated.
	 *  @note IF nothing is queued but we are primary selected; then it may be possible that the UI shows items of which
	 *  the requirements are no longer valid; hence we request a UI refresh.
	 */
	void CheckRequirementsAndRefreshUI();

	/** Register a requirement snapshot; returns the unique key (or INDEX_NONE if Snapshot is null). */
	int32 RegisterRequirementSnapshot(URTSRequirement* Snapshot);

	/** Resolve a previously registered requirement snapshot; returns nullptr if not found/invalid. */
	URTSRequirement* ResolveRequirementSnapshot(const int32 RequirementKey) const;

	/** Remove a snapshot from the live map (no-op for INDEX_NONE or missing key). */
	void UnregisterRequirementSnapshot(const int32 RequirementKey);

	/** @return Able to train if the queue is not paused. */
	inline bool GetIsAbleToTrain() const { return M_TrainingEnabled.bIsAbleToTrain; }

	/** @return True if the queue is paused. */
	inline bool GetIsPaused() const;
	inline bool GetIsQueueEmpty() const;

	// Note if you use training preview ensure that the building mesh on the nomadic vehicle has a socket named TrainingPreview.
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="ReferenceCasts")
	void InitUTrainerComponent(
		TArray<FTrainingOption> Options,
		TScriptInterface<ITrainer> Trainer,
		FTrainerSettings TrainerSettings,
		UTimeProgressBarWidget* ProgressBar, TSubclassOf<ARTSRallyPointActor> RallyPointActorClass, const bool
		bUseTrainingPreview);


	/**
	 * @brief Sets to primary selected which allows for updating the UI.
	 * @param IsPrimarySelected Whether this trainer is the primary selected one.
	 * @param TrainingMenuManager The manager that handles the training menu.
	 */
	void SetIsPrimarySelectedTraining(
		bool IsPrimarySelected,
		UTrainingMenuManager* TrainingMenuManager);

	/**
	 * @brief Adds a training item to the queue.
	 * @param TrainingItem The item to add to the training queue.
	 * @return true if this item is the first item in the queue.
	 * @post If this is the first item in the queue, the queue update timer is started inside this component.
	 */
	bool AddToQueue(const FTrainingQueueItem& TrainingItem);

	bool RemoveLastInstanceOfTypeFromQueue(const FTrainingOption& TrainingID);

	/** @return A map with for each training option of this trainer the count within the queue.
	 * e.g. how often that option is enqueued.
	 * @param OutActiveTrainingOptionIndex The index of the training option that is currently active.
	 * @param OutTimeRemaining The time remaining for the active training option.
	 * @param OutbIsPausedByResourceInsufficiency
	 */
	TMap<FTrainingOption, int32> GetCurrentTrainingQueueState(
		int32& OutActiveTrainingOptionIndex,
		int32& OutTimeRemaining, bool& OutbIsPausedByResourceInsufficiency) const;

	/**
	 * @brief Returns the training options available for this trainer.
	 * @return Array of training options.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	TArray<FTrainingOption> GetTrainingOptions() const;

	/** @return The spawner associated */
	ARTSAsyncSpawner* GetAsyncSpawner() const;

	/**
	 * Saves the spawned actor for when training is complete.
	 * @param SpawnedActor The spawned actor.
	 * @param Option The training option that was spawned.
	 * @note Will set the M_WaitForLoadingAsset to false.
	 */
	void OnOptionSpawned(AActor* SpawnedActor, const FTrainingOption Option);

	// Will hide training ui and pause the queue.
	void SetTrainingEnabled(const bool bIsAbleToTrain);

	/**
	 * @brief Pauses or resumes the training queue.
	 * @param bPause If true, pauses the queue. If false, resumes the queue.
	 */
	void SetPauseQueue(const bool bPause);
	void DebugPrintQueueLayout();

	// DO NOT CALL ONLY USED BY RTSQUEUEHELPERS.
	void NotifyTrainerCancelled(const TScriptInterface<ITrainer>& OwningTrainer,
	                            const FTrainingOption& CancelledOption, const bool bCancelledActiveItem);


	/**
	 * @brief Move the rally point actor to a new world-space location (ignores collisions).
	 * @param NewWorldLocation Desired target location.
	 */
	void MoveRallyPointActor(const FVector NewWorldLocation);

protected:
	virtual void BeginPlay() override;

private:
	// List of all training options this trainer has, set with InitUTrainerComponent which takes into account the max size
	// supported by the UI.
	TArray<FTrainingOption> M_TTrainingOptions;

	// The Training queue of this trainer; is loaded into main menu when primary selected.
	TQueue<FTrainingQueueItem, EQueueMode::Spsc> M_TTrainingQueue;

	// Reference to part of the UI that manages training, is null if this trainer is not the primary selected one.
	UPROPERTY()
	UTrainingMenuManager* M_TrainingMenuManager;

	// Rally point actor class to instantiate (from Init).
	UPROPERTY()
	TSubclassOf<ARTSRallyPointActor> M_RallyPointActorClass;

	// Spawned rally point actor (hidden unless trainer’s selection bucket is active).
	UPROPERTY()
	TWeakObjectPtr<ARTSRallyPointActor> M_RallyPointActor;

	// Cached pointer to owning selection component for delegate subscription.
	UPROPERTY()
	TWeakObjectPtr<USelectionComponent> M_OwnerSelectionComponent;
	// ---------------- Rally point helpers ----------------

	/** @brief Spawn the rally point actor at default waypoint (hidden), ignore collisions. */
	void SpawnRallyPointActorIfNeeded(TSubclassOf<ARTSRallyPointActor> RallyClass);

	/** @brief BeginPlay hook: subscribe to SelectionComponent's PrimarySameType delegate safely. */
	void BeginPlay_AddToSelectionPrimaryType();

	/** @brief Delegate handler: show/hide rally point when the owning unit enters/leaves PrimarySameType. */
	void OnSelectedAsSameTypePrimary(bool bIsInPrimarySameType);

	/** @brief Validity helpers with error reporting. */
	bool GetIsValidRallyPointActor() const;
	bool GetIsValidSelectionComponent() const;

	void OnSpawnedActorCheckForSquadController(AActor* SpawnedActor, const FVector& SpawnLocation) const;

	/** @brief Compute the desired rally point location (actor if valid; otherwise fallback default). */
	FVector GetDesiredRallyPointLocation() const;
	/**
	 * @return Whether the Training Menu Manager is valid and hence this component is primary selected.
	 * @note If Fails no error is reported.
	 */
	bool GetIsPrimarySelected() const;

	/**
	 * @return Whether the Training Menu Manager is valid and hence this component is primary selected.
	 * @note No Failure allowed; will report error.
	 */
	bool EnsureIsPrimarySelected() const;

	UPROPERTY()
	FTimerHandle M_QueueHandle;

	// Keeps track how often each option occurs in the training queue.
	UPROPERTY()
	TMap<FTrainingOption, int32> M_TQueueCounts;

	void StartQueueTimer(const bool bIsStartAfterPause);
	void StopQueueTimer(const EStopTrainingQueueTimerReason Reason);

	void UpdateQueue();

	/**
	 * @brief Determines the tick cost for this tick = 1 second of the queue item using the remaining cost of it.
	 * @param ActiveQueueItem The active item that needs its tick to be payed for.
	 * @return True if the cost was calculated and payed for, false otherwise.
	 * @post The remaining cost on the item is reduced by the calculated tick cost if we could pay for everything.
	 */
	bool UpdateQueue_PauseQueueInsufficientResources(
		FTrainingQueueItem* ActiveQueueItem) const;
	/**
	 * @param ActiveQueueItem The active item that needs its tick to be payed for.
	 * @param OutCalculatedTickCost The cost we calculated of the item for this tick. 
	 * @return Whether for the provided item the player can pay for one tick = 1 second
	 * as calcualted by the total cost and total trainig time of the item.
	 * True if the player can pay. False otherwise.
	 */
	bool GetCanPayForItemTick(const FTrainingQueueItem* const ActiveQueueItem,
	                          TMap<ERTSResourceType, int32>& OutCalculatedTickCost) const;

	/**
	 * @param ActiveQueueItem The queue item that needs its remaining costs reduced by the calcualted amount.
	 * @param TickCost The cost we calculated of the item for this tick.
	 * @return Whether the player resource manager could successfully pay.
	 * @pre The TickCost is calculated.
	 * @post The player has resources subtracted.
	 */
	bool PayForItemTick(FTrainingQueueItem* const ActiveQueueItem, const TMap<ERTSResourceType, int32>& TickCost) const;

	bool bM_AtInsufficientResources = false;

	bool EnsureActiveItemIsValid(const FTrainingQueueItem* const ActiveQueueItem) const;


	/**
	 * @brief Final function before queue item is removed.
	 * Removes the first item from the queue and updates the UI.
	 */
	void TrainingCompleteRemoveFromQueue();

	/**
	 * @brief Called by TrainingCompleteRemoveFromQueue, provides the spawned actor to the trainer.
	 * @param FinishedItem The item that was finished.
	 */
	void PropagateSpawnedActorToTrainer(FTrainingQueueItem FinishedItem);

	TWeakObjectPtr<ACPPController> M_PlayerController;
	bool GetIsValidPlayerController() const;

	TWeakInterfacePtr<ITrainer> M_OwningTrainer;
	bool GetIsValidOwningTrainer() const;

	inline void DetermineSpawnActiveItem(FTrainingQueueItem* ActiveItem);

	UPROPERTY()
	AActor* M_SpawnedActor;

	void MakeActorReadyForSpawn(AActor* ActorToMakeReady) const;

	/**
	 * @brief If the M_TrainingMenuManager is set we are primary selected and need to update the UI.
	 * @note Will provide a queue item with default values if the next item is null.
	 */
	void RefreshTrainingMenuIfPrimary(
	) const;

	bool RemoveLastQueueItemOfOption(
		const FTrainingOption& TrainingOption,
		bool& bOutRemovedActiveItem,
		EAsyncTrainingRequestState& OutRequestStateOfRemovedActiveItem,
		FTrainingQueueItem& OutRemovedItem);

	// Set to true if the item in the queue is finished but waiting for async asset to be loaded.
	bool M_WaitForLoadingAsset = false;

	// Destroys the spawned actor and reset the pointer.
	void DestroySpawnedActor();

	void CancelAsyncLoadingRequest(EAsyncTrainingRequestState State);

	UPROPERTY()
	FTrainerSettings M_TrainerSettings;

	bool GetSpawnTransform(FTransform& OutTransform) const;

	FVector GetDefaultWayPoint() const;

	// Contains if the queue is on pause and whether the trainer is able to train.
	FTrainingEnabled M_TrainingEnabled;


	void BeginPlay_InitPlayerResourceManager();
	void BeginPlay_InitPlayerController();


	/** @brief
	 * Generates a point that is navigatable on the nav mesh, preferabily close to the desired point.
	 * @param DesiredPoint The point to spawn close to.
	 */
	FVector GetValidSpawnPoint(const FVector& DesiredPoint) const;

	// The progressbar that can be used by the training component.
	TWeakObjectPtr<UTimeProgressBarWidget> M_OwnerProgressBar;

	bool GetIsValidProgressionBar() const;


	TWeakObjectPtr<UPlayerResourceManager> M_PlayerResourceManager;
	bool GetIsValidPlayerResourceManager() const;

	/**
	 * @brief Updates the UI clock if primary selected to reflect the resource insufficiency state.
	 * Also updates the ProgressionBar.
	 * @param bAtInsufficientResources Whether the queue is paused or not by resource insufficiencies
	 * @param ActiveItem The currently active item that is being trained.
	 * @pre IMPORTANT: IF the queue jumped from paused by resources to un-paused by resources then make sure that
	 * the remaining time for this first un-paused-tick is already updated to prevent sync errors.
	 */
	void UpdateUIQueueForInsufficientResourceState(const bool bAtInsufficientResources,
	                                               const FTrainingQueueItem& ActiveItem);

	void DebugTrainingComp(const FString& Message, const FColor& Color = FColor::Blue) const;

	void RefundManualRemoval(const FTrainingQueueItem& RemovedItem) const;

	// ----------------------------- Requirement Logic --------------------------
	void InitTrainerComp_StartRequirementCheckTimer();
	FTimerHandle M_RequirementCheckHandle;

	/** Remove all items in the queue whose keyed requirement snapshot is no longer met. Refund paid progress.
	 * @return Whether we performed a UI refresh request ( to update the UI If this trainer is the primary selected one).
	 * @note No UI refresh request is performed if the queue was not modified; returning false in that case.
	 */
	bool RemoveInvalidRequirementItems_AndRefund();

	/** GC-visible store of requirement snapshots, indexed by RequirementKey. */
	UPROPERTY()
	TMap<int32, TObjectPtr<URTSRequirement>> M_LiveRequirementSnapshots;

	/** Monotonic key generator for requirement snapshots. */
	UPROPERTY()
	int32 M_NextRequirementKey = 1;

	UPROPERTY()
	bool bM_UseTrainingPreview = false;
};
