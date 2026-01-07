
// Copyright (C) Bas Blokzijl - All rights reserved.

#include "AircraftWeapon.h"

#include "Kismet/KismetMathLibrary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/FlameThrowerWeapon/UWeaponStateFlameThrower.h"
#include "RTS_Survival/Weapons/SmallArmsProjectileManager/SmallArmsProjectileManager.h"
#include "RTS_Survival/Weapons/LaserWeapon/UWeaponStateLaser.h"
#include "RTS_Survival/Weapons/LaserWeapon/UWeaponStateMultiHitLaser.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"
#include "RTS_Survival/Weapons/WeaponData/MultiTraceWeapon/WeaponStateMultiTrace.h"

UAircraftWeapon::UAircraftWeapon()
	: M_OwningPlayer(0)
	, M_WorldSpawnedIn(nullptr)
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	M_LandedState = EAircraftLandingState::Landed;
}

void UAircraftWeapon::InitAircraftWeaponComponent(AAircraftMaster* AircraftOwner, UMeshComponent* AircraftWeaponMesh)
{
	M_AircraftOwner = AircraftOwner;
	M_AircraftWeaponMesh = AircraftWeaponMesh;
	(void)EnsureAircraftOwnerIsValid();
	(void)EnsureAircraftWeaponMeshIsValid();

	// Initialize the targeting struct with the aircraft owner’s player if available later via Setup* calls.
	M_TargetingData.InitTargetStruct(M_OwningPlayer);
}

void UAircraftWeapon::SetOwningPlayer(const int32 PlayerIndex)
{
	// Update any already-built weapon states.
	for (UWeaponState* WS : M_TWeapons)
	{
		if (IsValid(WS))
		{
			WS->ChangeOwningPlayer(PlayerIndex);
		}
	}
	M_OwningPlayer = PlayerIndex;
	M_TargetingData.InitTargetStruct(M_OwningPlayer);
}

void UAircraftWeapon::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_SetupCallbackToProjectileManager();
}

void UAircraftWeapon::OnWeaponAdded(const int32 WeaponIndex, UWeaponState* Weapon)
{
}

void UAircraftWeapon::RegisterIgnoreActor(AActor* ActorToIgnore, const bool bRegister)
{
	if(not RTSFunctionLibrary::RTSIsValid(ActorToIgnore))
	{
		return;
	}
	for(const auto EachWeapon : M_TWeapons)
	{
		if (not GetIsValidWeapon(EachWeapon))
		{
			continue;
		}
			EachWeapon->RegisterActorToIgnore(ActorToIgnore, bRegister);
	}
}

FVector& UAircraftWeapon::GetFireDirection(const int32 WeaponIndex)
{
	M_FireDir = FVector::ForwardVector;

	if (not EnsureAircraftOwnerIsValid())
	{
		return M_FireDir;
	}

	// Validate mesh/socket mapping for this weapon index.
	const bool bIndexValid = M_TWeapons.IsValidIndex(WeaponIndex);
	const bool bMeshValid = EnsureAircraftWeaponMeshIsValid();
	const bool bSocketExists =
		bIndexValid && bMeshValid &&
		M_AircraftWeaponMesh->DoesSocketExist(M_TWeapons[WeaponIndex]->GetWeaponSocket()) &&
		M_WeaponIndexToSocket.Contains(WeaponIndex);

	// Tick aim selection once per fire-direction read (supports 4-point cycling on actors).
	if (M_TargetingData.GetIsTargetValid())
	{
		M_TargetingData.TickAimSelection();
	}

	// Choose start location for the ray/vector
	const FVector StartLocation =
		bSocketExists ? M_AircraftWeaponMesh->GetSocketLocation(M_WeaponIndexToSocket[WeaponIndex])
		              : M_AircraftOwner->GetActorLocation();

	// Resolve target location (actor aimpoint or fallback)
	if (M_TargetingData.GetIsTargetValid())
	{
		const FVector TargetLoc = M_TargetingData.GetActiveTargetLocation();
		M_FireDir = GetDirectionVectorToTarget(TargetLoc, StartLocation);
	}
	else
	{
		// If no valid target (actor/ground) -> keep forward vector of aircraft
		M_FireDir = M_AircraftOwner->GetActorForwardVector();
	}
	return M_FireDir;
}

