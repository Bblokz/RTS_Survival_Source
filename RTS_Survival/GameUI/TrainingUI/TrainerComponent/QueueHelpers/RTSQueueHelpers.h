#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainerComponent/TrainingQueueItem/QueueItem.h"

// Forward decls to avoid heavy includes
class UTrainerComponent;
class UTrainingMenuManager;
class UPlayerResourceManager;
class URTSRequirement;
class UObject;
class UWorld;
class ITrainer;

struct FTrainingOption;               

enum class ERTSResourceType : uint8;
enum class EStopTrainingQueueTimerReason : uint8;

namespace RTSQueueHelpers
{
	// --- Queue utilities ---
	void DrainQueueToArray(
		TQueue<FTrainingQueueItem, EQueueMode::Spsc>& Queue,
		TArray<FTrainingQueueItem>& OutItems);

	void RequeueFromArray(
		TQueue<FTrainingQueueItem, EQueueMode::Spsc>& Queue,
		const TArray<FTrainingQueueItem>& Items);

	// --- Requirement evaluation ---
	bool IsItemRequirementMetForQueueItem(
		const FTrainingQueueItem& Item,
		UTrainerComponent* Trainer); // uses RequirementSnapshot->CheckIsRequirementMet(trainer)

	// --- Partition (removal+refund+notify) ---
	// Returns whether any item was removed. Also reports if the active item was removed + its async state.
	bool PartitionItemsByRequirement(
		const TArray<FTrainingQueueItem>& InItems,
		UPlayerResourceManager* PlayerResourceManager,     // may be null
		TMap<FTrainingOption, int32>& InOutQueueCounts,    // decremented on removals
		const TScriptInterface<ITrainer>& OwningTrainer,   // may be null
		UTrainerComponent* Trainer,                         // for requirement check
		TArray<FTrainingQueueItem>& OutKept,
		bool& bOutRemovedActiveItem,
		EAsyncTrainingRequestState& OutRemovedActiveAsync);

	// --- Side effects when active item gets removed ---
	void HandleActiveItemRemoved(
		int32 KeptCount,
		EAsyncTrainingRequestState RemovedActiveAsyncState,
		TFunction<void(EStopTrainingQueueTimerReason)> StopQueueTimerFn, // binds to UTrainerComponent::StopQueueTimer
		TFunction<void(EAsyncTrainingRequestState)>     CancelAsyncFn,    // binds to UTrainerComponent::CancelAsyncLoadingRequest
		bool& bInOutAtInsufficientResources);

	// --- UI / timers ---
	void RestartTimerIfNeededAfterRemoval(
		bool bRemovedActiveItem,
		const TQueue<FTrainingQueueItem, EQueueMode::Spsc>& Queue,
		TFunction<void(bool /*bIsStartAfterPause*/)> StartQueueTimerFn); // binds to UTrainerComponent::StartQueueTimer

	void ReloadUIIfPrimarySelected(
		UTrainingMenuManager* TrainingMenuManager,
		UTrainerComponent* Trainer);

	// --- Refund & accounting helpers (pure or side-effect w/ PRM) ---
	TMap<ERTSResourceType, int32> ComputePaidCosts(const FTrainingQueueItem& Item);
	void RefundQueueItemProgress(const FTrainingQueueItem& Item, UPlayerResourceManager* PlayerResourceManager);
	void DecrementQueueCountFor(const FTrainingOption& ItemID, TMap<FTrainingOption, int32>& QueueCounts);
	void NotifyTrainerCancelled(const FTrainingOption& ItemID,
	                            const TScriptInterface<ITrainer>& OwningTrainer, UTrainerComponent* TrainerComp, const bool bCancelledActiveItem);
} // namespace RTSQueueHelpers