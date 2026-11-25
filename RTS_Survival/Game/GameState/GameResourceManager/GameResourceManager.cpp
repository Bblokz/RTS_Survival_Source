// Copyright (C) Bas Blokzijl - All rights reserved.


#include "GameResourceManager.h"

#include "GetTargetResourceThread/FGetAsyncResource.h"
#include "GetTargetResourceThread/AsyncDropOffData/FAsyncResourceDropOffData.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/Resources/ResourceDropOff/ResourceDropOff.h"
#include "RTS_Survival/Resources/ResourceComponent/ResourceComponent.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

UGameResourceManager::UGameResourceManager(const FObjectInitializer& ObjectInitializer)
	: M_ResourceDropOffs(),
	  M_MapResources()
{
	M_UpdateDropOffDataInterval = DeveloperSettings::Async::GameThreadUpdateAsyncWithResourceDropOffsInterval;
	M_UpdateResourcesDataInterval = DeveloperSettings::Async::GameThreadUpdateAsyncWithResourcesInterval;
}


void UGameResourceManager::RegisterMapResource(
	const TObjectPtr<UResourceComponent>& Resource,
	const bool bRegister)
{
	if (not IsValid(Resource))
	{
		const FString IsContained = M_MapResources.Contains(Resource)
			                            ? "resource is in M_MapResources"
			                            : "resource is not in M_MapResources";
		const FString Register = bRegister ? "Registered to" : "Removed from";
		RTSFunctionLibrary::ReportError(
			"Resource is not valid in RegisterMapResource or failed to remove resource from array."
			"\n at function RegisterMapResource in CPPGameState.cpp."
			"\n Resource status in array: " + IsContained +
			"Resource will not be " + Register + " the array in game state.");
		return;
	}
	if (bRegister && not M_MapResources.Contains(Resource))
	{
		M_MapResources.Add(Resource);
	}
	else if (not bRegister && M_MapResources.Contains(Resource))
	{
		M_MapResources.Remove(Resource);
	}
}

void UGameResourceManager::RegisterResourceDropOff(
	const TWeakObjectPtr<UResourceDropOff>& ResourceDropOff,
	const bool bRegister,
	const int8 OwningPlayer)
{
	constexpr int8 HumanPlayer = 1;
	const int32 RegisteredIndex = FindRegisteredDropOffIndex(ResourceDropOff);
	if (bRegister && RegisteredIndex == INDEX_NONE)
	{
		if (not ResourceDropOff.IsValid())
		{
			RTSFunctionLibrary::ReportError("cannot register DropOff as it is INVALID"
								"\n See UGameResourceManager::RegisterResourceDropOff");
			return;
		}
		FRegisteredResourceDropOff NewEntry;
		NewEntry.DropOff = ResourceDropOff;
		NewEntry.OwningPlayer = OwningPlayer;
		M_ResourceDropOffs.Add(NewEntry);
		// Only the human's drop-offs hold the human's resources; an AI drop-off keeps its stock to itself.
		if (OwningPlayer == HumanPlayer)
		{
			RegisterDropOffWithPlayerResourceManager(bRegister, ResourceDropOff);
		}
		return;
	}
	if (not bRegister && RegisteredIndex != INDEX_NONE)
	{
		const int8 RegisteredOwner = M_ResourceDropOffs[RegisteredIndex].OwningPlayer;
		M_ResourceDropOffs.RemoveAt(RegisteredIndex);
		// As before AI drop-offs were supported: the human's storage only shrinks when the drop-off still reports
		// player 1. During BeginDestroy its owner actor is gone and it reports 0, which leaves the storage as it was.
		if (RegisteredOwner == HumanPlayer && OwningPlayer == HumanPlayer)
		{
			RegisterDropOffWithPlayerResourceManager(bRegister, ResourceDropOff);
		}
	}
}

