// Copyright (C) Bas Blokzijl - All rights reserved.
#include "TrainerComponent.h"

#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"
#include "RallyPointActor/RallyPoint.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingMenuManager.h"
#include "RTS_Survival/GameUI/TrainingUI/Interface/Trainer.h"
#include "RTS_Survival/Requirement/RTSRequirement.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Interfaces/RTSInterface/RTSUnit.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/RTSComponents/TimeProgressBarWidget.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainerComponent/QueueHelpers/RTSQueueHelpers.h"
#include "RTS_Survival/RTSComponents/SelectionComponent.h"

UTrainerComponent::UTrainerComponent()
	: M_TTrainingOptions{},
	  M_TrainingMenuManager(nullptr),
	  M_SpawnedActor(nullptr)
{
	bM_AtInsufficientResources = false;
	M_NextRequirementKey = 1;
}

void UTrainerComponent::CheckRequirementsAndRefreshUI()
{
	// If the queue is empty we do not check the requirements for the queued items but we do need
	// to refresh the UI in case one of the requirements no fails and the UI contains old state with items
	// that are no longer unlocked.
	if (M_TTrainingQueue.IsEmpty())
	{
		RefreshTrainingMenuIfPrimary();
		return;
	}

	const bool bDidRequestRefreshUI = RemoveInvalidRequirementItems_AndRefund();
	if (not bDidRequestRefreshUI)
	{
		RefreshTrainingMenuIfPrimary();
	}
}

int32 UTrainerComponent::RegisterRequirementSnapshot(URTSRequirement* Snapshot)
{
	if (!IsValid(Snapshot))
	{
		return INDEX_NONE;
	}

	// Ensure uniqueness per registration.
	const int32 NewKey = M_NextRequirementKey++;
	M_LiveRequirementSnapshots.Add(NewKey, Snapshot);
	return NewKey;
}

URTSRequirement* UTrainerComponent::ResolveRequirementSnapshot(const int32 RequirementKey) const
{
	if (RequirementKey == INDEX_NONE)
	{
		return nullptr;
	}
	if (const TObjectPtr<URTSRequirement>* Found = M_LiveRequirementSnapshots.Find(RequirementKey))
	{
		return Found->Get();
	}
	return nullptr;
}

void UTrainerComponent::UnregisterRequirementSnapshot(const int32 RequirementKey)
{
	if (RequirementKey == INDEX_NONE)
	{
		return;
	}
	M_LiveRequirementSnapshots.Remove(RequirementKey);
}


bool UTrainerComponent::GetIsPaused() const
{
	const FString StringPause = M_TrainingEnabled.bIsQueueOnPause ? "true" : "false";;
	DebugTrainingComp("Is queue paused: " + StringPause);
	return M_TrainingEnabled.bIsQueueOnPause;
}

bool UTrainerComponent::GetIsQueueEmpty() const
{
	return M_TTrainingQueue.IsEmpty();
}

void UTrainerComponent::InitUTrainerComponent(
	TArray<FTrainingOption> Options,
	TScriptInterface<ITrainer> Trainer,
	FTrainerSettings TrainerSettings, UTimeProgressBarWidget* ProgressBar, TSubclassOf<ARTSRallyPointActor>
	RallyPointActorClass, const bool bUseTrainingPreview)
{
	bM_UseTrainingPreview = bUseTrainingPreview;
	for (int32 i = 0; i < Options.Num(); i++)
	{
		if (i < DeveloperSettings::GamePlay::Training::MaxTrainingOptions)
		{
			M_TTrainingOptions.Add(Options[i]);
		}
		else
		{
			RTSFunctionLibrary::ReportError("Too many options provided for trainer component, will not fit UI."
				+ FString::FromInt(Options.Num()) + " options provided."
				+ " Max options: " + FString::FromInt(
					DeveloperSettings::GamePlay::Training::MaxTrainingOptions) +
				"Owner name: " + GetOwner()->GetName());
			break;
		}
	}
	// Initialize counts for all training options
	for (auto Option : M_TTrainingOptions)
	{
		M_TQueueCounts.Add(Option, 0);
	}
	if (Trainer)
	{
		M_OwningTrainer = Trainer;
	}
	else
	{
		RTSFunctionLibrary::ReportError("Trainer is not valid when initializing trainer component."
			"See function UTrainerComponent::InitUTrainerComponent");
	}
	if (!IsValid(TrainerSettings.TrainingMesh))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this,
		                                             "TrainingMesh on TrainerSettings struct",
		                                             "UTrainerComponent::InitUTrainerComponent");
	}
	M_RallyPointActorClass = RallyPointActorClass;
	M_TrainerSettings = TrainerSettings;
	M_OwnerProgressBar = ProgressBar;
	SetTrainingEnabled(TrainerSettings.StartEnabled);
	InitTrainerComp_StartRequirementCheckTimer();
	// Store and spawn the rally point actor (hidden), ignoring collisions.
}


void UTrainerComponent::SetIsPrimarySelectedTraining(
	const bool IsPrimarySelected,
	UTrainingMenuManager* TrainingMenuManager)
{
	if (IsPrimarySelected)
	{
		if (IsValid(TrainingMenuManager))
		{
			M_TrainingMenuManager = TrainingMenuManager;
		}
		else
		{
			RTSFunctionLibrary::ReportError("TrainingMenuManager is not valid when attempting to set"
				"Trainer component as primary selected: " + GetOwner()->GetName() +
				"\n The trainer component cannot update the UI now."
				"See function UTrainerComponent::SetIsPrimarySelectedTraining");
		}
	}
	else
	{
		M_TrainingMenuManager = nullptr;
	}
}

