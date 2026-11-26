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
	ResetAbilityEndedBroadcast();
	// A barrage that is still firing keeps firing: every execution is its own run.
	if (HasCachedProjectileManager())
	{
		StartBarrage(TargetLocation);
		return;
	}

	M_TargetsWaitingForProjectileManager.Add(TargetLocation);
	SetupCallbackToProjectileManager(nullptr);
	// The callback may already have run (and started the runs); if it could not be registered at all, the waiting
	// executions would never fire, so they end now.
	if (not bM_IsWaitingForProjectileManager && not M_TargetsWaitingForProjectileManager.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(TEXT("UGA_Barrage could not wait for the projectile manager (no game state)."));
		M_TargetsWaitingForProjectileManager.Reset();
		BroadcastBarrageFinishedIfNeeded();
	}
}

void UGA_Barrage::BeginDestroy()
{
	ClearAllRunTimers();
	M_Runs.Reset();
	M_TargetsWaitingForProjectileManager.Reset();
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
	bM_IsWaitingForProjectileManager = false;
	// Moved out first: starting a run can end it at once and re-enter through the finished broadcast.
	const TArray<FVector> WaitingTargets = MoveTemp(M_TargetsWaitingForProjectileManager);
	M_TargetsWaitingForProjectileManager.Reset();
	if (not IsValid(ProjectileManager))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("ProjectileManager"),
			TEXT("UGA_Barrage::OnProjectileManagerLoaded"),
			this);
		// Those executions can never fire; end them instead of leaving the ability running forever.
		BroadcastBarrageFinishedIfNeeded();
		return;
	}
	M_ProjectileManager = ProjectileManager;
	for (const FVector& TargetLocation : WaitingTargets)
	{
		StartBarrage(TargetLocation);
	}
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
	if (M_BarrageSettings.AmountShells <= 0)
	{
		BroadcastBarrageFinishedIfNeeded();
		return;
	}

	FBarrageRun NewRun;
	NewRun.RunId = M_NextRunId++;
	NewRun.TargetLocation = TargetLocation;
	// One battery far off the map fires the whole barrage, so all its shells come in on about the same bearing.
	NewRun.FlightYawDegrees = FMath::FRandRange(0.0f, 360.0f);
	const int32 RunId = NewRun.RunId;
	M_Runs.Add(MoveTemp(NewRun));
	FireNextBurst(RunId);
}

UGA_Barrage::FBarrageRun* UGA_Barrage::FindRun(const int32 RunId)
{
	return M_Runs.FindByPredicate([RunId](const FBarrageRun& Run)
	{
		return Run.RunId == RunId;
	});
}

void UGA_Barrage::FireNextBurst(const int32 RunId)
{
	const FBarrageRun* Run = FindRun(RunId);
	if (Run == nullptr)
	{
		return;
	}
	if (not GetIsValidProjectileManager())
	{
		// No projectiles to fire: end the run rather than leaving it (and the ability) running forever.
		FinishRun(RunId);
		return;
	}

	const int32 ShellsRemaining = M_BarrageSettings.AmountShells - Run->ShellsLaunched;
	const int32 SafeBurstAmount = FMath::Max(M_BarrageSettings.BurstAmount, 1);
	const int32 ShellsThisBurst = FMath::Min(SafeBurstAmount, ShellsRemaining);
	for (int32 ShellIndex = 0; ShellIndex < ShellsThisBurst; ++ShellIndex)
	{
		// Each shell can finish (and remove) the run, so the run is looked up again by id every time.
		FireSingleShell(RunId);
	}

	Run = FindRun(RunId);
	if (Run != nullptr && Run->ShellsLaunched < M_BarrageSettings.AmountShells)
	{
		ScheduleNextBurst(RunId);
	}
}

void UGA_Barrage::FireSingleShell(const int32 RunId)
{
	FBarrageRun* Run = FindRun(RunId);
	if (Run == nullptr)
	{
		return;
	}
	PlayOffMapLaunchSoundIfRequested();

	++Run->ShellsLaunched;

	if (GetShouldDelayShellLaunchForOffMapSound())
	{
		ScheduleDelayedShellLaunch(RunId);
	}
	else
	{
		LaunchSingleShell(RunId);
	}
}

