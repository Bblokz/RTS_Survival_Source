#include "SoA_TankProjectilesPool.h"

#include "RTS_Survival/Weapons/Projectile/Projectile.h"
#include "RTS_Survival/Weapons/SmallArmsProjectileManager/SmallArmsProjectileManager.h"


void FSoA_TankProjectilesPool::CreateProjectilesForPool(
	ASmallArmsProjectileManager* ProjectileOwner,
	UWorld* World,
	const TSubclassOf<AProjectile>& ProjectileClass,
	const FVector& CreateLocation)
{
	if (not IsValid(World) || not IsValid(ProjectileClass))
	{
		RTSFunctionLibrary::ReportError("Cannot fill tank projectile pool as world or class is invalid");
		return;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;


	for (int32 i = 0; i < PoolSize; ++i)
	{
		if (not Projectiles.IsValidIndex(i))
		{
			RTSFunctionLibrary::ReportError("Something went wrong with init the projectile pool, index is invalid"
				"\n Index: " + FString::FromInt(i));
			return;
		}
		// Spawn the projectile with the adjusted launch rotation.
		AProjectile* SpawnedProjectile = World->SpawnActor<AProjectile>(
			ProjectileClass,
			CreateLocation,
			FRotator::ZeroRotator,
			SpawnParameters);
		SpawnedProjectile->SetOwner(ProjectileOwner);
		// Setup the manager and index on the projectile.
		SpawnedProjectile->InitProjectilePoolSettings(ProjectileOwner, i);
		// Hide and deactivate movement component.
		SpawnedProjectile->OnCreatedInPoolSetDormant();
		Projectiles[i] = SpawnedProjectile;
	}
}

FSoA_TankProjectilesPool::FSoA_TankProjectilesPool()
{
	for (int32 i = 0; i < PoolSize; ++i)
	{
		bIsActive.Add(false);
		Projectiles.Add(nullptr);
	}
}

AProjectile* FSoA_TankProjectilesPool::GetDormantProjectile()
{
	for(int32 i = 0; i < PoolSize; ++i)
	{
		if (not bIsActive[i])
		{
			bIsActive[i] = true;
			return Projectiles[i];
		}
	}
	return nullptr;
}