bool UTrainerComponent::AddToQueue(const FTrainingQueueItem& TrainingItem)
{
	if (not M_TrainingEnabled.bIsAbleToTrain || not M_OwningTrainer.IsValid())
	{
		RTSFunctionLibrary::ReportError("cannot remove from queue as training queue is disabled or owning trainer is"
			"not valid!"
			+ GetName());
		return false;
	}
	if (not M_TQueueCounts.Contains(TrainingItem.ItemID))
	{
		RTSFunctionLibrary::ReportError("Clicked on item for which there is no space in the queue."
			"\n in function UTrainerComponent::AddToQueue"
			"\n ItemID: " + TrainingItem.ItemID.GetTrainingName());
		return false;
	}
	const bool bIsFirstItem = M_TTrainingQueue.IsEmpty();
	M_TTrainingQueue.Enqueue(TrainingItem);
	M_OwningTrainer->OnTrainingItemAddedToQueue(TrainingItem.ItemID);
	// Count this item in the queue.
	M_TQueueCounts[TrainingItem.ItemID]++;
	RefreshTrainingMenuIfPrimary();

	// Start the queue timer if this is the first item in the queue.
	if (bIsFirstItem)
	{
		StartQueueTimer(false);
		// this is the first item.
		return true;
	}
	return false;
}

bool UTrainerComponent::RemoveLastQueueItemOfOption(
	const FTrainingOption& TrainingOption,
	bool& bOutRemovedActiveItem,
	EAsyncTrainingRequestState& OutRequestStateOfRemovedActiveItem,
	FTrainingQueueItem& OutRemovedItem)
{
	// Drain into an array.
	TArray<FTrainingQueueItem> Items;
	FTrainingQueueItem Item;
	while (M_TTrainingQueue.Dequeue(Item))
	{
		Items.Add(Item);
	}
	if (Items.Num() == 0) return false;

	// Find last match
	int32 LastMatchIdx = INDEX_NONE;
	for (int32 i = Items.Num() - 1; i >= 0; --i)
	{
		if (Items[i].ItemID == TrainingOption)
		{
			LastMatchIdx = i;
			break;
		}
	}
	if (LastMatchIdx == INDEX_NONE)
	{
		for (auto& E : Items) M_TTrainingQueue.Enqueue(E);
		return false;
	}

	// Was this removed item the Active item?
	bOutRemovedActiveItem = (LastMatchIdx == 0);
	if (bOutRemovedActiveItem)
	{
		OutRequestStateOfRemovedActiveItem = Items[0].AsyncRequestState;
	}

	// Capture removed item (for refund) and re-enqueue others
	OutRemovedItem = Items[LastMatchIdx];
	for (int32 i = 0; i < Items.Num(); ++i)
	{
		if (i == LastMatchIdx) continue;
		M_TTrainingQueue.Enqueue(Items[i]);
	}
	return true;
}


void UTrainerComponent::DestroySpawnedActor()
{
	if (IsValid(M_SpawnedActor))
	{
		M_SpawnedActor->Destroy();
	}
	M_SpawnedActor = nullptr;
}

void UTrainerComponent::CancelAsyncLoadingRequest(EAsyncTrainingRequestState State)
{
	switch (State)
	{
	case EAsyncTrainingRequestState::Async_NoTrainingRequest:
	case EAsyncTrainingRequestState::Async_SpawningFailed:
		return;

	case EAsyncTrainingRequestState::Async_LoadingAsset:
		if (ARTSAsyncSpawner* Spawner = GetAsyncSpawner())
		{
			Spawner->CancelLoadRequestForTrainer(this);
			RTSFunctionLibrary::ReportError("We cancel the load request for this trainer");
		}
		break;
	case EAsyncTrainingRequestState::Async_TrainingSpawned:
		DestroySpawnedActor();
		RTSFunctionLibrary::ReportError("Destroying actor as we cancel the training request"
			"\n in function UTrainerComponent::CancelAsyncLoadingRequest");
	}
}

bool UTrainerComponent::GetSpawnTransform(FTransform& OutTransform) const
{
	if (not IsValid(M_TrainerSettings.TrainingMesh))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(
			this,
			"TrainingMesh on TrainerSettings struct",
			"UTrainerComponent::GetSpawnTransform");
		return false;
	}

	if (not M_TrainerSettings.TrainingMesh->DoesSocketExist(M_TrainerSettings.SpawnSocketName))
	{
		RTSFunctionLibrary::ReportError(
			"Spawn socket does not exist on training mesh."
			"\n in function UTrainerComponent::GetSpawnTransform"
			"\n Trainer component at: " + GetOwner()->GetName() +
			"socket name: " + M_TrainerSettings.SpawnSocketName.ToString());
		return false;
	}

	OutTransform = M_TrainerSettings.TrainingMesh->GetSocketTransform(M_TrainerSettings.SpawnSocketName);
	bool bWasSuccessful = false;
	OutTransform.SetLocation(RTSFunctionLibrary::GetLocationProjected(this, OutTransform.GetLocation(), true,
	                                                                  bWasSuccessful,
	                                                                  1));
	if (not bWasSuccessful)
	{
		RTSFunctionLibrary::ReportError("Failed to project spawn location to navmesh."
			"\n in function UTrainerComponent::GetSpawnTransform");
	}
	return true;
}


FVector UTrainerComponent::GetDefaultWayPoint() const
{
	if (IsValid(M_TrainerSettings.TrainingMesh) &&
		M_TrainerSettings.TrainingMesh->DoesSocketExist(M_TrainerSettings.DefaultWayPointSocketName))
	{
		return M_TrainerSettings.TrainingMesh->GetSocketLocation(M_TrainerSettings.DefaultWayPointSocketName);
	}

	RTSFunctionLibrary::ReportNullErrorComponent(
		this,
		"TrainingMesh on TrainerSettings struct or DefaultWayPointSocketName not found.",
		"UTrainerComponent::GetDefaultWayPoint");

	FTransform Spawn;
	if (GetSpawnTransform(Spawn))
	{
		constexpr float DefaultForwardWaypointOffset = 500.f; // 5m in front of spawn location
		return Spawn.GetLocation() + Spawn.GetRotation().GetForwardVector() * DefaultForwardWaypointOffset;
	}
	return FVector::ZeroVector;
}

FVector UTrainerComponent::GetValidSpawnPoint(const FVector& DesiredPoint) const
{
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (not NavSys)
	{
		return DesiredPoint;
	}

	FNavLocation NavLocation;
	const bool bFoundValidPoint =
		NavSys->ProjectPointToNavigation(DesiredPoint, NavLocation, FVector(500.f, 500.f, 500.f));

	return bFoundValidPoint ? NavLocation.Location : DesiredPoint;
}


