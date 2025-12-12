#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "SoA_TankProjectilesPool.generated.h"


class ASmallArmsProjectileManager;
class AProjectile;

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
};
