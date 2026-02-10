#pragma once

#include "CoreMinimal.h"
#include "Delegates/DelegateCombinations.h"
#include "ProjectileManagerCallBacks.generated.h"

class ASmallArmsProjectileManager;

// Delegate that takes a projectile manager pointer when fired.
DECLARE_MULTICAST_DELEGATE_OneParam(FOnProjectileManagerLoaded, const TObjectPtr<ASmallArmsProjectileManager>&);

/**
 * Keeps track of whether the projectile manager was already loaded.
 * Use CallbackOnProjectileMgrReady to register a callback. If the manager is already valid, the callback
 * is invoked immediately.
 */
USTRUCT()
struct FProjectileManagerCallBacks
{
	GENERATED_BODY()

	// Friend class so that the game state (or any other owner) can set the projectile manager.
	friend class ACPPGameState;  
	
public:
	/**
	 * @brief Register a callback that will be invoked when the projectile manager is ready.
	 * If the projectile manager is already valid, the callback is invoked immediately.
	 *
	 * @param Callback A function to call when the projectile manager is ready.
	 * @param WeakCallbackOwner A weak object pointer to the owner of the callback.
	 */
	void CallbackOnProjectileMgrReady(TFunction<void(const TObjectPtr<ASmallArmsProjectileManager>&)> Callback, TWeakObjectPtr<UObject> WeakCallbackOwner);

	// Templated overload to accept member function pointers directly.
	template <typename T>
	void CallbackOnProjectileMgrReady(void (T::*InCallback)(const TObjectPtr<ASmallArmsProjectileManager>&), T* InCallbackOwner)
	{
		const TWeakObjectPtr<T> WeakCallbackOwner = InCallbackOwner;
		TFunction<void(const TObjectPtr<ASmallArmsProjectileManager>&)> BoundCallback = [WeakCallbackOwner, InCallback](const TObjectPtr<ASmallArmsProjectileManager>& Manager)
		{
			if (not WeakCallbackOwner.IsValid())
			{
				return;
			}

			T* StrongCallbackOwner = WeakCallbackOwner.Get();
			(StrongCallbackOwner->*InCallback)(Manager);
		};
		CallbackOnProjectileMgrReady(BoundCallback, InCallbackOwner);
	}

private:
	// Delegate called when the projectile manager is loaded.
	FOnProjectileManagerLoaded OnProjectileManagerLoaded;

	// pointer to the projectile manager.
	TObjectPtr<ASmallArmsProjectileManager> ProjectileManager;




};