bool UTrainerComponent::GetIsValidProgressionBar() const
{
	if (M_OwnerProgressBar.IsValid())
	{
		return true;
	}
	const FString Name = GetName();
	RTSFunctionLibrary::ReportError(
		"Invalid progression bar on training component For: " + Name);
	return false;
}

bool UTrainerComponent::GetIsValidPlayerResourceManager() const
{
	if (M_PlayerResourceManager.IsValid())
	{
		return true;
	}
	const FString Owner = IsValid(GetOwner()) ? GetOwner()->GetName() : "nullptr";
	RTSFunctionLibrary::ReportError("PlayerResourceManager is not valid when attempting to access it."
		"\n in function UTrainerComponent::GetIsValidPlayerResourceManager"
		"\n Owner: " + Owner);
	return false;
}

void UTrainerComponent::UpdateUIQueueForInsufficientResourceState(
	const bool bAtInsufficientResources,
	const FTrainingQueueItem& ActiveItem)
{
	// Always update the progress bar, independent of primary selection.
	if (GetIsValidProgressionBar())
	{
		if (bAtInsufficientResources)
		{
			constexpr bool bHideBarWhilePaused = false;
			M_OwnerProgressBar->PauseProgressBar(bHideBarWhilePaused);
		}
		else
		{
			// Resumes if paused; starts fresh if needed.
			M_OwnerProgressBar->StartProgressBar(ActiveItem.RemainingTrainingTime);
		}
	}

	// Only the menu clock needs the primary-trainer gate.
	if (GetIsPrimarySelected())
	{
		M_TrainingMenuManager->UpdateUIClockAtInsufficientResources(
			bAtInsufficientResources,
			ActiveItem.WidgetIndex,
			ActiveItem.RemainingTrainingTime,
			ActiveItem.ItemID,
			this);
	}
}

void UTrainerComponent::DebugTrainingComp(const FString& Message, const FColor& Color) const
{
	if (DeveloperSettings::Debugging::GTrainingComponent_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString(Message, Color);
	}
}

void UTrainerComponent::RefundManualRemoval(const FTrainingQueueItem& RemovedItem) const
{
	if (!GetIsValidPlayerResourceManager())
	{
		return;
	}

	// Use the shared helper to compute and refund paid amounts.
	RTSQueueHelpers::RefundQueueItemProgress(RemovedItem, M_PlayerResourceManager.Get());
}


void UTrainerComponent::InitTrainerComp_StartRequirementCheckTimer()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_RequirementCheckHandle);
		World->GetTimerManager().SetTimer(
			M_RequirementCheckHandle,
			this,
			&UTrainerComponent::CheckRequirementsAndRefreshUI,
			1.f,
			true
		);
	}
}


void UTrainerComponent::BeginPlay_InitPlayerResourceManager()
{
	M_PlayerResourceManager = FRTS_Statics::GetPlayerResourceManager(this);
	// Validity check.
	(void)GetIsValidPlayerResourceManager();
}

void UTrainerComponent::BeginPlay_InitPlayerController()
{
	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (ACPPController* CPPController = Cast<ACPPController>(PlayerController))
		{
			M_PlayerController = CPPController;
		}
	}
	// Valicity check.
	(void)GetIsValidPlayerController();
}


bool UTrainerComponent::RemoveLastInstanceOfTypeFromQueue(const FTrainingOption& TrainingID)
{
	if (not M_TrainingEnabled.bIsAbleToTrain)
	{
		RTSFunctionLibrary::ReportError("cannot remove from queue as training queue is disabled!" + GetName());
		return false;
	}
	if (not GetIsValidOwningTrainer())
	{
		return false;
	}
	if (not M_TrainingEnabled.bIsQueueOnPause)
	{
		const FString OwnerName = IsValid(GetOwner()) ? GetOwner()->GetName() : "nullptr";
		RTSFunctionLibrary::ReportError("Attempted to remove item from queue while the queue is not paused."
			"\n this is note supported."
			"\n Training comp: " + GetName() +
			"\n Owner: " + OwnerName);
		return false;
	}

	bool bRemovedActiveItem = false;
	EAsyncTrainingRequestState RemovedActiveState = EAsyncTrainingRequestState::Async_NoTrainingRequest;
	FTrainingQueueItem RemovedItem;

	const bool bFoundOption = RemoveLastQueueItemOfOption(
		TrainingID, bRemovedActiveItem, RemovedActiveState, RemovedItem);

	DebugTrainingComp("After removing item from queue, we have: ");
	DebugPrintQueueLayout();

	if (not bFoundOption)
	{
		return false;
	}

	// Refund any progress already paid
	RefundManualRemoval(RemovedItem);

	// Remove the requirement snapshot if present
	UnregisterRequirementSnapshot(RemovedItem.RequirementKey);

	// Notify trainer & adjust counts
	M_OwningTrainer->OnTrainingCancelled(TrainingID, bRemovedActiveItem);
	M_TQueueCounts.FindOrAdd(TrainingID) = FMath::Max(M_TQueueCounts[TrainingID] - 1, 0);


	if (bRemovedActiveItem)
	{
		// if there’s nothing left at all we kill the progress bar.
		if (M_TTrainingQueue.IsEmpty())
		{
			StopQueueTimer(EStopTrainingQueueTimerReason::QueueCompleted_StopProgressBar);
		}
		else
		{
			// otherwise, reset the progress bar as it is paused for an item type that is no longer in the queue.
			StopQueueTimer(EStopTrainingQueueTimerReason::QueuePaused_RemovedActive_ResetProgressBar_QueueNotEmpty);
		}

		// Make sure that for the next item in the queue we do not have a resource issue at the start; rather,
		// let it be recalculated for  this new item at the UpdateQueue tick.
		bM_AtInsufficientResources = false;

		// also cancel any in‐flight async load for that active item
		CancelAsyncLoadingRequest(RemovedActiveState);
	}
	// Expected to be primary as this action can only be called from the training menu manager.
	if (EnsureIsPrimarySelected())
	{
		M_TrainingMenuManager->RequestRefreshTrainingUI(this);
	}
	return bFoundOption;
}