int32 UGameResourceManager::FindRegisteredDropOffIndex(const TWeakObjectPtr<UResourceDropOff>& DropOff) const
{
	return M_ResourceDropOffs.IndexOfByPredicate([&DropOff](const FRegisteredResourceDropOff& Entry)
	{
		return Entry.DropOff == DropOff;
	});
}

TArray<TWeakObjectPtr<UResourceDropOff>> UGameResourceManager::GetCopyOfResourceDropOffs() const
{
	TArray<TWeakObjectPtr<UResourceDropOff>> DropOffs;
	DropOffs.Reserve(M_ResourceDropOffs.Num());
	for (const FRegisteredResourceDropOff& Entry : M_ResourceDropOffs)
	{
		DropOffs.Add(Entry.DropOff);
	}
	return DropOffs;
}

TArray<TWeakObjectPtr<UResourceDropOff>> UGameResourceManager::GetCopyOfResourceDropOffsOfPlayer(
	const int8 OwningPlayer) const
{
	TArray<TWeakObjectPtr<UResourceDropOff>> DropOffs;
	for (const FRegisteredResourceDropOff& Entry : M_ResourceDropOffs)
	{
		if (Entry.OwningPlayer == OwningPlayer)
		{
			DropOffs.Add(Entry.DropOff);
		}
	}
	return DropOffs;
}

TArray<TWeakObjectPtr<UResourceComponent>> UGameResourceManager::GetCopyOfMapResources() const
{
	return M_MapResources;
}

void UGameResourceManager::AsyncRequestClosestDropOffs(
	const FVector& HarvesterLocation,
	const int32 NumDropOffs,
	uint8 OwningPlayer,
	int32 DropOffAmount,
	ERTSResourceType ResourceType,
	const TFunction<void(const TArray<TWeakObjectPtr<UResourceDropOff>>&)>& Callback)
{
	if (M_GetAsyncResourceThread)
	{
		M_GetAsyncResourceThread->ScheduleDropOffRequest(
			HarvesterLocation,
			NumDropOffs,
			OwningPlayer,
			DropOffAmount,
			ResourceType,
			Callback);
		return;
	}
	// Invoke callback with empty array.
	Callback(TArray<TWeakObjectPtr<UResourceDropOff>>());
}

void UGameResourceManager::AsyncRequestClosestResource(
	const FVector& HarvesterLocation,
	const int32 NumResources,
	const ERTSResourceType ResourceType,
	const TFunction<void(const TArray<TWeakObjectPtr<UResourceComponent>>&)>& Callback)
{
	if (M_GetAsyncResourceThread)
	{
		M_GetAsyncResourceThread->ScheduleResourceRequest(
			HarvesterLocation,
			NumResources,
			ResourceType,
			Callback);
		return;
	}
	// Invoke callback with empty array.
	Callback(TArray<TWeakObjectPtr<UResourceComponent>>());
}


void UGameResourceManager::BeginPlay()
{
	Super::BeginPlay();
	InitAsyncResourceThread();
}

void UGameResourceManager::BeginDestroy()
{
	ShutDownAsyncResourceThread();
	Super::BeginDestroy();
}

void UGameResourceManager::InitAsyncResourceThread()
{
	if (not M_GetAsyncResourceThread)
	{
		M_GetAsyncResourceThread = new FGetAsyncResource();
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				M_UpdateDropOffDataHandle,
				this,
				&UGameResourceManager::UpdateDropOffDataOnAsyncThread,
				M_UpdateDropOffDataInterval,
				true);
			World->GetTimerManager().SetTimer(
				M_UpdateResourcesDataHandle,
				this,
				&UGameResourceManager::UpdateResourcesDataAsyncThread,
				M_UpdateResourcesDataInterval,
				true);
			World->GetTimerManager().SetTimer(
				M_CleanInvalidPointersInterval,
				this,
				&UGameResourceManager::CleanUpInvalidResourcesAndDropOffs,
				DeveloperSettings::Optimization::CleanUp::ResourceMngrCleanUpInterval,
				true);
		}
	}
}

