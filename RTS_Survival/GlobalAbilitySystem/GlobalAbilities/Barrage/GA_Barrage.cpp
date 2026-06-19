// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GA_Barrage.h"

#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Player/PlayerAudioController/PlayerAudioController.h"
#include "RTS_Survival/Weapons/Projectile/Projectile.h"
#include "RTS_Survival/Weapons/SmallArmsProjectileManager/SmallArmsProjectileManager.h"

void UGA_Barrage::ExecuteAbilityAtLocation(const FVector& TargetLocation)
{
	ResetBarrageRuntimeState();
	M_PendingTargetLocation = TargetLocation;

	if (HasCachedProjectileManager())
	{
		StartBarrage(TargetLocation);
		return;
	}

	SetupCallbackToProjectileManager(nullptr);
}

void UGA_Barrage::BeginDestroy()
{
	ClearBurstTimer();
	ClearShellLaunchTimers();
	Super::BeginDestroy();
}

void UGA_Barrage::OnInit(AActor* WorldContextActor)
{
	Super::OnInit(WorldContextActor);
	if (not IsValid(WorldContextActor))
	{
		SetupCallbackToProjectileManager(nullptr);
		return;
	}

	SetupCallbackToProjectileManager(WorldContextActor);
}

void UGA_Barrage::SetupCallbackToProjectileManager(const AActor* WorldContextObject)
{
	if (bM_IsWaitingForProjectileManager)
	{
		return;
	}

	ACPPGameState* GameState = IsValid(WorldContextObject)
		                           ? FRTS_Statics::GetGameState(WorldContextObject)
		                           : FRTS_Statics::GetGameState(this);
	if (not GameState)
	{
		return;
	}

	TWeakObjectPtr<UGA_Barrage> WeakThis(this);
	auto Callback = [WeakThis](const TObjectPtr<ASmallArmsProjectileManager>& ProjectileManager)-> void
	{
		if (not WeakThis.IsValid())
		{
			return;
		}
		WeakThis->OnProjectileManagerLoaded(ProjectileManager);
	};
	bM_IsWaitingForProjectileManager = true;
	GameState->RegisterCallbackForSmallArmsProjectileMgr(Callback, this);
}

void UGA_Barrage::OnProjectileManagerLoaded(const TObjectPtr<ASmallArmsProjectileManager>& ProjectileManager)
{
	if (not IsValid(ProjectileManager))
	{
		bM_IsWaitingForProjectileManager = false;
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("ProjectileManager"),
			TEXT("UGA_Barrage::OnProjectileManagerLoaded"),
			this);
		return;
	}
	bM_IsWaitingForProjectileManager = false;
	M_ProjectileManager = ProjectileManager;
	StartBarrage(M_PendingTargetLocation);
}

bool UGA_Barrage::GetIsValidProjectileManager() const
{
	if (M_ProjectileManager.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_ProjectileManager"),
		TEXT("UGA_Barrage::GetIsValidProjectileManager"),
		this);
	return false;
}

void UGA_Barrage::StartBarrage(const FVector& TargetLocation)
{
	M_PendingTargetLocation = TargetLocation;
	FireNextBurst();
}

void UGA_Barrage::FireNextBurst()
{
	if (not GetIsValidProjectileManager())
	{
		return;
	}

	const int32 ShellsRemaining = M_BarrageSettings.AmountShells - M_ShellsLaunched;
	const int32 SafeBurstAmount = FMath::Max(M_BarrageSettings.BurstAmount, 1);
	const int32 ShellsThisBurst = FMath::Min(SafeBurstAmount, ShellsRemaining);
	for (int32 ShellIndex = 0; ShellIndex < ShellsThisBurst; ++ShellIndex)
	{
		FireSingleShell();
	}

	if (M_ShellsLaunched < M_BarrageSettings.AmountShells)
	{
		ScheduleNextBurst();
	}
}

void UGA_Barrage::FireSingleShell()
{
	PlayOffMapLaunchSoundIfRequested();

	if (GetShouldDelayShellLaunchForOffMapSound())
	{
		ScheduleDelayedShellLaunch();
	}
	else
	{
		LaunchSingleShell();
	}

	++M_ShellsLaunched;
}

void UGA_Barrage::LaunchSingleShell()
{
	if (not GetIsValidProjectileManager())
	{
		return;
	}

	AProjectile* Projectile = M_ProjectileManager.Get()->GetDormantTankProjectile();
	if (not IsValid(Projectile))
	{
		RTSFunctionLibrary::ReportError(TEXT("UGA_Barrage failed to obtain a dormant projectile."));
		return;
	}

	const FWeaponData VariedWeaponData = BuildVariedWeaponData();
	FProjectileVfxSettings ProjectileVfxSettings = M_BarrageSettings.ProjectileVfxSettings;
	ProjectileVfxSettings.ShellType = VariedWeaponData.ShellType;
	ProjectileVfxSettings.WeaponCaliber = VariedWeaponData.WeaponCalibre;
	const FVector LaunchLocation = BuildLaunchLocation(M_PendingTargetLocation);
	const FVector AimPoint = BuildRandomPointInRadius(M_PendingTargetLocation);
	const FRotator LaunchRotation = (AimPoint - LaunchLocation).Rotation();
	const TArray<AActor*> ActorsToIgnore;
	const FWeaponVFX WeaponVfxForShellLaunch = BuildWeaponVfxForShellLaunch();

	Projectile->SetupBarrageProjectileForNewLaunch(
		VariedWeaponData,
		WeaponVfxForShellLaunch,
		ProjectileVfxSettings,
		M_ProjectileMover,
		GetOwningPlayer(),
		LaunchLocation,
		LaunchRotation,
		AimPoint,
		ActorsToIgnore);
}