FVector& UAircraftWeapon::GetTargetLocation(const int32 WeaponIndex)
{
	return M_TargetingData.GetActiveTargetLocation();	
}

bool UAircraftWeapon::AllowWeaponToReload(const int32 /*WeaponIndex*/) const
{
	if (M_LandedState == EAircraftLandingState::Landed)
	{
		FRTSAircraftHelpers::AircraftDebug("RELOADING aircraft weapon!");
		return true;
	}
	FRTSAircraftHelpers::AircraftDebug("Cannot reload as not landed!");
	return false;
}

void UAircraftWeapon::OnWeaponKilledActor(const int32 /*WeaponIndex*/, AActor* KilledActor)
{
	// Refactored check using the targeting struct helper.
	if (!M_TargetingData.WasKilledActorCurrentTarget(KilledActor))
	{
		return;
	}
	// Clear target; external owner (aircraft) can decide next behavior.
	M_TargetingData.ResetTarget();
}

void UAircraftWeapon::PlayWeaponAnimation(const int32 WeaponIndex, const EWeaponFireMode FireMode)
{
	if(not EnsureAircraftOwnerIsValid())
	{
		return;
	}
	M_AircraftOwner->BP_OnPlayWeaponAnimation(WeaponIndex, FireMode);
}

void UAircraftWeapon::OnReloadStart(const int32 WeaponIndex, const float ReloadTime)
{
}

void UAircraftWeapon::OnReloadFinished(const int32 WeaponIndex)
{
}

void UAircraftWeapon::OnProjectileHit(const bool bBounced)
{
}

void UAircraftWeapon::SetupDirectHitWeapon(FInitWeaponStateDirectHit DirectHitWeaponParameters)
{
	SetOwningPlayer(DirectHitWeaponParameters.OwningPlayer);

	if (not EnsureWorldIsValid())
	{
		return;
	}
	const int32 WeaponIndex = M_TWeapons.Num();
	UWeaponStateDirectHit* DirectHit = NewObject<UWeaponStateDirectHit>(this);
	DirectHit->InitDirectHitWeapon(
		DirectHitWeaponParameters.OwningPlayer,
		WeaponIndex,
		DirectHitWeaponParameters.WeaponName,
		DirectHitWeaponParameters.WeaponBurstMode,
		DirectHitWeaponParameters.WeaponOwner,
		DirectHitWeaponParameters.MeshComponent,
		DirectHitWeaponParameters.FireSocketName,
		M_WorldSpawnedIn,
		DirectHitWeaponParameters.WeaponVFX,
		DirectHitWeaponParameters.WeaponShellCase,
		DirectHitWeaponParameters.BurstCooldown,
		DirectHitWeaponParameters.SingleBurstAmountMaxBurstAmount,
		DirectHitWeaponParameters.MinBurstAmount,
		DirectHitWeaponParameters.CreateShellCasingOnEveryRandomBurst);
	M_TWeapons.Add(DirectHit);
	DirectHit->SetIsAircraftWeapon(true);
	UpdateRangeBasedOnWeapons();
	M_WeaponIndexToSocket.Add(WeaponIndex, DirectHitWeaponParameters.FireSocketName);
}

void UAircraftWeapon::SetupTraceWeapon(FInitWeaponStatTrace TraceWeaponParameters)
{
	SetOwningPlayer(TraceWeaponParameters.OwningPlayer);

	if (not EnsureWorldIsValid())
	{
		return;
	}
	const int32 WeaponIndex = M_TWeapons.Num();
	UWeaponStateTrace* Trace = NewObject<UWeaponStateTrace>(this);
	Trace->InitTraceWeapon(
		TraceWeaponParameters.OwningPlayer,
		WeaponIndex,
		TraceWeaponParameters.WeaponName,
		TraceWeaponParameters.WeaponBurstMode,
		TraceWeaponParameters.WeaponOwner,
		TraceWeaponParameters.MeshComponent,
		TraceWeaponParameters.FireSocketName,
		M_WorldSpawnedIn,
		TraceWeaponParameters.WeaponVFX,
		TraceWeaponParameters.WeaponShellCase,
		TraceWeaponParameters.BurstCooldown,
		TraceWeaponParameters.SingleBurstAmountMaxBurstAmount,
		TraceWeaponParameters.MinBurstAmount,
		TraceWeaponParameters.CreateShellCasingOnEveryRandomBurst,
		TraceWeaponParameters.TraceProjectileType);
	M_TWeapons.Add(Trace);
	Trace->SetIsAircraftWeapon(true);
	UpdateRangeBasedOnWeapons();
	M_WeaponIndexToSocket.Add(WeaponIndex, TraceWeaponParameters.FireSocketName);
	// Ensures valid projectile manager. If not valid yet, it will be set by callback and retrospectively set
	// the reference on the weapon state.
	if(GetIsProjectileManagerLoaded())
	{
		Trace->SetupProjectileManager(M_ProjectileManager.Get());
	}
}

