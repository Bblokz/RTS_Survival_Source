#include "FGetAsyncResource.h"
#include "Async/Async.h"
#include "AsyncDropOffData/FAsyncResourceDropOffData.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Resources/HarvesterCargoSlot/HarvesterCargoSlot.h"

FGetAsyncResource::FGetAsyncResource()
	: bM_StopThread(false)
{
	M_Thread = FRunnableThread::Create(this, TEXT("AsyncGetResourceThread"));
}

FGetAsyncResource::~FGetAsyncResource()
{
	if (M_Thread)
	{
		M_Thread->Kill();
		delete M_Thread;
		M_Thread = nullptr;
	}
}

void FGetAsyncResource::ScheduleDropOffDataUpdate(const TArray<FAsyncDropOffData>& DropOffData)
{
	// Const reference lvalue DropOffData is copied to rvalue DropOffData.
	M_PendingDropOffDataUpdates.Enqueue(DropOffData);
}

void FGetAsyncResource::ScheduleResourceDataUpdate(const TArray<FAsyncResourceData>& ResourceData)
{
	M_PendingResourceDataUpdates.Enqueue(ResourceData);
}

bool FGetAsyncResource::Init()
{
	return true;
}

uint32 FGetAsyncResource::Run()
{
	while (!bM_StopThread)
	{
		// Process any pending Resource System data updates
		TArray<FAsyncDropOffData> NewDropOffData;
		while (M_PendingDropOffDataUpdates.Dequeue(NewDropOffData))
		{
			// Casts to rvalue to use move semantics to steal the data from the queue.
			M_DropOffData = MoveTemp(NewDropOffData);
		}
		TArray<FAsyncResourceData> NewResourceData;
		while (M_PendingResourceDataUpdates.Dequeue(NewResourceData))
		{
			M_ResourceData = MoveTemp(NewResourceData);
		}

		ProcessDropOffRequests();
		ProcessResourceRequests();

		// Sleep to prevent tight loop
		FPlatformProcess::Sleep(DeveloperSettings::Async::AsyncGetResourceThreadUpdateInterval);
	}
	return 0;
}

void FGetAsyncResource::ProcessDropOffRequests()
{
	FDropOffRequest Request;
	while (M_DropOffRequestQueue.Dequeue(Request))
	{
		// Build an array to store <distance, dropOffPtr> for sorting:
		TArray<TPair<float, TWeakObjectPtr<UResourceDropOff>>> Candidates;

		for (const FAsyncDropOffData& DropOffData : M_DropOffData)
		{
			if (DropOffData.OwningPlayer != Request.OwningPlayer)
			{
				continue;
			}
			// Skip if DropOff is inactive.
			if(not DropOffData.bIsActiveDropOff)
			{
				continue;
			}
			// Skip of dropOff does not support resource type or is full.
			const bool bSupportsResource = DropOffData.ResourceDropOffCapacity.Contains(Request.ResourceType);
			if (not bSupportsResource ||
				DropOffData.ResourceDropOffCapacity[Request.ResourceType].IsFull())
			{
				continue;
			}

			float Dist = FVector::Dist(Request.HarvesterLocation, DropOffData.DropOffLocation);
			Candidates.Emplace(Dist, DropOffData.DropOff);
		}

		// Sort by distance ascending
		Candidates.Sort([](const TPair<float, TWeakObjectPtr<UResourceDropOff>>& A,
		                   const TPair<float, TWeakObjectPtr<UResourceDropOff>>& B)
		{
			return A.Key < B.Key;
		});

		// Pick the top N = Request.NumDropOffs
		TArray<TWeakObjectPtr<UResourceDropOff>> FoundDropOffs;
		for (int32 i = 0; i < Candidates.Num() && i < Request.NumDropOffs; i++)
		{
			FoundDropOffs.Add(Candidates[i].Value);
		}
		auto RequestCallback = MoveTemp(Request.Callback);

		// Trigger callback back on the Game Thread:
		AsyncTask(ENamedThreads::GameThread, [RequestCallback, FoundDropOffs]()
		{
			RequestCallback(FoundDropOffs);
		});
	}
}

void FGetAsyncResource::ProcessResourceRequests()
{
	FResourceRequest Request;
	while (M_ResourceRequestQueue.Dequeue(Request))
	{
		// Build an array to store <distance, ResourcePointer> for sorting:
		TArray<TPair<float, TWeakObjectPtr<UResourceComponent>>> Candidates;

		for (const FAsyncResourceData& ResourceData : M_ResourceData)
		{
			// Skip resource does not support resource type or is empty.
			const bool bIsNotCorrectType = ResourceData.ResourceType == Request.ResourceType;
			const bool bIsResourceEmpty = not ResourceData.bStillContainsResources;
			if (not bIsNotCorrectType || bIsResourceEmpty)
			{
				continue;
			}
				
			float Dist = FVector::Dist(Request.HarvesterLocation, ResourceData.ResourceLocation);

			Candidates.Emplace(Dist, ResourceData.Resource);
		}

		// Sort by distance ascending
		Candidates.Sort([](const TPair<float, TWeakObjectPtr<UResourceComponent>>& A,
		                   const TPair<float, TWeakObjectPtr<UResourceComponent>>& B)
		{
			return A.Key < B.Key;
		});

		// Pick the top N = Request.NumResources
		TArray<TWeakObjectPtr<UResourceComponent>> FoundResources;
		for (int32 i = 0; i < Candidates.Num() && i < Request.NumResources; i++)
		{
			FoundResources.Add(Candidates[i].Value);
		}
		auto RequestCallback = MoveTemp(Request.Callback);

		// Trigger callback back on the Game Thread:
		AsyncTask(ENamedThreads::GameThread, [RequestCallback, FoundResources]()
		{
			RequestCallback(FoundResources);
		});
	}
}

void FGetAsyncResource::Stop()
{
	bM_StopThread = true;
	FRunnable::Stop();
}

void FGetAsyncResource::ScheduleDropOffRequest(
	const FVector& HarvesterLocation,
	const int32 NumDropOffs,
	const uint8 OwningPlayer,
	const int32 DropOffAmount,
	const ERTSResourceType ResourceType,
	TFunction<void(const TArray<TWeakObjectPtr<UResourceDropOff>>&)>
	Callback)
{
	FDropOffRequest NewRequest;
	NewRequest.RequestID = FPlatformTime::Cycles();
	NewRequest.HarvesterLocation = HarvesterLocation;
	NewRequest.NumDropOffs = NumDropOffs;
	NewRequest.OwningPlayer = OwningPlayer;
	NewRequest.DropOffAmount = DropOffAmount;
	NewRequest.ResourceType = ResourceType;
	NewRequest.Callback = MoveTemp(Callback);

	// Enqueue the request, so it can be processed on Run()
	M_DropOffRequestQueue.Enqueue(MoveTemp(NewRequest));
}

void FGetAsyncResource::ScheduleResourceRequest(
	const FVector& HarvesterLocation,
	const int32 NumResources,
	const ERTSResourceType ResourceType,
	TFunction<void(const TArray<TWeakObjectPtr<UResourceComponent>>&)> Callback)
{
	FResourceRequest NewRequest;
	NewRequest.RequestID = FPlatformTime::Cycles();
	NewRequest.HarvesterLocation = HarvesterLocation;
	NewRequest.NumResources = NumResources;
	NewRequest.ResourceType = ResourceType;
	NewRequest.Callback = MoveTemp(Callback);

	// Enqueue the request, so it can be processed on Run()
	M_ResourceRequestQueue.Enqueue(MoveTemp(NewRequest));
}