bool UGA_Barrage::GetShouldDelayShellLaunchForOffMapSound() const
{
	return M_BarrageSettings.bUseOffMapLaunchSound
		&& IsValid(GetOffMapLaunchSound())
		&& M_BarrageSettings.TimeBetweenOffMapSoundAndStart > 0.0f;
}

void UGA_Barrage::ScheduleDelayedShellLaunch()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		LaunchSingleShell();
		return;
	}

	TWeakObjectPtr<UGA_Barrage> WeakThis(this);
	FTimerHandle& ShellLaunchTimerHandle = M_ShellLaunchTimerHandles.AddDefaulted_GetRef();
	World->GetTimerManager().SetTimer(
		ShellLaunchTimerHandle,
		[WeakThis]()
		{
			if (not WeakThis.IsValid())
			{
				return;
			}
			WeakThis->LaunchSingleShell();
		},
		M_BarrageSettings.TimeBetweenOffMapSoundAndStart,
		false);
}

FWeaponVFX UGA_Barrage::BuildWeaponVfxForShellLaunch() const
{
	FWeaponVFX WeaponVfxForShellLaunch = M_WeaponVfx;
	if (not M_BarrageSettings.bUseOffMapLaunchSound)
	{
		return WeaponVfxForShellLaunch;
	}

	WeaponVfxForShellLaunch.LaunchSound = nullptr;
	return WeaponVfxForShellLaunch;
}

void UGA_Barrage::PlayOffMapLaunchSoundIfRequested() const
{
	if (not M_BarrageSettings.bUseOffMapLaunchSound)
	{
		return;
	}

	USoundBase* LaunchSound = GetOffMapLaunchSound();
	if (not IsValid(LaunchSound))
	{
		return;
	}

	UPlayerAudioController* PlayerAudioController = FRTS_Statics::GetPlayerAudioController(this);
	if (not IsValid(PlayerAudioController))
	{
		return;
	}

	PlayerAudioController->PlayOffMapAbilitySound(LaunchSound);
}

USoundBase* UGA_Barrage::GetOffMapLaunchSound() const
{
	if (IsValid(M_BarrageSettings.OffMapLaunchSound))
	{
		return M_BarrageSettings.OffMapLaunchSound;
	}

	return M_WeaponVfx.LaunchSound;
}

FVector UGA_Barrage::BuildRandomPointInRadius(const FVector& CenterLocation) const
{
	const FVector2D RandomOffset = FMath::RandPointInCircle(M_BarrageSettings.Radius);
	return CenterLocation + FVector(RandomOffset.X, RandomOffset.Y, 0.0f);
}

FVector UGA_Barrage::BuildLaunchLocation(const FVector& TargetLocation) const
{
	FVector LaunchLocation = BuildRandomPointInRadius(TargetLocation);
	LaunchLocation.Z += FMath::RandRange(M_BarrageSettings.MinHeightStart, M_BarrageSettings.MaxHeightStart);
	return LaunchLocation;
}

FWeaponData UGA_Barrage::BuildVariedWeaponData() const
{
	FWeaponData VariedWeaponData = M_WeaponData;
	if (not M_BarrageSettings.bVariationWeaponCalibre)
	{
		return VariedWeaponData;
	}

	const float VariationMultiplier = FMath::RandRange(
		M_BarrageSettings.OnVariationMinMlt,
		M_BarrageSettings.OnVariationMaxMlt);
	VariedWeaponData.BaseDamage *= VariationMultiplier;
	VariedWeaponData.WeaponCalibre *= VariationMultiplier;
	VariedWeaponData.ArmorPen *= VariationMultiplier;
	VariedWeaponData.ArmorPenMaxRange *= VariationMultiplier;
	return VariedWeaponData;
}

void UGA_Barrage::ScheduleNextBurst()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	const float Delay = FMath::RandRange(
		M_BarrageSettings.MinTimeBetweenBurst,
		M_BarrageSettings.MaxTimeBetweenBurst);
	TWeakObjectPtr<UGA_Barrage> WeakThis(this);
	World->GetTimerManager().SetTimer(
		M_BurstTimerHandle,
		[WeakThis]()
		{
			if (not WeakThis.IsValid())
			{
				return;
			}
			WeakThis->FireNextBurst();
		},
		Delay,
		false);
}


void UGA_Barrage::ClearBurstTimer()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	World->GetTimerManager().ClearTimer(M_BurstTimerHandle);
}

void UGA_Barrage::ClearShellLaunchTimers()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		M_ShellLaunchTimerHandles.Reset();
		return;
	}

	for (FTimerHandle& ShellLaunchTimerHandle : M_ShellLaunchTimerHandles)
	{
		World->GetTimerManager().ClearTimer(ShellLaunchTimerHandle);
	}
	M_ShellLaunchTimerHandles.Reset();
}

void UGA_Barrage::ResetBarrageRuntimeState()
{
	ClearBurstTimer();
	ClearShellLaunchTimers();
	M_ShellsLaunched = 0;
}

bool UGA_Barrage::HasCachedProjectileManager() const
{
	return M_ProjectileManager.Get() != nullptr;
}