void UAircraftWeapon::SetupProjectileWeapon(FInitWeaponStateProjectile ProjectileWeaponParameters)
{
	SetOwningPlayer(ProjectileWeaponParameters.OwningPlayer);

	if (not EnsureWorldIsValid())
	{
		return;
	}
	const int32 WeaponIndex = M_TWeapons.Num();
	UWeaponStateProjectile* Projectile = NewObject<UWeaponStateProjectile>(this);
	Projectile->InitProjectileWeapon(
		ProjectileWeaponParameters.OwningPlayer,
		WeaponIndex,
		ProjectileWeaponParameters.WeaponName,
		ProjectileWeaponParameters.WeaponBurstMode,
		ProjectileWeaponParameters.WeaponOwner,
		ProjectileWeaponParameters.MeshComponent,
		ProjectileWeaponParameters.FireSocketName,
		M_WorldSpawnedIn,
		ProjectileWeaponParameters.ProjectileSystem,
		ProjectileWeaponParameters.WeaponVFX,
		ProjectileWeaponParameters.WeaponShellCase,
		ProjectileWeaponParameters.BurstCooldown,
		ProjectileWeaponParameters.SingleBurstAmountMaxBurstAmount,
		ProjectileWeaponParameters.MinBurstAmount,
		ProjectileWeaponParameters.CreateShellCasingOnEveryRandomBurst);
	M_TWeapons.Add(Projectile);
	Projectile->SetIsAircraftWeapon(true);
	UpdateRangeBasedOnWeapons();
	M_WeaponIndexToSocket.Add(WeaponIndex, ProjectileWeaponParameters.FireSocketName);
	// Ensures valid projectile manager. If not valid yet, it will be set by callback and retrospectively set
	// the reference on the weapon state.
	if(GetIsProjectileManagerLoaded())
	{
		Projectile->SetupProjectileManager(M_ProjectileManager.Get());
	}
}

void UAircraftWeapon::SetupRocketProjectileWeapon(FInitWeaponStateRocketProjectile RocketProjectileParameters)
{
	SetOwningPlayer(RocketProjectileParameters.OwningPlayer);

	if (not EnsureWorldIsValid())
	{
		return;
	}
	const int32 WeaponIndex = M_TWeapons.Num();
	UWeaponStateRocketProjectile* RocketProjectile = NewObject<UWeaponStateRocketProjectile>(this);
	RocketProjectile->InitRocketProjectileWeapon(
		RocketProjectileParameters.OwningPlayer,
		WeaponIndex,
		RocketProjectileParameters.WeaponName,
		RocketProjectileParameters.WeaponBurstMode,
		RocketProjectileParameters.WeaponOwner,
		RocketProjectileParameters.MeshComponent,
		RocketProjectileParameters.FireSocketName,
		M_WorldSpawnedIn,
		RocketProjectileParameters.ProjectileSystem,
		RocketProjectileParameters.WeaponVFX,
		RocketProjectileParameters.WeaponShellCase,
		RocketProjectileParameters.RocketSettings,
		RocketProjectileParameters.BurstCooldown,
		RocketProjectileParameters.SingleBurstAmountMaxBurstAmount,
		RocketProjectileParameters.MinBurstAmount,
		RocketProjectileParameters.CreateShellCasingOnEveryRandomBurst);
	M_TWeapons.Add(RocketProjectile);
	RocketProjectile->SetIsAircraftWeapon(true);
	UpdateRangeBasedOnWeapons();
	M_WeaponIndexToSocket.Add(WeaponIndex, RocketProjectileParameters.FireSocketName);
	if (GetIsProjectileManagerLoaded())
	{
		RocketProjectile->SetupProjectileManager(M_ProjectileManager.Get());
	}
}

