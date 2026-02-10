#pragma once

#include "CoreMinimal.h"

#include "OnArchiveLoadedCallbacks.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnArchiveReady);
class RTS_SURVIVAL_API UMainGameUI;

USTRUCT()
struct FOnArchiveLoadedCallbacks
{
	GENERATED_BODY()

	friend class UMainGameUI;

	/**
	 * @brief Register a callback that will be invoked when the Archive is ready.
	 * If the Archive is already valid, the callback is invoked immediately.
	 */
	void CallbackOnArchiveReady(TFunction<void()> Callback, TWeakObjectPtr<UObject> WeakCallbackOwner);

	// Templated overload to accept member function pointers directly.
	template <typename T>
	void CallbackOnArchiveReady(void (T::*InCallback)(), T* InCallbackOwner)
	{
		const TWeakObjectPtr<T> WeakCallbackOwner = InCallbackOwner;
		TFunction<void()> BoundCallback = [WeakCallbackOwner, InCallback]()
		{
			if (not WeakCallbackOwner.IsValid())
			{
				return;
			}

			T* StrongCallbackOwner = WeakCallbackOwner.Get();
			(StrongCallbackOwner->*InCallback)();
		};
		CallbackOnArchiveReady(BoundCallback, InCallbackOwner);
	}

private:

	void SetMainMenu(UMainGameUI* MainMenu);
	
	// Delegate called when the Archive is loaded.
	FOnArchiveReady OnArchiveReady;

	TWeakObjectPtr<UMainGameUI> M_MainGameUI;

	bool EnsureMainGameUIIsValid() const;
};
