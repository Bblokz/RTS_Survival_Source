#pragma once

#include "CoreMinimal.h"
#include "AsyncDropOffData/FAsyncResourceDropOffData.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Containers/Queue.h"
#include "Delegates/Delegate.h"
#include "Templates/Function.h"

enum class ERTSResourceType : uint8;
class UResourceDropOff;

class FGetAsyncResource : public FRunnable
{
public:
	FGetAsyncResource();
	virtual ~FGetAsyncResource();

	void ScheduleDropOffDataUpdate(const TArray<FAsyncDropOffData>& DropOffData);
	void ScheduleResourceDataUpdate(const TArray<FAsyncResourceData>& ResourceData);

	// FRunnable interface
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;

	void ScheduleDropOffRequest(
		const FVector& HarvesterLocation,
		int32 NumDropOffs,
		uint8 OwningPlayer,
		int32 DropOffAmount,
		ERTSResourceType ResourceType,
		TFunction<void(const TArray<TWeakObjectPtr<UResourceDropOff>>&)>
		Callback
	);

	void ScheduleResourceRequest(
		const FVector& HarvesterLocation,
		int32 NumResources,
		ERTSResourceType ResourceType,
		TFunction<void(const TArray<TWeakObjectPtr<UResourceComponent>>&)>
		Callback
	);

	

private:

	void ProcessDropOffRequests();
	void ProcessResourceRequests();
	
	/** Flag to signal the thread to stop */
	FThreadSafeBool bM_StopThread;

	FRunnableThread* M_Thread;

    struct FDropOffRequest
    {
        uint32 RequestID;
        FVector HarvesterLocation;
        int32 NumDropOffs;
        uint8 OwningPlayer;
        int32 DropOffAmount;
        ERTSResourceType ResourceType;
        TFunction<void(const TArray<TWeakObjectPtr<UResourceDropOff>>&)> Callback;
    };

	struct FResourceRequest
	{
		uint32 RequestID;
		FVector HarvesterLocation;
		int32 NumResources;
		ERTSResourceType ResourceType;
		TFunction<void(const TArray<TWeakObjectPtr<UResourceComponent>>&)> Callback;
	};

	TQueue<FDropOffRequest> M_DropOffRequestQueue;
	TQueue<FResourceRequest> M_ResourceRequestQueue;

	// Array of all drop offs on the map.
	TArray<FAsyncDropOffData> M_DropOffData;

	// Array of all Resources on the map.
	TArray<FAsyncResourceData> M_ResourceData;

	TQueue<TArray<FAsyncDropOffData>> M_PendingDropOffDataUpdates;
	TQueue<TArray<FAsyncResourceData>> M_PendingResourceDataUpdates;
};
