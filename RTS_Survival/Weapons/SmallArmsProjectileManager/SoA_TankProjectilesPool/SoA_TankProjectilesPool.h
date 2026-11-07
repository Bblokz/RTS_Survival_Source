#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "SoA_TankProjectilesPool.generated.h"


class ASmallArmsProjectileManager;
class AProjectile;
class URTSCameraShakeSubsystem;

USTRUCT()
struct FSoA_TankProjectilesPool
{
	GENERATED_BODY()

	static constexpr int32 PoolSize = DeveloperSettings::GamePlay::Projectile::TankProjectilePoolSize;

	FSoA_TankProjectilesPool();

	AProjectile* GetDormantProjectile();


	// Array that keeps track of whether each projectile is active/used.
	TArray<bool> bIsActive;

	UPROPERTY()
	TArray<AProjectile*> Projectiles;

	void CreateProjectilesForPool(
		ASmallArmsProjectileManager* ProjectileOwner,
		UWorld* World,
		const TSubclassOf<AProjectile>& ProjectileClass,
		const FVector& CreateLocation);

	void SetCameraShakeSubsystemForProjectiles(URTSCameraShakeSubsystem* CameraShakeSubsystem);

	/**
	 * @brief Barrage shells and every tank gun share this pool; a big barrage can hold all PoolSize slots while its
	 * shells fly, and a shot without a slot fails. Adds dormant projectiles instead.
	 * @param ProjectileOwner The manager the projectiles return to.
	 * @param World World to spawn in.
	 * @param ProjectileClass Class of the pool's projectiles.
	 * @param CreateLocation Where the dormant projectiles wait.
	 * @param AmountToAdd How many slots to add.
	 * @param CameraShakeSubsystem Given to the new projectiles like to the first ones; may be null.
	 * @return How many projectiles were added.
	 */
	int32 GrowPool(ASmallArmsProjectileManager* ProjectileOwner, UWorld* World,
	               const TSubclassOf<AProjectile>& ProjectileClass, const FVector& CreateLocation, int32 AmountToAdd,
	               URTSCameraShakeSubsystem* CameraShakeSubsystem);

private:
	AProjectile* SpawnDormantProjectile(ASmallArmsProjectileManager* ProjectileOwner, UWorld* World,
	                                    const TSubclassOf<AProjectile>& ProjectileClass, const FVector& CreateLocation,
	                                    int32 Index) const;
};
