#pragma once


#include "CoreMinimal.h"
#include "Delegates/DelegateCombinations.h"
#include "SquadControllerDataCallback.generated.h"
DECLARE_MULTICAST_DELEGATE(FOnSquadControllerDataReady);

// Keeps track of whether the squad controller data is loaded. Use CallbackOnSquadDataReady to register a callback.
// If the SquadData was already loaded invoke the callback immediately.
USTRUCT()
struct FSquadDataCallbacks
{
	GENERATED_BODY()

	friend class ASquadController;

	/**
	 * @brief Register a callback that will be invoked when the data in the squad controller ready.
	 * If the data is already loaded the callback is invoked immediately.
	 */
	void CallbackOnSquadDataLoaded(TFunction<void()> Callback, TWeakObjectPtr<UObject> WeakCallbackOwner);

	// Templated overload to accept member function pointers directly.
	template <typename T>
	void CallbackOnSquadDataLoaded(void (T::*InCallback)(), T* InCallbackOwner)
	{
		TFunction<void()> BoundCallback = [InCallbackOwner, InCallback]()
		{
			(InCallbackOwner->*InCallback)();
		};
		CallbackOnSquadDataLoaded(BoundCallback, InCallbackOwner);
	}

private:

	void SetDataLoaded(const bool bLoaded);
	// Delegate called when the main ui is loaded.
	FOnSquadControllerDataReady OnSquadDataReady;

	bool bM_IsDataLoaded = false;

};