TMap<FTrainingOption, int32> UTrainerComponent::GetCurrentTrainingQueueState(
	int32& OutActiveTrainingOptionIndex,
	int32& OutTimeRemaining, bool& OutbIsPausedByResourceInsufficiency) const
{
	if (!M_TTrainingQueue.IsEmpty())
	{
		// Look at the item at the tail.
		const FTrainingQueueItem* ActiveItem = M_TTrainingQueue.Peek();
		OutActiveTrainingOptionIndex = ActiveItem->WidgetIndex;
		OutTimeRemaining = ActiveItem->RemainingTrainingTime;
	}
	else
	{
		OutActiveTrainingOptionIndex = INDEX_NONE;
		OutTimeRemaining = 0;
	}
	OutbIsPausedByResourceInsufficiency = bM_AtInsufficientResources;
	return M_TQueueCounts;
}

TArray<FTrainingOption> UTrainerComponent::GetTrainingOptions() const
{
	return M_TTrainingOptions;
}

ARTSAsyncSpawner* UTrainerComponent::GetAsyncSpawner() const
{
	if (not GetIsValidPlayerController())
	{
		return nullptr;
	}
	if (ARTSAsyncSpawner* Spawner = M_PlayerController->GetRTSAsyncSpawner())
	{
		return Spawner;
	}
	RTSFunctionLibrary::ReportError("Could not obtain valid spawner"
		"\n in function UTrainerComponent::GetAsyncSpawner");
	return nullptr;
}


void UTrainerComponent::StartQueueTimer(const bool bIsStartAfterPause)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_QueueHandle);
		World->GetTimerManager().SetTimer(M_QueueHandle, this, &UTrainerComponent::UpdateQueue, 1.f, true);
	}
	if (M_TTrainingQueue.IsEmpty())
	{
		return;
	}
	FTrainingQueueItem* ActiveItem = M_TTrainingQueue.Peek();
	if (ActiveItem && GetIsValidProgressionBar() && ActiveItem->RemainingTrainingTime > 0)
	{
		M_OwnerProgressBar->StartProgressBar(ActiveItem->RemainingTrainingTime);
	}

	if (not bIsStartAfterPause && M_OwningTrainer.IsValid())
	{
		M_OwningTrainer->OnTrainingProgressStartedOrResumed(ActiveItem->ItemID);
	}
}

void UTrainerComponent::StopQueueTimer(const EStopTrainingQueueTimerReason Reason)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_QueueHandle);
	}
	if (not GetIsValidProgressionBar())
	{
		return;
	}
	if (Reason == EStopTrainingQueueTimerReason::QueuePaused_PauseProgressBar)
	{
		constexpr bool bHideBarWhilePaused = false;
		M_OwnerProgressBar->PauseProgressBar(bHideBarWhilePaused);
		return;
	}
	if (Reason == EStopTrainingQueueTimerReason::QueueCompleted_StopProgressBar)
	{
		M_OwnerProgressBar->StopProgressBar();
		return;
	}
	if (Reason == EStopTrainingQueueTimerReason::QueuePaused_RemovedActive_ResetProgressBar_QueueNotEmpty)
	{
		// Reset any progress and pause states as the active item was removed.
		M_OwnerProgressBar->ResetProgressBarKeepVisible();
		return;
	}
	RTSFunctionLibrary::ReportError("Trainer comp asked to stop the progress bar but did not provide a valid reason."
		"Reason as integer: " + FString::FromInt(static_cast<int32>(Reason)));
}

void UTrainerComponent::UpdateQueue()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Training_UpdateQueue);
	if (!M_TTrainingQueue.IsEmpty() && !M_WaitForLoadingAsset)
	{
		FTrainingQueueItem* ActiveItem = M_TTrainingQueue.Peek();
		// Attempt to pay for this second of training.
		if (not UpdateQueue_PauseQueueInsufficientResources(ActiveItem))
		{
			// We cannot continue subtracting time as the player is out of resources.
			bM_AtInsufficientResources = true;
			// The remaining time will not be altered; pause the UI clock with current remaining time.
			UpdateUIQueueForInsufficientResourceState(true, *ActiveItem);
			return;
		}
		ActiveItem->RemainingTrainingTime--;
		if (bM_AtInsufficientResources)
		{
			// The queue was paused by resource insufficiency before and is now started again; the time for this tick
			// is already updated -> set the UI clock to this remaining time and unpause it.
			UpdateUIQueueForInsufficientResourceState(false, *ActiveItem);
			bM_AtInsufficientResources = false;
		}
		// Starts spawning the item if not spawning yet and time remaining is below threshold.
		DetermineSpawnActiveItem(ActiveItem);


		if (ActiveItem->RemainingTrainingTime <= 0)
		{
			if (ActiveItem->AsyncRequestState == Async_LoadingAsset)
			{
				// The queue will stop updating until this flag is reset.
				M_WaitForLoadingAsset = true;
				return;
			}
			// Propagates loaded asset and updates UI if needed.
			TrainingCompleteRemoveFromQueue();
		}
	}
	else
	{
		DebugTrainingComp("Waiting for asset to load");
	}
}

bool UTrainerComponent::UpdateQueue_PauseQueueInsufficientResources(FTrainingQueueItem* ActiveQueueItem) const
{
	if (!EnsureActiveItemIsValid(ActiveQueueItem))
	{
		return false;
	}

	// Calculate the tick cost for this second
	TMap<ERTSResourceType, int32> TickCost;
	if (GetCanPayForItemTick(ActiveQueueItem, TickCost))
	{
		// Attempt to pay; if successful, consume resources and continue
		if (PayForItemTick(ActiveQueueItem, TickCost))
		{
			DebugTrainingComp("Queue Tick Payed", FColor::Green);
			return true;
		}
		// Shouldn't happen: check passed but pay failed
		RTSFunctionLibrary::ReportError("Training comp failed to pay for item tick despite passing check"
			"\n See UpdateQueue_PauseQueueInsufficientResources.");
		return false;
	}

	// Not enough resources this tick: pause the queue
	DebugTrainingComp("Queue On insufficientResources", FColor::Red);
	return false;
}

