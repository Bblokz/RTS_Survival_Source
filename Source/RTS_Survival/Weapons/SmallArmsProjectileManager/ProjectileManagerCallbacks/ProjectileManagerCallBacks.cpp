#include "ProjectileManagerCallBacks.h"

#include "RTS_Survival/Weapons/SmallArmsProjectileManager/SmallArmsProjectileManager.h"



void FProjectileManagerCallBacks::CallbackOnProjectileMgrReady(
	TFunction<void(const TObjectPtr<ASmallArmsProjectileManager>&)> Callback, TWeakObjectPtr<UObject> WeakCallbackOwner)
{
	if (!WeakCallbackOwner.IsValid())
	{
		return;
	}

	// If the projectile manager is already loaded, invoke the callback immediately.
	if (IsValid(ProjectileManager.Get()))
	{
		Callback(ProjectileManager);
		return;
	}

	// Otherwise, add a lambda to the delegate for later execution.
	OnProjectileManagerLoaded.AddLambda(
		[WeakCallbackOwner, Callback](const TObjectPtr<ASmallArmsProjectileManager>& Manager)
		{
			if (WeakCallbackOwner.IsValid())
			{
				Callback(Manager);
			}
		});
}