void UGA_Barrage::LaunchSingleShell(const int32 RunId)
{
	const FBarrageRun* Run = FindRun(RunId);
	if (Run == nullptr)
	{
		return;
	}
	if (not GetIsValidProjectileManager())
	{
		NotifyShellLaunchFinished(RunId);
		return;
	}

	AProjectile* Projectile = M_ProjectileManager.Get()->GetDormantTankProjectile();
	if (not IsValid(Projectile))
	{
		RTSFunctionLibrary::ReportError(TEXT("UGA_Barrage failed to obtain a dormant projectile."));
		NotifyShellLaunchFinished(RunId);
		return;
	}

	const FWeaponData VariedWeaponData = BuildVariedWeaponData();
	FProjectileVfxSettings ProjectileVfxSettings = M_BarrageSettings.ProjectileVfxSettings;
	ProjectileVfxSettings.ShellType = VariedWeaponData.ShellType;
	ProjectileVfxSettings.WeaponCaliber = VariedWeaponData.WeaponCalibre;
	// Barrage shells are always drawn by the tank shell trail (tracer, glow and lingering smoke).
	ProjectileVfxSettings.ProjectileNiagaraSystem = EProjectileNiagaraSystem::TankShell;
	const TArray<AActor*> ActorsToIgnore;
	const FWeaponVFX WeaponVfxForShellLaunch = BuildWeaponVfxForShellLaunch();
	const FBarrageShellFlight Flight = BuildShellFlight(*Run);

	Projectile->SetupBarrageProjectileForNewLaunch(
		VariedWeaponData,
		WeaponVfxForShellLaunch,
		ProjectileVfxSettings,
		Flight,
		GetOwningPlayer(),
		ActorsToIgnore);
	NotifyShellLaunchFinished(RunId);
}

void UGA_Barrage::NotifyShellLaunchFinished(const int32 RunId)
{
	FBarrageRun* Run = FindRun(RunId);
	if (Run == nullptr)
	{
		return;
	}
	++Run->ShellsActuallyLaunched;
	if (Run->ShellsLaunched < M_BarrageSettings.AmountShells
		|| Run->ShellsActuallyLaunched < M_BarrageSettings.AmountShells)
	{
		return;
	}
	FinishRun(RunId);
}

void UGA_Barrage::FinishRun(const int32 RunId)
{
	const int32 RunIndex = M_Runs.IndexOfByPredicate([RunId](const FBarrageRun& Run)
	{
		return Run.RunId == RunId;
	});
	if (not M_Runs.IsValidIndex(RunIndex))
	{
		return;
	}
	ClearRunTimers(M_Runs[RunIndex]);
	M_Runs.RemoveAt(RunIndex);
	BroadcastBarrageFinishedIfNeeded();
}

void UGA_Barrage::BroadcastBarrageFinishedIfNeeded()
{
	// The ability ends when the last running execution has launched all its shells.
	if (not M_Runs.IsEmpty() || not M_TargetsWaitingForProjectileManager.IsEmpty())
	{
		return;
	}

	BroadcastAbilityEnded();
}

bool UGA_Barrage::GetShouldDelayShellLaunchForOffMapSound() const
{
	return M_BarrageSettings.bUseOffMapLaunchSound
		&& IsValid(GetOffMapLaunchSound())
		&& M_BarrageSettings.TimeBetweenOffMapSoundAndStart > 0.0f;
}

void UGA_Barrage::ScheduleDelayedShellLaunch(const int32 RunId)
{
	UWorld* World = GetWorld();
	FBarrageRun* Run = FindRun(RunId);
	if (not IsValid(World) || Run == nullptr)
	{
		LaunchSingleShell(RunId);
		return;
	}

	TWeakObjectPtr<UGA_Barrage> WeakThis(this);
	FTimerHandle ShellLaunchTimerHandle;
	World->GetTimerManager().SetTimer(
		ShellLaunchTimerHandle,
		[WeakThis, RunId]()
		{
			if (not WeakThis.IsValid())
			{
				return;
			}
			WeakThis->LaunchSingleShell(RunId);
		},
		M_BarrageSettings.TimeBetweenOffMapSoundAndStart,
		false);
	Run->ShellLaunchTimerHandles.Add(ShellLaunchTimerHandle);
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

FVector UGA_Barrage::BuildGroundAimPoint(const FVector& TargetLocation) const
{
	FVector AimPoint = BuildRandomPointInRadius(TargetLocation);
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return AimPoint;
	}
	// The area is not flat: land each shell on the ground under its own point, not at the height of the centre.
	constexpr float GroundTraceHalfHeightCm = 5000.0f;
	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldStatic);
	FHitResult GroundHit;
	const FVector TraceStart = AimPoint + FVector::UpVector * GroundTraceHalfHeightCm;
	const FVector TraceEnd = AimPoint - FVector::UpVector * GroundTraceHalfHeightCm;
	if (World->LineTraceSingleByObjectType(GroundHit, TraceStart, TraceEnd, ObjectQuery))
	{
		AimPoint.Z = GroundHit.ImpactPoint.Z;
	}
	return AimPoint;
}