bool UTrainerComponent::GetCanPayForItemTick(
	const FTrainingQueueItem* const ActiveQueueItem,
	TMap<ERTSResourceType, int32>& OutCalculatedTickCost) const
{
	if (!GetIsValidPlayerResourceManager() || !EnsureActiveItemIsValid(ActiveQueueItem))
	{
		return false;
	}

	const int32 TotalTime = ActiveQueueItem->TotalTrainingTime;
	const int32 RemainingTime = FMath::Max(ActiveQueueItem->RemainingTrainingTime, 1);
	const int32 TickNumber = TotalTime - RemainingTime + 1;

	// Distribute each resource cost exactly over TotalTime ticks
	for (const auto& Pair : ActiveQueueItem->TotalResourceCosts)
	{
		const ERTSResourceType Resource = Pair.Key;
		const int32 TotalCost = Pair.Value;

		// Amount paid before this tick
		const int32 PaidBefore = (TickNumber - 1) * TotalCost / TotalTime;
		// Amount paid after this tick
		const int32 PaidNow = TickNumber * TotalCost / TotalTime;
		// This tick's cost is the difference, ensuring sum == TotalCost
		const int32 CostThisTick = PaidNow - PaidBefore;

		OutCalculatedTickCost.Add(Resource, CostThisTick);
	}

	// Check if player can pay exactly this tick's cost
	// Stop any resource ticking if insufficient.
	return M_PlayerResourceManager->GetCanPayForCost(OutCalculatedTickCost) == EPlayerError::Error_None;
}


bool UTrainerComponent::PayForItemTick(
	FTrainingQueueItem* const ActiveQueueItem,
	const TMap<ERTSResourceType, int32>& TickCost) const
{
	if (!GetIsValidPlayerResourceManager() || !EnsureActiveItemIsValid(ActiveQueueItem))
	{
		return false;
	}

	// Consume resources (we've already validated availability)
	// Play resource tick audio.
	M_PlayerResourceManager->PayForCostsNoCheck(TickCost);

	// Subtract the paid cost from the item's remaining cost map
	for (const auto& Entry : TickCost)
	{
		const ERTSResourceType Resource = Entry.Key;
		const int32 PaidAmount = Entry.Value;
		int32& RemainingForResource = ActiveQueueItem->RemainingResourceCosts.FindOrAdd(Resource);
		RemainingForResource = FMath::Max(RemainingForResource - PaidAmount, 0);
	}

	return true;
}


bool UTrainerComponent::EnsureActiveItemIsValid(const FTrainingQueueItem* const ActiveQueueItem) const
{
	if (ActiveQueueItem)
	{
		return true;
	}
	const FString OwnerName = IsValid(GetOwner()) ? GetOwner()->GetName() : "nullptr";
	RTSFunctionLibrary::ReportError("Active item is not valid when checking if it is valid."
		"\n in function UTrainerComponent::EnsureActiveItemIsValid"
		"\n Trainer component at: " + OwnerName);
	return false;
}


void UTrainerComponent::TrainingCompleteRemoveFromQueue()
{
	DebugTrainingComp("Training complete remove from queue", FColor::Green);
	DebugPrintQueueLayout();

	const FTrainingQueueItem FinishedItem = *M_TTrainingQueue.Peek();

	// Accounting
	M_TQueueCounts.FindOrAdd(FinishedItem.ItemID) =
		FMath::Max(M_TQueueCounts[FinishedItem.ItemID] - 1, 0);

	// Physically pop
	M_TTrainingQueue.Pop();

	// Remove the requirement snapshot for this item
	UnregisterRequirementSnapshot(FinishedItem.RequirementKey);

	// Update UI/progress bar, then propagate spawn
	if (const FTrainingQueueItem* NextItem = M_TTrainingQueue.Peek())
	{
		if (M_OwnerProgressBar.IsValid())
		{
			M_OwnerProgressBar->StartProgressBar(NextItem->RemainingTrainingTime);
		}
	}
	else
	{
		StopQueueTimer(EStopTrainingQueueTimerReason::QueueCompleted_StopProgressBar);
	}

	RefreshTrainingMenuIfPrimary();
	// Notify trainer about the completed item (may run BP logic that hides the old preview, etc.)
	PropagateSpawnedActorToTrainer(FinishedItem);

	//  if the queue continues without pausing, notify that the NEXT item has started/resumed 
	if (!M_TrainingEnabled.bIsQueueOnPause && M_OwningTrainer.IsValid())
	{
		if (const FTrainingQueueItem* NewActive = M_TTrainingQueue.Peek())
		{
			// This is the new head right after completion; fire the started/resumed hook for it.
			M_OwningTrainer->OnTrainingProgressStartedOrResumed(NewActive->ItemID);
		}
	};
}

void UTrainerComponent::PropagateSpawnedActorToTrainer(const FTrainingQueueItem FinishedItem)
{
	if (not M_OwningTrainer.IsValid())
	{
		RTSFunctionLibrary::ReportError(
			"Owning trainer is not valid when spawning actor."
			"\n in function UTrainerComponent::PropagateSpawnedActorToTrainer"
			"\n We destroy the spawned actor"
			"\n Completed item: " + FinishedItem.ItemID.GetTrainingName());
		DestroySpawnedActor();
	}

	if (FinishedItem.AsyncRequestState == Async_TrainingSpawned && IsValid(M_SpawnedActor))
	{
		MakeActorReadyForSpawn(M_SpawnedActor);
		M_OwningTrainer->OnTrainingComplete(FinishedItem.ItemID, M_SpawnedActor);
		M_SpawnedActor = nullptr;
		return;
	}

	RTSFunctionLibrary::ReportError(
		"Spawned actor is not valid while finished item is not waiting for asset."
		"\n in function UTrainerComponent::PropagateSpawnedActorToTrainer"
		"\n We reset all state for this item and continue the queue."
		"\n Completed item: " + FinishedItem.ItemID.GetTrainingName());
	M_SpawnedActor = nullptr;
}