void UGameResourceManager::ShutDownAsyncResourceThread()
{
	if (M_GetAsyncResourceThread)
	{
		delete M_GetAsyncResourceThread;
		M_GetAsyncResourceThread = nullptr;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_UpdateDropOffDataHandle);
		World->GetTimerManager().ClearTimer(M_UpdateResourcesDataHandle);
		World->GetTimerManager().ClearTimer(M_CleanInvalidPointersInterval);
	}
}

void UGameResourceManager::UpdateDropOffDataOnAsyncThread()
{
	TArray<FAsyncDropOffData> DropOffDataCollection;
	for (const FRegisteredResourceDropOff& Entry : M_ResourceDropOffs)
	{
		const TWeakObjectPtr<UResourceDropOff>& EachDropOff = Entry.DropOff;
		if (EachDropOff.IsValid())
		{
			FAsyncDropOffData DropOffData;
			DropOffData.DropOff = EachDropOff;
			DropOffData.OwningPlayer = Entry.OwningPlayer;
			DropOffData.DropOffLocation = EachDropOff->GetDropOffLocationNotThreadSafe();
			DropOffData.ResourceDropOffCapacity = EachDropOff->GetResourceDropOffCapacity();
			DropOffData.bIsActiveDropOff = EachDropOff->GetIsDropOffActive();
			DropOffDataCollection.Add(DropOffData);
		}
	}
	if (DropOffDataCollection.Num() && M_GetAsyncResourceThread)
	{
		M_GetAsyncResourceThread->ScheduleDropOffDataUpdate(DropOffDataCollection);
	}
}

void UGameResourceManager::UpdateResourcesDataAsyncThread()
{
	TArray<FAsyncResourceData> ResourceDataCollection;
	for (auto EachResourceData : M_MapResources)
	{
		if (EachResourceData.IsValid())
		{
			FAsyncResourceData ResourceData;
			ResourceData.Resource = EachResourceData;
			ResourceData.ResourceType = EachResourceData->GetResourceType();
			ResourceData.ResourceLocation = EachResourceData->GetResourceLocationNotThreadSafe();
			ResourceData.bStillContainsResources = EachResourceData->StillContainsResources();
			ResourceDataCollection.Add(ResourceData);
		}
	}
	if (ResourceDataCollection.Num() && M_GetAsyncResourceThread)
	{
		M_GetAsyncResourceThread->ScheduleResourceDataUpdate(ResourceDataCollection);
	}
}

bool UGameResourceManager::GetIsValidPlayerResourceManager()
{
	if (M_PlayerResourceManager.IsValid())
	{
		return true;
	}
	M_PlayerResourceManager = FRTS_Statics::GetPlayerResourceManager(this);
	if (M_PlayerResourceManager.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(
		"PlayerResourceManager is not valid in UGameResourceManager::GetIsValidPlayerResourceManager"
		"\n After attempting to repair the reference with FRTS_Statics::GetPlayerResourceManager");
	return false;
}

void UGameResourceManager::RegisterDropOffWithPlayerResourceManager(const bool bRegister,
                                                                    const TWeakObjectPtr<UResourceDropOff> DropOff)
{
	if (not GetIsValidPlayerResourceManager())
	{
		return;
	}
	M_PlayerResourceManager->RegisterDropOff(DropOff, bRegister);
}

void UGameResourceManager::CleanUpInvalidResourcesAndDropOffs()
{
	M_ResourceDropOffs.RemoveAll([](const FRegisteredResourceDropOff& Entry)
	{
		return not Entry.DropOff.IsValid();
	});
	TArray<TWeakObjectPtr<UResourceComponent>> M_ValidResources;
	for (auto EachResource : M_MapResources)
	{
		if (EachResource.IsValid())
		{
			M_ValidResources.Add(EachResource);
		}
	}
	M_MapResources = M_ValidResources;
}
