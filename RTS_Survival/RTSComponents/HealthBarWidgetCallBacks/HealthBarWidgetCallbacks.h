// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delegates/DelegateCombinations.h"
#include "HealthBarWidgetCallbacks.generated.h"

class UHealthComponent;
class UWidgetComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnHealthBarWidgetReady, UHealthComponent* /*HealthComp*/);

/**
 * @brief Keeps track of whether the HealthBar WidgetComponent has been created.
 * Allows other systems to register callbacks that fire once the widget exists.
 * If the widget already exists at registration time, the callback fires immediately.
 */
USTRUCT()
struct FHealthBarWidgetCallbacks
{
	GENERATED_BODY()

	friend class UHealthComponent;

	/**
	 * @brief Register a callback that is invoked when the health bar widget is ready.
	 * If it already exists, the callback fires immediately.
	 * @param Callback The function to call when ready.
	 * @param WeakCallbackOwner Used to prevent dangling callbacks on destroyed owners.
	 */
	void CallbackOnHealthBarReady(TFunction<void(UHealthComponent* HealthComp)> Callback,
	                              TWeakObjectPtr<UObject> WeakCallbackOwner);

	// Templated overload to directly bind member functions.
	template <typename T>
	void CallbackOnHealthBarReady(void (T::*InCallback)(UHealthComponent*), T* InCallbackOwner)
	{
		TFunction<void(UHealthComponent*)> BoundCallback = [InCallbackOwner, InCallback](UHealthComponent* HealthComp)
		{
			(InCallbackOwner->*InCallback)(HealthComp);
		};
		CallbackOnHealthBarReady(BoundCallback, InCallbackOwner);
	}

private:
	void SetOwningHealthComponent(UHealthComponent* InHealthComp);

	bool EnsureHealthComponentIsValid() const;

	/** Delegate triggered when the widget is ready. */
	FOnHealthBarWidgetReady OnHealthBarWidgetReady;

	/** Weak ref to owning health component. */
	TWeakObjectPtr<UHealthComponent> M_HealthComponent;
};
