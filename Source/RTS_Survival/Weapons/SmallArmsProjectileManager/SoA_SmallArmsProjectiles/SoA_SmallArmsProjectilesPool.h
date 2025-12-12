#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/DeveloperSettings.h"

struct FSoA_SmallArmsProjectilesPool
{
	static constexpr int32 PoolSize = DeveloperSettings::GamePlay::Projectile::SmallArmsProjectilePoolSize;

	// Array that keeps track of whether each projectile is active/used.
	TArray<bool> bIsActive;

	// Array that keeps track of the positions of projectiles.
	TArray<FVector> Positions;

	// Array that keeps track of the end positions of projectiles.
	TArray<FVector> EndPositions;

	// Array that keeps track of the type of the projectile on X, the start time on Y and the lifetime on Z.
	TArray<FVector> ProjectileTypesStartTimeLifeTime;

	FSoA_SmallArmsProjectilesPool()
	{
		
		for (int32 i = 0; i < PoolSize; ++i)
		{
			bIsActive.Add(false);
			Positions.Add(FVector::ZeroVector);
			EndPositions.Add(FVector::ZeroVector);
			ProjectileTypesStartTimeLifeTime.Add(FVector::ZeroVector);
		}
	}
};