void UAircraftWeapon::SetupArchProjectileWeapon(FInitWeaponStateArchProjectile ArchProjParameters)
{
        RTSFunctionLibrary::ReportError(
                "ArchProjectile weapons are not supported for aircraft weapon component: " + GetName());
}

void UAircraftWeapon::SetupPooledArchProjectileWeapon(FInitWeaponStateArchProjectile ArchProjParameters)
{
        SetupArchProjectileWeapon(ArchProjParameters);
}

void UAircraftWeapon::SetupMultiTraceWeapon(FInitWeaponStateMultiTrace MultiTraceWeaponParameters)
{
	SetOwningPlayer(MultiTraceWeaponParameters.OwningPlayer);

	if (not EnsureWorldIsValid())
	{
		return;
	}

	const int32 WeaponIndex = M_TWeapons.Num();

	UWeaponStateMultiTrace* MultiTrace = NewObject<UWeaponStateMultiTrace>(this);
	MultiTrace->InitMultiTraceWeapon(
		MultiTraceWeaponParameters.OwningPlayer,
		WeaponIndex,
		MultiTraceWeaponParameters.WeaponName,
		MultiTraceWeaponParameters.WeaponBurstMode,
		MultiTraceWeaponParameters.WeaponOwner,
		MultiTraceWeaponParameters.MeshComponent,
		M_WorldSpawnedIn,
		MultiTraceWeaponParameters.FireSocketNames,
		MultiTraceWeaponParameters.bUseOwnerFireDirection,
		MultiTraceWeaponParameters.WeaponVFX,
		MultiTraceWeaponParameters.WeaponShellCase,
		MultiTraceWeaponParameters.BurstCooldown,
		MultiTraceWeaponParameters.SingleBurstAmountMaxBurstAmount,
		MultiTraceWeaponParameters.MinBurstAmount,
		MultiTraceWeaponParameters.CreateShellCasingOnEveryRandomBurst,
		MultiTraceWeaponParameters.TraceProjectileType);
	M_TWeapons.Add(MultiTrace);
	MultiTrace->SetIsAircraftWeapon(true);
	UpdateRangeBasedOnWeapons();
	M_WeaponIndexToSocket.Add(WeaponIndex, MultiTraceWeaponParameters.FireSocketNames[0]);
	// Ensures valid projectile manager. If not valid yet, it will be set by callback and retrospectively set
	// the reference on the weapon state.
	if(GetIsProjectileManagerLoaded())
	{
		MultiTrace->SetupProjectileManager(M_ProjectileManager.Get());
	}
}

void UAircraftWeapon::SetupLaserWeapon(const FInitWeaponStateLaser& LaserWeaponParameters)
{
	SetOwningPlayer(LaserWeaponParameters.OwningPlayer);

	if (not EnsureWorldIsValid())
	{
		return;
	}

	const int32 WeaponIndex = M_TWeapons.Num();
	UWeaponStateLaser* LaserWeapon = NewObject<UWeaponStateLaser>(this);
	LaserWeapon->InitLaserWeapon(
		LaserWeaponParameters.OwningPlayer,
		WeaponIndex,
		LaserWeaponParameters.WeaponName,
		LaserWeaponParameters.WeaponOwner,
		LaserWeaponParameters.MeshComponent,
		LaserWeaponParameters.FireSocketName,
		GetWorld(),
		LaserWeaponParameters.LaserWeaponSettings);

	LaserWeapon->SetIsAircraftWeapon(true);
	M_TWeapons.Add(LaserWeapon);
	UpdateRangeBasedOnWeapons();
	M_WeaponIndexToSocket.Add(WeaponIndex, LaserWeaponParameters.FireSocketName);
}

