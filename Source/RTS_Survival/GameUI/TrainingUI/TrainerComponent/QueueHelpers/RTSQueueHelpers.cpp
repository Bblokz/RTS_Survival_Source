// Copyright ...

#include "RTSQueueHelpers.h"

#include "RTS_Survival/GameUI/TrainingUI/TrainingMenuManager.h"   // RequestReloadTrainingUI
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/Requirement/RTSRequirement.h"
#include "RTS_Survival/GameUI/TrainingUI/Interface/Trainer.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainerComponent/TrainerComponent.h"

namespace RTSQueueHelpers
{
	void DrainQueueToArray(
		TQueue<FTrainingQueueItem, EQueueMode::Spsc>& Queue,
		TArray<FTrainingQueueItem>& OutItems)
	{
		FTrainingQueueItem Tmp;
		while (Queue.Dequeue(Tmp))
		{
			OutItems.Add(Tmp);
		}
	}

	void RequeueFromArray(
		TQueue<FTrainingQueueItem, EQueueMode::Spsc>& Queue,
		const TArray<FTrainingQueueItem>& Items)
	{
		for (const FTrainingQueueItem& E : Items)
		{
			Queue.Enqueue(E);
		}
	}

	bool IsItemRequirementMetForQueueItem(
		const FTrainingQueueItem& Item,
		UTrainerComponent* Trainer)
	{
		if (!Trainer)
		{
			return true;
		}

		URTSRequirement* Snapshot = Trainer->ResolveRequirementSnapshot(Item.RequirementKey);
		if (!IsValid(Snapshot))
		{
			// No requirement → considered met
			return true;
		}

		constexpr bool bIsQueueItemCheck = true;
		return Snapshot->CheckIsRequirementMet(Trainer, bIsQueueItemCheck);
	}

	TMap<ERTSResourceType, int32> ComputePaidCosts(const FTrainingQueueItem& Item)
	{
		TMap<ERTSResourceType, int32> Paid;
		for (const auto& Pair : Item.TotalResourceCosts)
		{
			const ERTSResourceType Res = Pair.Key;
			const int32 Total = Pair.Value;
			const int32* RemainingPtr = Item.RemainingResourceCosts.Find(Res);
			const int32 Remaining = RemainingPtr ? *RemainingPtr : 0;
			const int32 AlreadyPaid = FMath::Max(Total - Remaining, 0);
			if (AlreadyPaid > 0)
			{
				Paid.Add(Res, AlreadyPaid);
			}
		}
		return Paid;
	}

	void RefundQueueItemProgress(const FTrainingQueueItem& Item, UPlayerResourceManager* PlayerResourceManager)
	{
		if (!PlayerResourceManager) return;
		const TMap<ERTSResourceType, int32> Paid = ComputePaidCosts(Item);
		if (Paid.Num() > 0)
		{
			PlayerResourceManager->RefundCosts(Paid);
		}
	}

	void DecrementQueueCountFor(const FTrainingOption& ItemID, TMap<FTrainingOption, int32>& QueueCounts)
	{
		if (int32* Cnt = QueueCounts.Find(ItemID))
		{
			*Cnt = FMath::Max(*Cnt - 1, 0);
		}
	}

	void NotifyTrainerCancelled(const FTrainingOption& ItemID, const TScriptInterface<ITrainer>& OwningTrainer,
	                            UTrainerComponent* TrainerComp, const bool bCancelledActiveItem)
	{
		if (OwningTrainer && TrainerComp)
		{
			TrainerComp->NotifyTrainerCancelled(OwningTrainer, ItemID, bCancelledActiveItem);
		}
	}

	bool PartitionItemsByRequirement(
		const TArray<FTrainingQueueItem>& InItems,
		UPlayerResourceManager* PlayerResourceManager,
		TMap<FTrainingOption, int32>& InOutQueueCounts,
		const TScriptInterface<ITrainer>& OwningTrainer,
		UTrainerComponent* Trainer,
		TArray<FTrainingQueueItem>& OutKept,
		bool& bOutRemovedActiveItem,
		EAsyncTrainingRequestState& OutRemovedActiveAsync)
	{
		bool bModified = false;
		bOutRemovedActiveItem = false;
		OutRemovedActiveAsync = EAsyncTrainingRequestState::Async_NoTrainingRequest;

		OutKept.Reserve(InItems.Num());

		for (int32 i = 0; i < InItems.Num(); ++i)
		{
			const FTrainingQueueItem& Each = InItems[i];

			const bool bMet = IsItemRequirementMetForQueueItem(Each, Trainer);
			if (bMet)
			{
				OutKept.Add(Each);
				continue;
			}
			// Track if we removed the head
			if (i == 0)
			{
				bOutRemovedActiveItem = true;
				OutRemovedActiveAsync = Each.AsyncRequestState;
			}
			

			bModified = true;

			// Accounting
			DecrementQueueCountFor(Each.ItemID, InOutQueueCounts);
			NotifyTrainerCancelled(Each.ItemID, OwningTrainer, Trainer, bOutRemovedActiveItem);
			RefundQueueItemProgress(Each, PlayerResourceManager);

			// Requirement snapshot cleanup
			if (Trainer)
			{
				Trainer->UnregisterRequirementSnapshot(Each.RequirementKey);
			}


		}
		return bModified;
	}


	void HandleActiveItemRemoved(
		int32 KeptCount,
		EAsyncTrainingRequestState RemovedActiveAsyncState,
		TFunction<void(EStopTrainingQueueTimerReason)> StopQueueTimerFn,
		TFunction<void(EAsyncTrainingRequestState)> CancelAsyncFn,
		bool& bInOutAtInsufficientResources)
	{
		// Stop/reset progress bar
		if (KeptCount == 0)
		{
			StopQueueTimerFn(EStopTrainingQueueTimerReason::QueueCompleted_StopProgressBar);
		}
		else
		{
			StopQueueTimerFn(EStopTrainingQueueTimerReason::QueuePaused_RemovedActive_ResetProgressBar_QueueNotEmpty);
		}

		// new head: recompute resource state
		bInOutAtInsufficientResources = false;

		// cancel in-flight async for that active item
		CancelAsyncFn(RemovedActiveAsyncState);
	}

	void RestartTimerIfNeededAfterRemoval(
		bool bRemovedActiveItem,
		const TQueue<FTrainingQueueItem, EQueueMode::Spsc>& Queue,
		TFunction<void(bool)> StartQueueTimerFn)
	{
		if (bRemovedActiveItem && !Queue.IsEmpty())
		{
			StartQueueTimerFn(true /*bIsStartAfterPause*/);
		}
	}

} // namespace RTSQueueHelpers
