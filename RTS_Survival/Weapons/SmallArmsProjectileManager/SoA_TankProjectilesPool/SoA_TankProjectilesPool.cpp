#include "SoA_TankProjectilesPool.h"

#include "RTS_Survival/Weapons/Projectile/Projectile.h"
#include "RTS_Survival/Weapons/SmallArmsProjectileManager/SmallArmsProjectileManager.h"
#include "RTS_Survival/Subsystems/CameraShakeSubsystem/RTSCameraShakeSubsystem.h"


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
	for (int32 i = 0; i < PoolSize; ++i)
	{
		if (not Projectiles.IsValidIndex(i))
		{
			RTSFunctionLibrary::ReportError("Something went wrong with init the projectile pool, index is invalid"
				"\n Index: " + FString::FromInt(i));
			return;
		}
		Projectiles[i] = SpawnDormantProjectile(ProjectileOwner, World, ProjectileClass, CreateLocation, i);
	}
}

AProjectile* FSoA_TankProjectilesPool::SpawnDormantProjectile(ASmallArmsProjectileManager* ProjectileOwner,
                                                              UWorld* World,
                                                              const TSubclassOf<AProjectile>& ProjectileClass,
                                                              const FVector& CreateLocation, const int32 Index) const
{
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AProjectile* SpawnedProjectile = World->SpawnActor<AProjectile>(
		ProjectileClass,
		CreateLocation,
		FRotator::ZeroRotator,
		SpawnParameters);
	if (not IsValid(SpawnedProjectile))
	{
		RTSFunctionLibrary::ReportError("Failed to spawn a tank projectile for the pool, index: " + FString::FromInt(Index));
		return nullptr;
	}
	SpawnedProjectile->SetOwner(ProjectileOwner);
	// Setup the manager and index on the projectile.
	SpawnedProjectile->InitProjectilePoolSettings(ProjectileOwner, Index);
	// Hide and deactivate movement component.
	SpawnedProjectile->OnCreatedInPoolSetDormant();
	return SpawnedProjectile;
}

int32 FSoA_TankProjectilesPool::GrowPool(ASmallArmsProjectileManager* ProjectileOwner, UWorld* World,
                                         const TSubclassOf<AProjectile>& ProjectileClass,
                                         const FVector& CreateLocation, const int32 AmountToAdd,
                                         URTSCameraShakeSubsystem* CameraShakeSubsystem)
{
	if (not IsValid(World) || not IsValid(ProjectileClass))
	{
		return 0;
	}
	int32 Added = 0;
	for (int32 Step = 0; Step < AmountToAdd; ++Step)
	{
		const int32 Index = Projectiles.Num();
		AProjectile* SpawnedProjectile = SpawnDormantProjectile(ProjectileOwner, World, ProjectileClass, CreateLocation,
		                                                        Index);
		if (not IsValid(SpawnedProjectile))
		{
			break;
		}
		if (CameraShakeSubsystem)
		{
			SpawnedProjectile->SetCameraShakeSubsystem(CameraShakeSubsystem);
		}
		// Both arrays grow together: the returned index frees bIsActive[Index].
		Projectiles.Add(SpawnedProjectile);
		bIsActive.Add(false);
		++Added;
	}
	return Added;
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
	// The pool can grow past PoolSize (GrowPool); both arrays always have the same length.
	const int32 SlotCount = FMath::Min(bIsActive.Num(), Projectiles.Num());
	for (int32 i = 0; i < SlotCount; ++i)
	{
		if (not bIsActive[i] && IsValid(Projectiles[i]))
		{
			bIsActive[i] = true;
			return Projectiles[i];
		}
	}
	return nullptr;
}


void FSoA_TankProjectilesPool::SetCameraShakeSubsystemForProjectiles(URTSCameraShakeSubsystem* CameraShakeSubsystem)
{
	if (not CameraShakeSubsystem)
	{
		RTSFunctionLibrary::ReportError("SetCameraShakeSubsystemForProjectiles received an invalid subsystem.");
		return;
	}

	for (AProjectile* EachProjectile : Projectiles)
	{
		if (not EachProjectile)
		{
			continue;
		}

		EachProjectile->SetCameraShakeSubsystem(CameraShakeSubsystem);
	}
}