bool UTrainerComponent::GetIsValidPlayerController() const
{
	if (M_PlayerController.IsValid())
	{
		return true;
	}
	const FString Name = GetName();
	RTSFunctionLibrary::ReportError(
		"Invalid PlayerController For: " + Name);
	return false;
}

void UTrainerComponent::MakeActorReadyForSpawn(AActor* ActorToMakeReady) const
{
	if (not IsValid(ActorToMakeReady))
	{
		RTSFunctionLibrary::ReportError("MakeActorReadyForSpawn called with invalid actor.");
		return;
	}

	if (IRTSUnit* RTSUnit = Cast<IRTSUnit>(ActorToMakeReady))
	{
		const FVector Waypoint = GetDesiredRallyPointLocation();

		// Draw debug sphere for visibility (short-lived).
		constexpr float DebugRadius = 60.0f;
		constexpr int32 DebugSegments = 12;
		constexpr float DebugLifeTime = 5.0f;
		constexpr float DebugThickness = 2.0f;
		RTSUnit->OnRTSUnitSpawned(false, M_TrainerSettings.TimeNotSelectable, Waypoint);
		return;
	}

	RTSFunctionLibrary::ReportFailedCastError(
		ActorToMakeReady->GetName(),
		"IRTSUnit",
		"UTrainerComponent::MakeActorReadyForSpawn"
		"\n Training component at: " + (IsValid(GetOwner()) ? GetOwner()->GetName() : FString("UnknownOwner")));
}


bool UTrainerComponent::GetIsValidOwningTrainer() const
{
	if (not M_OwningTrainer.IsValid())
	{
		const FString OwnerName = IsValid(GetOwner()) ? GetOwner()->GetName() : "nullptr";
		RTSFunctionLibrary::ReportError("Owning trainer is not valid when attempting to access it."
			"\n in function UTrainerComponent::GetIsValidOwningTrainer"
			"\n Owner: " + OwnerName);
		return false;
	}
	return true;
}

void UTrainerComponent::DetermineSpawnActiveItem(FTrainingQueueItem* ActiveItem)
{
	if (ActiveItem->AsyncRequestState == Async_NoTrainingRequest && ActiveItem->RemainingTrainingTime <=
		DeveloperSettings::GamePlay::Training::TimeRemainingStartAsyncLoading)
	{
		if (GetIsValidPlayerController() && M_OwningTrainer.IsValid())
		{
			if (ARTSAsyncSpawner* Spawner = GetAsyncSpawner())
			{
				// The request will only fail if the trainer is already in the queue.
				if (Spawner->AsyncSpawnTrainingOption(
					ActiveItem->ItemID,
					this))
				{
					// It is possible Async instantly loaded the asset, in which case the status is set to spawned!
					// Which is why we only set to loading if the current status is at no request.
					if (ActiveItem->AsyncRequestState == Async_NoTrainingRequest)
					{
						ActiveItem->AsyncRequestState = Async_LoadingAsset;
					}
				}
				else
				{
					ActiveItem->AsyncRequestState = Async_SpawningFailed;
				}
			}
		}
	}
}


void UTrainerComponent::RefreshTrainingMenuIfPrimary(
) const
{
	if (not GetIsPrimarySelected())
	{
		return;
	}
	M_TrainingMenuManager->RequestRefreshTrainingUI(this);
}


void UTrainerComponent::OnOptionSpawned(
	AActor* SpawnedActor,
	const FTrainingOption Option)
{
	if (IsValid(SpawnedActor))
	{
		M_SpawnedActor = SpawnedActor;

		if (FTrainingQueueItem* Head = M_TTrainingQueue.Peek())
		{
			Head->AsyncRequestState = Async_TrainingSpawned;
		}
		else
		{
			// Queue was cleared (likely by requirement removals); drop the spawn safely.
			RTSFunctionLibrary::ReportError("Async spawn completed but queue was empty; destroying spawned actor.");
			DestroySpawnedActor();
			return;
		}
		if (M_OwningTrainer.IsValid())
		{
			FTransform SpawnTransform;
			if (GetSpawnTransform(SpawnTransform))
			{
				DebugTrainingComp("Z value of teleport to training location: " + FString::SanitizeFloat(
					                  SpawnTransform.GetLocation().Z), FColor::Purple);
				SpawnedActor->TeleportTo(SpawnTransform.GetLocation(),
				                         SpawnTransform.GetRotation().Rotator(),
				                         false,
				                         true);
				OnSpawnedActorCheckForSquadController(SpawnedActor, SpawnTransform.GetLocation());
			}
			else
			{
				RTSFunctionLibrary::ReportError("Spawn Transform not found, destroying spawned actor.");
				DestroySpawnedActor();
			}
		}
		else
		{
			RTSFunctionLibrary::ReportError("Owning trainer is not valid when spawning actor."
				"\n in function UTrainerComponent::OnOptionSpawned"
				"\n We destroy the spawned actor");
			DestroySpawnedActor();
		}
	}
	else
	{
		M_TTrainingQueue.Peek()->AsyncRequestState = Async_SpawningFailed;
	}
	if (M_WaitForLoadingAsset)
	{
		// Complete training for waiting item.
		TrainingCompleteRemoveFromQueue();
	}
	M_WaitForLoadingAsset = false;
}

void UTrainerComponent::SetTrainingEnabled(const bool bIsAbleToTrain)
{
	// Pause the queue if we cannot train anymore.
	SetPauseQueue(not bIsAbleToTrain);
	if (not bIsAbleToTrain)
	{
		// The training component is disabled meaning that the owning nomadic vehicle will convert, because pausing the
		// queue pauses the progressbar but this bar is needed for the nomadic vehicle we reset the pause on the bar here.
		if (GetIsValidProgressionBar())
		{
			M_OwnerProgressBar->StopProgressBar();
		}
	}
	// Hide or show UI if primary selected.
	if (GetIsPrimarySelected())
	{
		// Will reload the training UI with this component.
		// Will also stop the clock animation if needed on training item previously activated from different trainer. 
		M_TrainingMenuManager->RequestEnableTrainingUI(this, bIsAbleToTrain);
	}
	M_TrainingEnabled.bIsAbleToTrain = bIsAbleToTrain;
	if (bIsAbleToTrain)
	{
		SpawnRallyPointActorIfNeeded(M_RallyPointActorClass);
	}
}

