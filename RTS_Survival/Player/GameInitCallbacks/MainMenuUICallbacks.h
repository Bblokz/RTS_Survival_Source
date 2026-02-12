#pragma once


#include "CoreMinimal.h"
#include "Delegates/DelegateCombinations.h"
#include "MainMenuUICallbacks.generated.h"
class ACPPController;
DECLARE_MULTICAST_DELEGATE(FOnMainMenuUIReady);

// Keeps track of whether the main game ui was already loaded. Use CallbackOnMenuReady to register a callback.
// If the menu was already loaded we invoke the callback immediately.
USTRUCT()
struct FMainMenuUICallbacks
{
	GENERATED_BODY()

	friend class ACPPController;


	/**
	 * @brief Register a callback that will be invoked when the main menu is ready.
	 * If the main menu is already valid, the callback is invoked immediately.
	 */
	void CallbackOnMenuReady(TFunction<void()> Callback, TWeakObjectPtr<UObject> WeakCallbackOwner);

	// Templated overload to accept member function pointers directly.
	template <typename T>
	void CallbackOnMenuReady(void (T::*InCallback)(), T* InCallbackOwner)
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
		CallbackOnMenuReady(BoundCallback, InCallbackOwner);
	}

private:

	void SetPlayerController(ACPPController* Controller);
	
	// Delegate called when the main ui is loaded.
	FOnMainMenuUIReady OnMainMenuUIReady;

	TWeakObjectPtr<ACPPController> PlayerController;

	bool EnsurePlayerControllerIsValid() const;
};
