#include "SquadControllerDataCallback.h"

void FSquadDataCallbacks::CallbackOnSquadDataLoaded(TFunction<void()> Callback,
	TWeakObjectPtr<UObject> WeakCallbackOwner)
{
	if ( not WeakCallbackOwner.IsValid())
	{
		return;
	}
	// If data already valid invoke immediately.
	if(bM_IsDataLoaded)
	{
		Callback();
		return;
	}
	// Otherwise, add the lambda to the delegate for later execution.
	OnSquadDataReady.AddLambda([WeakCallbackOwner, Callback]()->void
	{
		if (WeakCallbackOwner.IsValid())
		{
			Callback();
		}
	});
}

void FSquadDataCallbacks::SetDataLoaded(const bool bLoaded)
{
	bM_IsDataLoaded = bLoaded;
}