void UAircraftWeapon::SetupMultiHitLaserWeapon(const FInitWeaponStateMultiHitLaser& LaserWeaponParameters)
{
	SetOwningPlayer(LaserWeaponParameters.OwningPlayer);

	if (not EnsureWorldIsValid())
	{
		return;
	}

	const int32 WeaponIndex = M_TWeapons.Num();
	UWeaponStateMultiHitLaser* LaserWeapon = NewObject<UWeaponStateMultiHitLaser>(this);
	LaserWeapon->InitMultiHitLaserWeapon(
		LaserWeaponParameters.OwningPlayer,
		WeaponIndex,
		LaserWeaponParameters.WeaponName,
		LaserWeaponParameters.WeaponOwner,
		LaserWeaponParameters.MeshComponent,
		LaserWeaponParameters.FireSocketName,
		GetWorld(),
		LaserWeaponParameters.LaserWeaponSettings);

	LaserWeapon->SetIsAircraftWeapon(true);
	M_TWeapons.Add(LaserWeapon);
	UpdateRangeBasedOnWeapons();
	M_WeaponIndexToSocket.Add(WeaponIndex, LaserWeaponParameters.FireSocketName);
}

void UAircraftWeapon::SetupFlameThrowerWeapon(const FInitWeaponStateFlameThrower& FlameWeaponParameters)
{
	SetOwningPlayer(FlameWeaponParameters.OwningPlayer);

	if (!EnsureWorldIsValid())
	{
		return;
	}

	const int32 WeaponIndex = M_TWeapons.Num();

	UWeaponStateFlameThrower* FlameWeapon = NewObject<UWeaponStateFlameThrower>(this);
	FlameWeapon->InitFlameThrowerWeapon(
		FlameWeaponParameters.OwningPlayer,
		WeaponIndex,
		FlameWeaponParameters.WeaponName,
		FlameWeaponParameters.WeaponOwner,
		FlameWeaponParameters.MeshComponent,
		FlameWeaponParameters.FireSocketName,
		GetWorld(),
		FlameWeaponParameters.FlameSettings);

	FlameWeapon->SetIsAircraftWeapon(true);
	M_TWeapons.Add(FlameWeapon);
	UpdateRangeBasedOnWeapons();
	M_WeaponIndexToSocket.Add(WeaponIndex, FlameWeaponParameters.FireSocketName);
}

void UAircraftWeapon::SetupMultiProjectileWeapon(FInitWeaponStateMultiProjectile MultiProjectileState)
{
	SetOwningPlayer(MultiProjectileState.OwningPlayer);

	const int32 WeaponIndex = M_TWeapons.Num();
	UWorld* World = GetWorld();
	if (!World)
	{
		RTSFunctionLibrary::ReportError("World is null for turret: " + GetName());
		return;
	}
	UWeaponStateMultiProjectile* MultiProjectile = NewObject<UWeaponStateMultiProjectile>(this);
	MultiProjectile->InitMultiProjectileWeapon(
		M_OwningPlayer,
		WeaponIndex,
		MultiProjectileState.WeaponName,
		MultiProjectileState.WeaponBurstMode,
		MultiProjectileState.WeaponOwner,
		MultiProjectileState.MeshComponent,
		World,
		MultiProjectileState.FireSocketNames,
		MultiProjectileState.ProjectileSystem,
		MultiProjectileState.WeaponVFX,
		MultiProjectileState.WeaponShellCase,
		MultiProjectileState.BurstCooldown,
		MultiProjectileState.SingleBurstAmountMaxBurstAmount,
		MultiProjectileState.MinBurstAmount,
		MultiProjectileState.CreateShellCasingOnEveryRandomBurst);
	M_TWeapons.Add(MultiProjectile);
	MultiProjectile->SetIsAircraftWeapon(true);
	UpdateRangeBasedOnWeapons();
	M_WeaponIndexToSocket.Add(WeaponIndex, MultiProjectileState.FireSocketNames[0]);
	// Ensures valid projectile manager. If not valid yet, it will be set by callback and retrospectively set
	// the reference on the weapon state.
	if(GetIsProjectileManagerLoaded())
	{
		MultiProjectile->SetupProjectileManager(M_ProjectileManager.Get());
	}
}

void UAircraftWeapon::ForceInstantReloadAllWeapons()
{
	for (const auto EachWeapon : M_TWeapons)
	{
		if (not GetIsValidWeapon(EachWeapon))
		{
			continue;
		}
		EachWeapon->ForceInstantReload();
	}
}

void UAircraftWeapon::AllWeaponsStopFire(const bool bStopReload, const bool bStopCoolDown)
{
	for (const auto EachWeapon : M_TWeapons)
	{
		if (not GetIsValidWeapon(EachWeapon))
		{
			continue;
		}
		EachWeapon->StopFire(bStopReload, bStopCoolDown);
	}
}