FBarrageShellFlight UGA_Barrage::BuildShellFlight(const FBarrageRun& Run) const
{
	using namespace DeveloperSettings::GamePlay::Projectile;
	constexpr float LowestDescentAngleDegrees = 10.0f;
	constexpr float HighestDescentAngleDegrees = 89.0f;
	constexpr float LowestImpactSpeed = 1000.0f;
	constexpr float ShortestFlightTimeSeconds = 0.2f;
	const FBarrageProjectileMover& Mover = M_ProjectileMover;
	const float MinAngle = FMath::Min(Mover.MinDescentAngleDegrees, Mover.MaxDescentAngleDegrees);
	const float MaxAngle = FMath::Max(Mover.MinDescentAngleDegrees, Mover.MaxDescentAngleDegrees);
	const float DescentAngle = FMath::Clamp(FMath::FRandRange(MinAngle, MaxAngle),
	                                        LowestDescentAngleDegrees, HighestDescentAngleDegrees);
	const float FlightYaw = Run.FlightYawDegrees
		+ FMath::FRandRange(-Mover.BearingSpreadDegrees, Mover.BearingSpreadDegrees);
	const float ImpactSpeed = FMath::Max(Mover.ImpactSpeed, LowestImpactSpeed) * GlobalProjectileSpeedMlt;
	const float GravityScale = FMath::Max(Mover.GravityMultiplier, 0.0f) * GlobalArcGravityMlt;
	const UWorld* World = GetWorld();
	const float WorldGravityZ = IsValid(World) ? World->GetGravityZ() : 0.0f;
	return BarrageShellFlight::Solve(
		BuildGroundAimPoint(Run.TargetLocation),
		FlightYaw,
		DescentAngle,
		ImpactSpeed,
		FMath::Max(Mover.FlightTimeSeconds, ShortestFlightTimeSeconds),
		WorldGravityZ,
		GravityScale);
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

void UGA_Barrage::ScheduleNextBurst(const int32 RunId)
{
	UWorld* World = GetWorld();
	FBarrageRun* Run = FindRun(RunId);
	if (not IsValid(World) || Run == nullptr)
	{
		return;
	}

	const float Delay = FMath::RandRange(
		M_BarrageSettings.MinTimeBetweenBurst,
		M_BarrageSettings.MaxTimeBetweenBurst);
	TWeakObjectPtr<UGA_Barrage> WeakThis(this);
	World->GetTimerManager().SetTimer(
		Run->BurstTimerHandle,
		[WeakThis, RunId]()
		{
			if (not WeakThis.IsValid())
			{
				return;
			}
			WeakThis->FireNextBurst(RunId);
		},
		Delay,
		false);
}

void UGA_Barrage::ClearRunTimers(FBarrageRun& Run) const
{
	UWorld* World = GetWorld();
	if (IsValid(World))
	{
		World->GetTimerManager().ClearTimer(Run.BurstTimerHandle);
		for (FTimerHandle& ShellLaunchTimerHandle : Run.ShellLaunchTimerHandles)
		{
			World->GetTimerManager().ClearTimer(ShellLaunchTimerHandle);
		}
	}
	Run.ShellLaunchTimerHandles.Reset();
}

void UGA_Barrage::ClearAllRunTimers()
{
	for (FBarrageRun& Run : M_Runs)
	{
		ClearRunTimers(Run);
	}
}

bool UGA_Barrage::HasCachedProjectileManager() const
{
	return M_ProjectileManager.Get() != nullptr;
}