void UTrainerComponent::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_InitPlayerController();
	BeginPlay_InitPlayerResourceManager();
	// Subscribe to selection-bucket visibility updates (PrimarySameType changes).
	BeginPlay_AddToSelectionPrimaryType();
}

void UTrainerComponent::SpawnRallyPointActorIfNeeded(TSubclassOf<ARTSRallyPointActor> RallyClass)
{
	UWorld* World = GetWorld();
	if (not World || M_RallyPointActor.IsValid())
	{
		return;
	}

	if (not RallyClass)
	{
		RTSFunctionLibrary::ReportError(
			"RallyPointActorClass not provided to TrainerComponent::InitUTrainerComponent on "
			+ (IsValid(GetOwner()) ? GetOwner()->GetName() : FString("UnknownOwner")));
		return;
	}


	const FVector SpawnLocation = GetValidSpawnPoint(GetDefaultWayPoint());
	const FTransform SpawnTM(FRotator::ZeroRotator, SpawnLocation);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Owner = GetOwner();
	SpawnParams.Instigator = Cast<APawn>(GetOwner());

	ARTSRallyPointActor* Spawned = World->SpawnActor<ARTSRallyPointActor>(RallyClass, SpawnTM, SpawnParams);
	if (not IsValid(Spawned))
	{
		RTSFunctionLibrary::ReportError("Failed to spawn RallyPointActor in TrainerComponent.");
		return;
	}
	bool bIsPrimarySelectedType = false;
	if (M_OwnerSelectionComponent.IsValid())
	{
		bIsPrimarySelectedType = M_OwnerSelectionComponent->GetIsInPrimarySameType();
	}

	Spawned->SetActorHiddenInGame(not bIsPrimarySelectedType);
	Spawned->SetActorEnableCollision(false);

	M_RallyPointActor = Spawned;
}


void UTrainerComponent::BeginPlay_AddToSelectionPrimaryType()
{
	AActor* OwnerActor = GetOwner();
	if (not IsValid(OwnerActor))
	{
		RTSFunctionLibrary::ReportError("TrainerComponent owner invalid in BeginPlay_AddToSelectionPrimaryType.");
		return;
	}

	USelectionComponent* SelectionComp = OwnerActor->FindComponentByClass<USelectionComponent>();
	if (not IsValid(SelectionComp))
	{
		RTSFunctionLibrary::ReportError("SelectionComponent not found on owner for TrainerComponent.");
		return;
	}

	M_OwnerSelectionComponent = SelectionComp;

	// Safe object binding (no lambdas needed; engine unbinds when this component dies).
	SelectionComp->OnPrimarySameTypeChanged.AddUObject(this, &UTrainerComponent::OnSelectedAsSameTypePrimary);
}

void UTrainerComponent::OnSelectedAsSameTypePrimary(const bool bIsInPrimarySameType)
{
	// fail silently as the rally point actor may not be spawned (nomad in truck mode)
	if (not M_RallyPointActor.IsValid())
	{
		return;
	}
	// Show rally point only while this trainer's unit is in the primary-same-type bucket.
	M_RallyPointActor.Get()->SetActorHiddenInGame(not bIsInPrimarySameType);
}