void UAircraftWeapon::AllWeaponsFire(AActor* TargetActor)
{
	// Store target in struct; weapons still receive nullptr for ground if caller passes nullptr.
	M_TargetingData.SetTargetActor(TargetActor);

	for (const auto EachWeapon : M_TWeapons)
	{
		if (not GetIsValidWeapon(EachWeapon))
		{
			continue;
		}
			EachWeapon->Fire(M_TargetingData.GetActiveTargetLocation());
	}
}


float UAircraftWeapon::GetTotalReloadTimeOfWeapons()
{
	float AccumulativeReloadTime = 0.f;
	for (const auto EachWeapon : M_TWeapons)
	{
		if (not GetIsValidWeapon(EachWeapon))
		{
			continue;
		}
		AccumulativeReloadTime += EachWeapon->GetRawWeaponData().ReloadSpeed;
	}
	return AccumulativeReloadTime;
}

bool UAircraftWeapon::GetIsValidWeapon(const UWeaponState* Weapon) const
{
	if (IsValid(Weapon))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("Invalid weapon state on AircraftWeapon : " + GetName());
	return false;
}

void UAircraftWeapon::BeginPlay_SetupCallbackToProjectileManager()
{
	ACPPGameState* GameState = FRTS_Statics::GetGameState(this);
	if (not GameState)
	{
		return;
	}
	TWeakObjectPtr<UAircraftWeapon> WeakThis(this);
	auto Callback = [WeakThis](const TObjectPtr<ASmallArmsProjectileManager>& ProjectileManager)-> void
	{
		if (not WeakThis.IsValid())
		{
			return;
		}
		WeakThis->OnProjectileManagerLoaded(ProjectileManager);
	};
	GameState->RegisterCallbackForSmallArmsProjectileMgr(
		Callback, this);
}

void UAircraftWeapon::OnProjectileManagerLoaded(const TObjectPtr<ASmallArmsProjectileManager>& ProjectileManager)
{
	if (not IsValid(ProjectileManager))
	{
		RTSFunctionLibrary::ReportError("OnProjectileManagerLoaded: ProjectileManager is not valid"
			"\n For HullWeaponComponent: " + GetName());
	}
	for (const auto EachMountedWeapon : M_TWeapons)
	{
		FRTSWeaponHelpers::SetupProjectileManagerForWeapon(EachMountedWeapon, ProjectileManager);
	}
	// In case the projectile manager is loaded before the weapon; we use the weak reference to set it up on
	// added weapons.
	M_ProjectileManager = ProjectileManager;
}

void UAircraftWeapon::UpdateRangeBasedOnWeapons()
{
	for (const auto EachWeapon : M_TWeapons)
	{
		if (not IsValid(EachWeapon))
		{
			RTSFunctionLibrary::ReportError(
				"Invalid weapon for aircraft weapon: " + GetName() +
				" at function: AircraftWeapon::UpdateTurretRangeBasedOnWeapons");
			continue;
		}
		M_WeaponRangeData.AdjustRangeForNewWeapon(EachWeapon->GetRange());
	}
}

bool UAircraftWeapon::EnsureWorldIsValid()
{
	if (IsValid(M_WorldSpawnedIn))
	{
		return true;
	}
	M_WorldSpawnedIn = GetWorld();
	return IsValid(M_WorldSpawnedIn);
}

bool UAircraftWeapon::EnsureAircraftWeaponMeshIsValid() const
{
	if (not M_AircraftWeaponMesh.IsValid())
	{
		RTSFunctionLibrary::ReportError("Invalid AircraftWeaponMesh on AircraftWeapon: " + GetName());
		return false;
	}
	return true;
}

bool UAircraftWeapon::EnsureAircraftOwnerIsValid() const
{
	if (not M_AircraftOwner.IsValid())
	{
		RTSFunctionLibrary::ReportError("Invalid AircraftOwner on AircraftWeapon: " + GetName());
		return false;
	}
	return true;
}

bool UAircraftWeapon::GetIsProjectileManagerLoaded() const
{
	return M_ProjectileManager.IsValid();
}

FVector UAircraftWeapon::GetDirectionVectorToTarget(const FVector& TargetLocation, const FVector& StartLocation) const
{
	return TargetLocation - StartLocation;
}
