// Copyright (C) Bas Blokzijl - All rights reserved.

#include "ProjectileMapTestHooks.h"

#if RTS_WITH_MAP_TESTS

#include "RTS_Survival/RTSComponents/HealthComponent.h"

namespace ProjectileMapTestHooksPrivate
{
	FOnMapTestArmorHit ArmorHitDelegate;
	FOnMapTestShellLaunched ShellLaunchedDelegate;
	uint32 LastShellLaunchId = 0;
	// Pooled projectiles are reused, so the id belongs to the projectile's current flight.
	TMap<TWeakObjectPtr<const UObject>, uint32> ShellLaunchIdByProjectile;
}

FOnMapTestArmorHit& FProjectileMapTestHooks::OnArmorHit()
{
	return ProjectileMapTestHooksPrivate::ArmorHitDelegate;
}

FOnMapTestShellLaunched& FProjectileMapTestHooks::OnShellLaunched()
{
	return ProjectileMapTestHooksPrivate::ShellLaunchedDelegate;
}

bool FProjectileMapTestHooks::GetHasListeners()
{
	using namespace ProjectileMapTestHooksPrivate;
	return ArmorHitDelegate.IsBound() || ShellLaunchedDelegate.IsBound();
}

void FProjectileMapTestHooks::ReportShellLaunched(const UObject* Projectile, UObject* WeaponOwner,
                                                  const EWeaponName WeaponName)
{
	using namespace ProjectileMapTestHooksPrivate;
	if (not GetHasListeners() || Projectile == nullptr)
	{
		return;
	}
	FMapTestShellLaunchReport Report;
	Report.ShellLaunchId = ++LastShellLaunchId;
	Report.WeaponOwner = WeaponOwner;
	Report.WeaponName = WeaponName;
	ShellLaunchIdByProjectile.Add(Projectile, Report.ShellLaunchId);
	ShellLaunchedDelegate.Broadcast(Report);
}

uint32 FProjectileMapTestHooks::GetShellLaunchId(const UObject* Projectile)
{
	const uint32* const LaunchId = ProjectileMapTestHooksPrivate::ShellLaunchIdByProjectile.Find(Projectile);
	return LaunchId != nullptr ? *LaunchId : 0;
}

float FProjectileMapTestHooks::GetHealthOrZero(const AActor* Actor)
{
	if (not IsValid(Actor))
	{
		return 0.f;
	}
	const UHealthComponent* const HealthComponent = Actor->FindComponentByClass<UHealthComponent>();
	return IsValid(HealthComponent) ? HealthComponent->GetCurrentHealth() : 0.f;
}

#endif