bool UTrainerComponent::GetIsValidRallyPointActor() const
{
	if (M_RallyPointActor.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("RallyPointActor invalid in TrainerComponent.");
	return false;
}

bool UTrainerComponent::GetIsValidSelectionComponent() const
{
	if (M_OwnerSelectionComponent.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("SelectionComponent invalid in TrainerComponent.");
	return false;
}

void UTrainerComponent::OnSpawnedActorCheckForSquadController(AActor* SpawnedActor, const FVector& SpawnLocation) const
{
	if (not IsValid(SpawnedActor))
	{
		return;
	}
	ASquadController* SquadController = Cast<ASquadController>(SpawnedActor);
	if (not IsValid(SquadController))
	{
		return;
	}
	SquadController->SetSquadSpawnLocation(SpawnLocation);
	if (not SquadController->SetActorLocation(SpawnLocation, false, nullptr, ETeleportType::ResetPhysics))
	{
		RTSFunctionLibrary::PrintString("Failed to set location on spawned SquadController actor."
		                                "\n Location: " + SpawnLocation.ToString(), FColor::Red, 15);
	}
}


FVector UTrainerComponent::GetDesiredRallyPointLocation() const
{
	if (M_RallyPointActor.IsValid())
	{
		return M_RallyPointActor.Get()->GetActorLocation();
	}
	// Fallback: previous default waypoint logic.
	return GetValidSpawnPoint(GetDefaultWayPoint());
}


bool UTrainerComponent::GetIsPrimarySelected() const
{
	return IsValid(M_TrainingMenuManager);
}

bool UTrainerComponent::EnsureIsPrimarySelected() const
{
	if (not IsValid(M_TrainingMenuManager))
	{
		const FString OwnerName = IsValid(GetOwner()) ? GetOwner()->GetName() : "nullptr";
		RTSFunctionLibrary::ReportError("Trainer component is not primary selected, cannot update UI."
			"\n in function UTrainerComponent::EnsureIsPrimarySelected"
			"\n Trainer with Owner: " + OwnerName);
		return false;
	}
	return true;
}

void UTrainerComponent::SetPauseQueue(const bool bPause)
{
	M_TrainingEnabled.bIsQueueOnPause = bPause;
	if (bPause)
	{
		StopQueueTimer(EStopTrainingQueueTimerReason::QueuePaused_PauseProgressBar);
	}
	else if (!M_TTrainingQueue.IsEmpty())
	{
		StartQueueTimer(true);
	}
}


void UTrainerComponent::DebugPrintQueueLayout()
{
	if (DeveloperSettings::Debugging::GTrainingComponent_Compile_DebugSymbols)
	{
		// Build a multi-line debug string
		FString Debug;
		Debug += TEXT("\n\n===== TRAINER QUEUE LAYOUT =====\n\n");

		// 1) Current world time
		const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		Debug += FString::Printf(TEXT("Current Queue Time: %.2f\n\n"), CurrentTime);

		// 2) Active vs pending items
		if (M_TTrainingQueue.IsEmpty())
		{
			Debug += TEXT("Queue is EMPTY\n\n");
		}
		else
		{
			// Active item (in RED)
			const FTrainingQueueItem* ActiveItem = M_TTrainingQueue.Peek();
			const float TotalTime = ActiveItem->TotalTrainingTime;

			Debug += TEXT("[RED] ACTIVE ITEM:\n");
			if (ActiveItem)
			{
				Debug += FString::Printf(TEXT("    Name           : %s\n"), *ActiveItem->ItemID.GetTrainingName());
				Debug += FString::Printf(TEXT("    Remaining Time : %d\n"), ActiveItem->RemainingTrainingTime);
				Debug += FString::Printf(TEXT("    Total Time     : %f\n\n"), TotalTime);
			}
			else
			{
				Debug += TEXT("    No active item\n\n");
			}

			// Pending items (in BLUE)
			Debug += TEXT("[BLUE] PENDING ITEMS:\n");
			TQueue<FTrainingQueueItem, EQueueMode::Spsc> TempQueue;
			TQueue<FTrainingQueueItem, EQueueMode::Spsc> Helper;
			FTrainingQueueItem LastItem;

			// Iterate through the queue to find the last occurrence of the item and store all items temporarily
			while (M_TTrainingQueue.Dequeue(LastItem))
			{
				TempQueue.Enqueue(LastItem);
				Helper.Enqueue(LastItem);
			}
			while (Helper.Dequeue(LastItem))
			{
				M_TTrainingQueue.Enqueue(LastItem);
			}

			FTrainingQueueItem TmpItem;
			TempQueue.Dequeue(TmpItem); // skip the active one
			int32 Index = 1;
			while (TempQueue.Dequeue(TmpItem))
			{
				Debug += FString::Printf(TEXT("    [%d] %s\n"), Index++, *TmpItem.ItemID.GetTrainingName());
			}
			Debug += TEXT("\n");
		}

		// 3) Component & owner names
		Debug += FString::Printf(TEXT("Component: %s\n"), *GetName());
		const FString OwnerName = GetOwner() ? GetOwner()->GetName() : TEXT("None");
		Debug += FString::Printf(TEXT("Owner    : %s\n\n"), *OwnerName);

		// Print it all at once
		RTSFunctionLibrary::PrintString(Debug, FColor::White, 20);
	}
}

void UTrainerComponent::NotifyTrainerCancelled(const TScriptInterface<ITrainer>& OwningTrainer,
                                               const FTrainingOption& CancelledOption, const bool bCancelledActiveItem)
{
	if (IsValid(OwningTrainer.GetObject()))
	{
		OwningTrainer->OnTrainingCancelled(CancelledOption, bCancelledActiveItem);
	}
}

void UTrainerComponent::MoveRallyPointActor(const FVector NewWorldLocation)
{
	if (not GetIsValidRallyPointActor())
	{
		return;
	}
	// Ignore collisions: no sweep; teleport type is fine (no physics).
	M_RallyPointActor.Get()->SetActorLocation(NewWorldLocation, /*bSweep*/ false, /*OutHit*/ nullptr,
	                                          ETeleportType::TeleportPhysics);
}

bool UTrainerComponent::RemoveInvalidRequirementItems_AndRefund()
{
	using namespace RTSQueueHelpers;

	// 1) Drain queue
	TArray<FTrainingQueueItem> Items;
	DrainQueueToArray(M_TTrainingQueue, Items);
	if (Items.Num() == 0)
	{
		return false;
	}

	// Build an owning-trainer interface for helpers (may be null)
	TScriptInterface<ITrainer> OwningTrainerIf;
	if (M_OwningTrainer.IsValid())
	{
		OwningTrainerIf.SetObject(M_OwningTrainer.GetObject());
		OwningTrainerIf.SetInterface(M_OwningTrainer.Get());
	}

	// 2) Partition by requirement (also refunds & cancels via params we pass)
	TArray<FTrainingQueueItem> Kept;
	bool bRemovedActiveItem = false;
	EAsyncTrainingRequestState RemovedActiveAsyncState = EAsyncTrainingRequestState::Async_NoTrainingRequest;

	const bool bModified = PartitionItemsByRequirement(
		Items,
		M_PlayerResourceManager.Get(),
		M_TQueueCounts,
		OwningTrainerIf,
		this,
		Kept,
		bRemovedActiveItem,
		RemovedActiveAsyncState);

	// 3) If nothing changed → requeue originals and bail
	if (!bModified)
	{
		RequeueFromArray(M_TTrainingQueue, Items);
		return false;
	}

	// 4) If active item was removed → stop/reset progress bar & cancel async
	if (bRemovedActiveItem)
	{
		HandleActiveItemRemoved(
			Kept.Num(),
			RemovedActiveAsyncState,
			[this](EStopTrainingQueueTimerReason Reason)
			{
				this->StopQueueTimer(Reason);
			},
			[this](EAsyncTrainingRequestState State)
			{
				this->CancelAsyncLoadingRequest(State);
			},
			bM_AtInsufficientResources);
	}

	// 5) Re-enqueue survivors (preserve order)
	RequeueFromArray(M_TTrainingQueue, Kept);

	// 6) Restart timer if anything left and we removed the active item
	RestartTimerIfNeededAfterRemoval(
		bRemovedActiveItem,
		M_TTrainingQueue,
		[this](bool bIsStartAfterPause)
		{
			this->StartQueueTimer(bIsStartAfterPause);
		});


	RefreshTrainingMenuIfPrimary();
	return true;
}
