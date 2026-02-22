// Copyright (C) 2020-2023 Bas Blokzijl - All rights reserved.

#include "CPPTurretsMaster.h"

#include "ComponentUtils.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponSystems.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/TargetPreference/TargetPreference.h"
#include "RTS_Survival/GameUI/Healthbar/AmmoHealthBar/AmmoHpBarTrackerState/FAmmoHpBarTrackerState.h"
#include "RTS_Survival/MasterObjects/HealthBase/HpPawnMaster.h"
#include "RTS_Survival/RTSComponents/SelectionComponent.h"
#include "RTS_Survival/Weapons/SmallArmsProjectileManager/SmallArmsProjectileManager.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/FlameThrowerWeapon/UWeaponStateFlameThrower.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"
#include "RTS_Survival/Weapons/WeaponData/MultiTraceWeapon/WeaponStateMultiTrace.h"
#include "Kismet/KismetMathLibrary.h"
#include "RTS_Survival/Weapons/LaserWeapon/UWeaponStateLaser.h"
#include "RTS_Survival/Weapons/LaserWeapon/UWeaponStateMultiHitLaser.h"
#include "Trace/Trace.h"
#include "TurretOwner/TurretOwner.h"

ACPPTurretsMaster::ACPPTurretsMaster()
	: SceneSkeletalMesh(nullptr)
	  , RotationSpeed(30)
	  , TargetPitch(0)
	  , M_WorldSpawnedIn(nullptr)
	  , M_MaxTurretPitch(0)
	  , M_MinTurretPitch(0)
{
	SetActorTickInterval(DeveloperSettings::Optimization::TurretTickInterval);
	M_WeaponAIState = EWeaponAIState::None;

	// Defaults covered by struct default members.
}

void ACPPTurretsMaster::SetAutoEngageTargets(const bool bUseLastTarget)
{
	M_WeaponAIState = EWeaponAIState::AutoEngage;
	StopAllWeaponsFire(!bUseLastTarget);
	InitiateAutoEngageTimers();
	OnTurretActive();
}

void ACPPTurretsMaster::SetEngageSpecificTarget(AActor* Target)
{
	if (not Target)
	{
		return;
	}
	// If already aiming at this actor: ignore.
	if (M_TargetingData.GetTargetActor() == Target)
	{
		return;
	}
	M_WeaponAIState = EWeaponAIState::SpecificEngage;

	StopAllWeaponsFire(true);
	// Preserve turret state changes but store target in struct.
	SetTarget(Target);

	InitiateSpecificTargetTimers();
}

void ACPPTurretsMaster::SetEngageGroundLocation(const FVector& GroundLocation)
{
	if (not EnsureWorldIsValid())
	{
		return;
	}
	M_WeaponAIState = EWeaponAIState::TargetGround;

	StopAllWeaponsFire(/*InvalidateTarget*/true);

	OnTurretActive();

	M_TargetingData.SetTargetGround(GroundLocation);

	// Use the specific-engage loop so we get OnTurretOutOfRange / OnTurretInRange callbacks to the owner (tank).
	InitiateSpecificTargetTimers();
}

FTurretIdleAnimationState::FTurretIdleAnimationState(): M_BaseLocalYaw(0), M_TargetLocalYaw(0),
                                                        M_IdleTurretRotationType(EIdleRotation::Idle_Animate)
{
}

void ACPPTurretsMaster::Tick(float DeltaTime)
{
	// 0.33 ms with 123 turrets.
	// trace_cpuprofiler_event_scope(acppturretsmaster::tick);
	if (not bM_IsFullyRotatedToTarget)
	{
		RotateTurret(DeltaTime);
	}
}

void ACPPTurretsMaster::UpdateTargetPitchCPP(float NewPitch)
{
	TargetPitch = FMath::Clamp(NewPitch, M_MinTurretPitch, M_MaxTurretPitch);
	UpdateTargetPitchForAnimation(TargetPitch);
}

void ACPPTurretsMaster::InitTurretOwner(AActor* NewTurretOwner)
{
	if (NewTurretOwner->Implements<UTurretOwner>())
	{
		TurretOwner.SetInterface(Cast<ITurretOwner>(NewTurretOwner));
		TurretOwner.SetObject(NewTurretOwner);
	}
}

bool ACPPTurretsMaster::EnsureWorldIsValid()
{
	if (IsValid(M_WorldSpawnedIn))
	{
		return true;
	}
	M_WorldSpawnedIn = GetWorld();
	return IsValid(M_WorldSpawnedIn);
}

void ACPPTurretsMaster::SetOwningPlayer(const int32 NewOwningPlayer)
{
	M_OwningPlayer = NewOwningPlayer;
	M_TargetingData.InitTargetStruct(M_OwningPlayer);
}

void ACPPTurretsMaster::InitiateAutoEngageTimers()
{
	if (not EnsureWorldIsValid())
	{
		return;
	}
	M_WorldSpawnedIn->GetTimerManager().ClearTimer(M_WeaponSearchHandle);
	M_WeaponSearchDel.BindUObject(this, &ACPPTurretsMaster::AutoEngage);
	M_WorldSpawnedIn->GetTimerManager().SetTimer(M_WeaponSearchHandle,
	                                             M_WeaponSearchDel, M_WeaponSearchTimeInterval, true);
}

void ACPPTurretsMaster::InitiateSpecificTargetTimers()
{
	if (not EnsureWorldIsValid())
	{
		return;
	}
	M_WorldSpawnedIn->GetTimerManager().ClearTimer(M_WeaponSearchHandle);
	M_WeaponSearchDel.BindUObject(this, &ACPPTurretsMaster::SpecificEngage);
	M_WorldSpawnedIn->GetTimerManager().SetTimer(M_WeaponSearchHandle,
	                                             M_WeaponSearchDel, M_WeaponSearchTimeInterval, true);
}

TArray<UWeaponState*> ACPPTurretsMaster::GetWeapons()
{
	TArray<UWeaponState*> Weapons;
	for (const auto EachWeapon : M_TWeapons)
	{
		if (IsValid(EachWeapon))
		{
			Weapons.Add(EachWeapon);
		}
	}
	return Weapons;
}

int32 ACPPTurretsMaster::GetOwningPlayer()
{
	if (M_OwningPlayer == -1)
	{
		SetOwningPlayer(TurretOwner.GetInterface() ? TurretOwner.GetInterface()->GetOwningPlayer() : -1);
	}
	return M_OwningPlayer;
}

void ACPPTurretsMaster::UpgradeWeaponRange(const UWeaponState* MyWeapon, const float RangeMlt)
{
	if (not IsValid(MyWeapon))
	{
		return;
	}
	for (auto EachWeapon : M_TWeapons)
	{
		if (IsValid(EachWeapon) && EachWeapon == MyWeapon)
		{
			EachWeapon->UpgradeWeaponWithRangeMlt(RangeMlt);
			UpdateTurretRangeBasedOnWeapons();
		}
	}
}

void ACPPTurretsMaster::DisableTurret()
{
	if (M_WeaponSearchHandle.IsValid())
	{
		M_WorldSpawnedIn->GetTimerManager().ClearTimer(M_WeaponSearchHandle);
	}
	DisableAllWeapons();
}

void ACPPTurretsMaster::ApplyMaterialToAllMeshComponents(UMaterialInterface* MaterialOverride) const
{
	if (not IsValid(MaterialOverride))
	{
		return;
	}

	TArray<UMeshComponent*> MeshComponents;
	GetComponents<UMeshComponent>(MeshComponents);
	for (UMeshComponent* MeshComponent : MeshComponents)
	{
		if (not IsValid(MeshComponent))
		{
			continue;
		}

		const int32 MaterialCount = MeshComponent->GetNumMaterials();
		for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; MaterialIndex++)
		{
			MeshComponent->SetMaterial(MaterialIndex, MaterialOverride);
		}
	}
}

void ACPPTurretsMaster::OnSetupTurret(AActor* OwnerOfTurret)
{
	if (M_AmmoTrackingState.bIsUsingWeaponAmmoTracker)
	{
		M_AmmoTrackingState.SetActorWithAmmoWidget(OwnerOfTurret);
	}
}

void ACPPTurretsMaster::BeginPlay()
{
	// Calls beginplay on blueprint.
	ACPPWeaponsMaster::BeginPlay();

	SetActorTickEnabled(true);

	if (AActor* ParentActor = GetParentActor(); ParentActor && ParentActor->GetClass()->ImplementsInterface(
		UTurretOwner::StaticClass()))
	{
		InitTurretOwner(ParentActor);
		InitSelectionDelegatesOfOwner(ParentActor);
	}

	M_WorldSpawnedIn = GetWorld();

	if (ACPPController* NewPlayerController = Cast<ACPPController>(UGameplayStatics::GetPlayerController(this, 0)))
	{
		PlayerController = NewPlayerController;
	}
	else
	{
		RTSFunctionLibrary::ReportNullErrorInitialisation(this, "PlayerController", "ACPPTurretsMaster::BeginPlay");
	}
	// For trace weapons.
	SetupCallbackToProjectileManager();

	if (M_AmmoTrackingState.bIsUsingWeaponAmmoTracker && M_WorldSpawnedIn)
	{
		const TWeakObjectPtr<ACPPTurretsMaster> WeakTurret(this);
		M_WorldSpawnedIn->GetTimerManager().SetTimer(
			M_AmmoTrackingState.VerifyBindingTimer,
			FTimerDelegate::CreateLambda([WeakTurret]()
			{
				if (not WeakTurret.IsValid())
				{
					return;
				}

				WeakTurret->M_AmmoTrackingState.VerifyTrackingActive();
			}),
			M_AmmoTrackingState.VerifyBindingDelay, false);
	}
}

void ACPPTurretsMaster::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	BP_PostInit();
}

int32 ACPPTurretsMaster::GetOwningPlayerForWeaponInit()
{
	if (AActor* OwnerActor = GetParentActor())
	{
		if (ITurretOwner* OwnerInterface = Cast<ITurretOwner>(OwnerActor))
		{
			return OwnerInterface->GetOwningPlayer();
		}
		RTSFunctionLibrary::ReportError("No OwnerInterface found for turret: " + GetName() +
			"\n at function: ACPPTurretsMaster::GetOwningPlayerForWeaponInit");
	}
	RTSFunctionLibrary::ReportError("No Owning Player found for turret: " + GetName()
		+ "\n at function: ACPPTurretsMaster::GetOwningPlayerForWeaponInit");
	return int32();
}

FRotator ACPPTurretsMaster::GetTargetRotation(const FVector& TargetLocation, const FVector& TurretLocation) const
{
	return UKismetMathLibrary::MakeRotator(0, 0,
	                                       UKismetMathLibrary::FindLookAtRotation(TurretLocation, TargetLocation).Yaw);
}

FTransform ACPPTurretsMaster::GetTurretTransform() const
{
	return SceneSkeletalMesh->GetComponentTransform();
}

void ACPPTurretsMaster::InitTurretsMaster(USkeletalMeshComponent* NewSceneSkeletalMesh)
{
	SceneSkeletalMesh = NewSceneSkeletalMesh;

	if (GetIsValidSceneSkeletalMesh())
	{
		// Remember the mesh’s local "forward" yaw as our base for Idle_Base
		IdleAnimationState.M_BaseLocalYaw = SceneSkeletalMesh->GetRelativeRotation().Yaw;
	}
}

void ACPPTurretsMaster::InitChildTurret(const float NewRotationSpeed,
                                        const float NewMaxPitch,
                                        const float NewMinPitch,
                                        EIdleRotation NewIdleTurretRotationType)
{
	RotationSpeed = NewRotationSpeed;
	M_MaxTurretPitch = NewMaxPitch;
	M_MinTurretPitch = NewMinPitch;
	IdleAnimationState.M_IdleTurretRotationType = NewIdleTurretRotationType;
}

void ACPPTurretsMaster::RegisterIgnoreActor(AActor* ActorToIgnore, const bool bRegister)
{
	if (not RTSFunctionLibrary::RTSIsValid(ActorToIgnore))
	{
		return;
	}
	for (auto EachWeapon : M_TWeapons)
	{
		if (IsValid(EachWeapon))
		{
			EachWeapon->RegisterActorToIgnore(ActorToIgnore, bRegister);
		}
	}
}

void ACPPTurretsMaster::OnWeaponAdded(const int32 WeaponIndex, UWeaponState* Weapon)
{
	if (M_AmmoTrackingState.bIsUsingWeaponAmmoTracker)
	{
		M_AmmoTrackingState.CheckIsWeaponToTrack(WeaponIndex, Weapon);
	}
}

void ACPPTurretsMaster::OnWeaponBehaviourChangesRange(const UWeaponState* ReportingWeaponState, const float NewRange)
{
	UpdateTurretRangeBasedOnWeapons();
}

void ACPPTurretsMaster::SetupWeaponAmmoTracker(FAmmoTrackerInitSettings AmmoTrackingSettings)
{
	if (not AmmoTrackingSettings.EnsureIsValid())
	{
		return;
	}
	// After this the actor to look on for the widget still needs to be set.
	M_AmmoTrackingState.AmmoTrackerInitSettings = AmmoTrackingSettings;
	M_AmmoTrackingState.bIsUsingWeaponAmmoTracker = true;
	for (uint8 i = 0; i < M_TWeapons.Num(); ++i)
	{
		UWeaponState* EachWeapon = M_TWeapons[i];
		if (IsValid(EachWeapon))
		{
			M_AmmoTrackingState.CheckIsWeaponToTrack(i, EachWeapon);
		}
	}
}

FVector& ACPPTurretsMaster::GetFireDirection(const int32 /*WeaponIndex*/)
{
	return SteeringState.M_TargetDirection;
}

FVector& ACPPTurretsMaster::GetTargetLocation(const int32 WeaponIndex)
{
	return M_TargetingData.GetActiveTargetLocation();
}

bool ACPPTurretsMaster::AllowWeaponToReload(const int32 /*WeaponIndex*/) const
{
	return true;
}

void ACPPTurretsMaster::OnWeaponKilledActor(const int32 /*WeaponIndex*/, AActor* KilledActor)
{
	if (not M_TargetingData.WasKilledActorCurrentTarget(KilledActor))
	{
		return;
	}

	if (TurretOwner)
	{
		// Owner will dictate turret behaviour after this.
		TurretOwner.GetInterface()->OnMountedWeaponTargetDestroyed(this, nullptr, M_TargetingData.GetTargetActor(),
		                                                           true);
	}
	else
	{
		SetAutoEngageTargets(false);
	}

	if constexpr (DeveloperSettings::Debugging::GTurret_Master_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Turret killed specific target!");
	}
}

void ACPPTurretsMaster::PlayWeaponAnimation(const int32 WeaponIndex, const EWeaponFireMode FireMode)
{
	BP_PlayWeaponAnimation(WeaponIndex, FireMode);
	if (TurretOwner)
	{
		TurretOwner->OnFireWeapon(this);
	}
}

void ACPPTurretsMaster::OnProjectileHit(const bool bBounced)
{
	if (TurretOwner)
	{
		TurretOwner->OnProjectileHit(bBounced);
	}
}

void ACPPTurretsMaster::OnReloadStart(const int32 WeaponIndex, const float ReloadTime)
{
	ReloadWeapon(WeaponIndex, ReloadTime);
}

void ACPPTurretsMaster::OnReloadFinished(const int32 /*WeaponIndex*/)
{
}

void ACPPTurretsMaster::ForceSetAllWeaponsFullyReloaded()
{
	for (const auto EachWeapon : M_TWeapons)
	{
		if (not EachWeapon)
		{
			continue;
		}
		EachWeapon->ForceInstantReload();
	}
}

void ACPPTurretsMaster::SetupDirectHitWeapon(FInitWeaponStateDirectHit DirectHitWeaponParameters)
{
	SetOwningPlayer(DirectHitWeaponParameters.OwningPlayer);
	const int32 WeaponIndex = M_TWeapons.Num();

	UWorld* World = GetWorld();
	if (not World)
	{
		RTSFunctionLibrary::ReportError("World is null for turret: " + GetName());
		return;
	}
	UWeaponStateDirectHit* DirectHit = NewObject<UWeaponStateDirectHit>(this);
	DirectHit->InitDirectHitWeapon(
		DirectHitWeaponParameters.OwningPlayer,
		WeaponIndex,
		DirectHitWeaponParameters.WeaponName,
		DirectHitWeaponParameters.WeaponBurstMode,
		DirectHitWeaponParameters.WeaponOwner,
		DirectHitWeaponParameters.MeshComponent,
		DirectHitWeaponParameters.FireSocketName,
		World,
		DirectHitWeaponParameters.WeaponVFX,
		DirectHitWeaponParameters.WeaponShellCase,
		DirectHitWeaponParameters.BurstCooldown,
		DirectHitWeaponParameters.SingleBurstAmountMaxBurstAmount,
		DirectHitWeaponParameters.MinBurstAmount,
		DirectHitWeaponParameters.CreateShellCasingOnEveryRandomBurst);
	M_TWeapons.Add(DirectHit);
	UpdateTurretRangeBasedOnWeapons();
}

void ACPPTurretsMaster::SetupTraceWeapon(FInitWeaponStatTrace TraceWeaponParameters)
{
	SetOwningPlayer(TraceWeaponParameters.OwningPlayer);
	const int32 WeaponIndex = M_TWeapons.Num();
	UWorld* World = GetWorld();
	if (not World)
	{
		RTSFunctionLibrary::ReportError("World is null for turret: " + GetName());
		return;
	}
	UWeaponStateTrace* Trace = NewObject<UWeaponStateTrace>(this);
	Trace->InitTraceWeapon(
		TraceWeaponParameters.OwningPlayer,
		WeaponIndex,
		TraceWeaponParameters.WeaponName,
		TraceWeaponParameters.WeaponBurstMode,
		TraceWeaponParameters.WeaponOwner,
		TraceWeaponParameters.MeshComponent,
		TraceWeaponParameters.FireSocketName,
		World,
		TraceWeaponParameters.WeaponVFX,
		TraceWeaponParameters.WeaponShellCase,
		TraceWeaponParameters.BurstCooldown,
		TraceWeaponParameters.SingleBurstAmountMaxBurstAmount,
		TraceWeaponParameters.MinBurstAmount,
		TraceWeaponParameters.CreateShellCasingOnEveryRandomBurst,
		TraceWeaponParameters.TraceProjectileType);
	M_TWeapons.Add(Trace);
	UpdateTurretRangeBasedOnWeapons();

	// Did we already obtain the projectile manager from the callback; if so set the reference now.
	if (M_ProjectileManager.IsValid())
	{
		FRTSWeaponHelpers::SetupProjectileManagerForWeapon(Trace, M_ProjectileManager.Get());
	}
}

void ACPPTurretsMaster::SetupMultiTraceWeapon(FInitWeaponStateMultiTrace MultiTraceWeaponParameters)
{
	SetOwningPlayer(MultiTraceWeaponParameters.OwningPlayer);
	UWorld* World = GetWorld();
	if (not World)
	{
		RTSFunctionLibrary::ReportError("World is null for turret: " + GetName());
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
	UpdateTurretRangeBasedOnWeapons();

	// Did we already obtain the projectile manager from the callback; if so set the reference now.
	if (M_ProjectileManager.IsValid())
	{
		FRTSWeaponHelpers::SetupProjectileManagerForWeapon(MultiTrace, M_ProjectileManager.Get());
	}
}

void ACPPTurretsMaster::SetupLaserWeapon(const FInitWeaponStateLaser& LaserWeaponParameters)
{
	SetOwningPlayer(LaserWeaponParameters.OwningPlayer);
	UWorld* World = GetWorld();
	if (not World)
	{
		RTSFunctionLibrary::ReportError("World is null for turret: " + GetName());
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

	M_TWeapons.Add(LaserWeapon);
	UpdateTurretRangeBasedOnWeapons();
}

void ACPPTurretsMaster::SetupMultiHitLaserWeapon(const FInitWeaponStateMultiHitLaser& LaserWeaponParameters)
{
	SetOwningPlayer(LaserWeaponParameters.OwningPlayer);
	UWorld* World = GetWorld();
	if (not World)
	{
		RTSFunctionLibrary::ReportError("World is null for turret: " + GetName());
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

	M_TWeapons.Add(LaserWeapon);
	UpdateTurretRangeBasedOnWeapons();
}

void ACPPTurretsMaster::SetupFlameThrowerWeapon(const FInitWeaponStateFlameThrower& FlameWeaponParameters)
{
	SetOwningPlayer(FlameWeaponParameters.OwningPlayer);

	UWorld* World = GetWorld();
	if (not World)
	{
		RTSFunctionLibrary::ReportError("World is null for turret: " + GetName());
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

	M_TWeapons.Add(FlameWeapon);
	UpdateTurretRangeBasedOnWeapons();
}

void ACPPTurretsMaster::SetupProjectileWeapon(FInitWeaponStateProjectile ProjectileWeaponParameters)
{
	SetOwningPlayer(ProjectileWeaponParameters.OwningPlayer);
	const int32 WeaponIndex = M_TWeapons.Num();
	UWorld* World = GetWorld();
	if (not World)
	{
		RTSFunctionLibrary::ReportError("World is null for turret: " + GetName());
		return;
	}
	UWeaponStateProjectile* Projectile = NewObject<UWeaponStateProjectile>(this);
	Projectile->InitProjectileWeapon(
		ProjectileWeaponParameters.OwningPlayer,
		WeaponIndex,
		ProjectileWeaponParameters.WeaponName,
		ProjectileWeaponParameters.WeaponBurstMode,
		ProjectileWeaponParameters.WeaponOwner,
		ProjectileWeaponParameters.MeshComponent,
		ProjectileWeaponParameters.FireSocketName,
		World,
		ProjectileWeaponParameters.ProjectileSystem,
		ProjectileWeaponParameters.WeaponVFX,
		ProjectileWeaponParameters.WeaponShellCase,
		ProjectileWeaponParameters.BurstCooldown,
		ProjectileWeaponParameters.SingleBurstAmountMaxBurstAmount,
		ProjectileWeaponParameters.MinBurstAmount,
		ProjectileWeaponParameters.CreateShellCasingOnEveryRandomBurst);
	M_TWeapons.Add(Projectile);
	UpdateTurretRangeBasedOnWeapons();
	// Did we already obtain the projectile manager from the callback; if so set the reference now.
	if (M_ProjectileManager.IsValid())
	{
		FRTSWeaponHelpers::SetupProjectileManagerForWeapon(Projectile, M_ProjectileManager.Get());
	}
}

void ACPPTurretsMaster::SetupRocketProjectileWeapon(FInitWeaponStateRocketProjectile RocketProjectileParameters)
{
	SetOwningPlayer(RocketProjectileParameters.OwningPlayer);
	const int32 WeaponIndex = M_TWeapons.Num();
	UWorld* World = GetWorld();
	if (not World)
	{
		RTSFunctionLibrary::ReportError("World is null for turret: " + GetName());
		return;
	}
	UWeaponStateRocketProjectile* RocketProjectile = NewObject<UWeaponStateRocketProjectile>(this);
	RocketProjectile->InitRocketProjectileWeapon(
		RocketProjectileParameters.OwningPlayer,
		WeaponIndex,
		RocketProjectileParameters.WeaponName,
		RocketProjectileParameters.WeaponBurstMode,
		RocketProjectileParameters.WeaponOwner,
		RocketProjectileParameters.MeshComponent,
		RocketProjectileParameters.FireSocketName,
		World,
		RocketProjectileParameters.ProjectileSystem,
		RocketProjectileParameters.WeaponVFX,
		RocketProjectileParameters.WeaponShellCase,
		RocketProjectileParameters.RocketSettings,
		RocketProjectileParameters.BurstCooldown,
		RocketProjectileParameters.SingleBurstAmountMaxBurstAmount,
		RocketProjectileParameters.MinBurstAmount,
		RocketProjectileParameters.CreateShellCasingOnEveryRandomBurst);
	M_TWeapons.Add(RocketProjectile);
	UpdateTurretRangeBasedOnWeapons();
	if (M_ProjectileManager.IsValid())
	{
		FRTSWeaponHelpers::SetupProjectileManagerForWeapon(RocketProjectile, M_ProjectileManager.Get());
	}
}

void ACPPTurretsMaster::SetupVerticalRocketProjectileWeapon(
	FInitWeaponStateVerticalRocketProjectile VerticalRocketProjectileParameters)
{
	SetOwningPlayer(VerticalRocketProjectileParameters.OwningPlayer);
	const int32 WeaponIndex = M_TWeapons.Num();
	UWorld* World = GetWorld();
	if (not World)
	{
		RTSFunctionLibrary::ReportError("World is null for turret: " + GetName());
		return;
	}
	UVerticalRocketWeaponState* VerticalRocketProjectile = NewObject<UVerticalRocketWeaponState>(this);
	VerticalRocketProjectile->InitVerticalRocketWeapon(
		VerticalRocketProjectileParameters.OwningPlayer,
		WeaponIndex,
		VerticalRocketProjectileParameters.WeaponName,
		VerticalRocketProjectileParameters.WeaponBurstMode,
		VerticalRocketProjectileParameters.WeaponOwner,
		VerticalRocketProjectileParameters.MeshComponent,
		VerticalRocketProjectileParameters.FireSocketName,
		World,
		VerticalRocketProjectileParameters.ProjectileSystem,
		VerticalRocketProjectileParameters.WeaponVFX,
		VerticalRocketProjectileParameters.WeaponShellCase,
		VerticalRocketProjectileParameters.VerticalRocketSettings,
		VerticalRocketProjectileParameters.BurstCooldown,
		VerticalRocketProjectileParameters.SingleBurstAmountMaxBurstAmount,
		VerticalRocketProjectileParameters.MinBurstAmount,
		VerticalRocketProjectileParameters.CreateShellCasingOnEveryRandomBurst);
	M_TWeapons.Add(VerticalRocketProjectile);
	UpdateTurretRangeBasedOnWeapons();
	if (M_ProjectileManager.IsValid())
	{
		FRTSWeaponHelpers::SetupProjectileManagerForWeapon(VerticalRocketProjectile, M_ProjectileManager.Get());
	}
}

void ACPPTurretsMaster::SetupMultiProjectileWeapon(FInitWeaponStateMultiProjectile MultiProjectileState)
{
	SetOwningPlayer(MultiProjectileState.OwningPlayer);
	const int32 WeaponIndex = M_TWeapons.Num();
	UWorld* World = GetWorld();
	if (not World)
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
	UpdateTurretRangeBasedOnWeapons();

	// Did we already obtain the projectile manager from the callback; if so set the reference now.
	if (M_ProjectileManager.IsValid())
	{
		FRTSWeaponHelpers::SetupProjectileManagerForWeapon(MultiProjectile, M_ProjectileManager.Get());
	}
}

void ACPPTurretsMaster::SetupArchProjectileWeapon(FInitWeaponStateArchProjectile ArchProjParameters)
{
	SetOwningPlayer(ArchProjParameters.OwningPlayer);
	const int32 WeaponIndex = M_TWeapons.Num();
	UWorld* World = GetWorld();
	if (not World)
	{
		RTSFunctionLibrary::ReportError("World is null for turret: " + GetName());
		return;
	}
	UWeaponStateArchProjectile* ArchProjectile = NewObject<UWeaponStateArchProjectile>(this);
	if (ArchProjectile)
	{
		ArchProjectile->InitArchProjectileWeapon(
			ArchProjParameters.OwningPlayer,
			WeaponIndex,
			ArchProjParameters.WeaponName,
			ArchProjParameters.WeaponBurstMode,
			ArchProjParameters.WeaponOwner,
			ArchProjParameters.MeshComponent,
			ArchProjParameters.FireSocketName,
			World,
			ArchProjParameters.ProjectileSystem,
			ArchProjParameters.WeaponVFX,
			ArchProjParameters.WeaponShellCase,
			ArchProjParameters.BurstCooldown,
			ArchProjParameters.SingleBurstAmountMaxBurstAmount,
			ArchProjParameters.MinBurstAmount,
			ArchProjParameters.CreateShellCasingOnEveryRandomBurst,
			ArchProjParameters.ArchSettings);
	}
	if (M_ProjectileManager.IsValid())
	{
		FRTSWeaponHelpers::SetupProjectileManagerForWeapon(ArchProjectile, M_ProjectileManager.Get());
	}
	M_TWeapons.Add(ArchProjectile);
	UpdateTurretRangeBasedOnWeapons();
}

void ACPPTurretsMaster::SetupPooledArchProjectileWeapon(FInitWeaponStateArchProjectile ArchProjParameters)
{
	SetupArchProjectileWeapon(ArchProjParameters);
}


void ACPPTurretsMaster::SetupSplitterArchProjectileWeapon(
	FInitWeaponStateSplitterArchProjectile SplitterArchProjParameters)
{
	SetOwningPlayer(SplitterArchProjParameters.OwningPlayer);
	const int32 WeaponIndex = M_TWeapons.Num();
	UWorld* SpawnWorld = GetWorld();
	if (not SpawnWorld)
	{
		RTSFunctionLibrary::ReportError("World is null for turret: " + GetName());
		return;
	}

	UWeaponStateSplitterArchProjectile* SplitterProjectile = NewObject<UWeaponStateSplitterArchProjectile>(this);
	if (not SplitterProjectile)
	{
		RTSFunctionLibrary::ReportError("Failed to create splitter arch projectile state for turret: " + GetName());
		return;
	}

	SplitterProjectile->InitSplitterArchProjectileWeapon(
		SplitterArchProjParameters.OwningPlayer,
		WeaponIndex,
		SplitterArchProjParameters.WeaponName,
		SplitterArchProjParameters.WeaponBurstMode,
		SplitterArchProjParameters.WeaponOwner,
		SplitterArchProjParameters.MeshComponent,
		SplitterArchProjParameters.FireSocketName,
		SpawnWorld,
		SplitterArchProjParameters.ProjectileSystem,
		SplitterArchProjParameters.WeaponVFX,
		SplitterArchProjParameters.WeaponShellCase,
		SplitterArchProjParameters.BurstCooldown,
		SplitterArchProjParameters.SingleBurstAmountMaxBurstAmount,
		SplitterArchProjParameters.MinBurstAmount,
		SplitterArchProjParameters.CreateShellCasingOnEveryRandomBurst,
		SplitterArchProjParameters.SplitterSettings);

	if (M_ProjectileManager.IsValid())
	{
		FRTSWeaponHelpers::SetupProjectileManagerForWeapon(SplitterProjectile, M_ProjectileManager.Get());
	}

	M_TWeapons.Add(SplitterProjectile);
	UpdateTurretRangeBasedOnWeapons();
}

void ACPPTurretsMaster::GetClosestTarget()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GetClosestTarget);
	if (bM_IsPendingTargetSearch || not GetIsValidGameUnitManger())
	{
		return;
	}

	TWeakObjectPtr<ACPPTurretsMaster> WeakThis(this);

	const FVector SearchLocation = GetActorLocation();
	const float SearchRadius = M_WeaponRangeData.M_WeaponSearchRadius;
	const int32 NumTargets = 3;
	bM_IsPendingTargetSearch = true;

	// Request targets from the game unit manager
	M_GameUnitManager->RequestClosestTargets(
		SearchLocation,
		SearchRadius,
		NumTargets,
		GetOwningPlayer(),
		TargetPreference,
		[WeakThis](const TArray<AActor*>& Targets)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnTargetsFound(Targets);
			}
		});
}

void ACPPTurretsMaster::OnTargetsFound(const TArray<AActor*>& Targets)
{
	bM_IsPendingTargetSearch = false;
	for (AActor* Target : Targets)
	{
		if (RTSFunctionLibrary::RTSIsVisibleTarget(Target, GetOwningPlayer()))
		{
			// preserves turret state changes; stores target in struct internally 	
			SetTarget(Target);
			return;
		}
	}
}

void ACPPTurretsMaster::SetupCallbackToProjectileManager()
{
	ACPPGameState* GameState = FRTS_Statics::GetGameState(this);
	if (not GameState)
	{
		return;
	}
	TWeakObjectPtr<ACPPTurretsMaster> WeakThis(this);
	auto Callback = [WeakThis](const TObjectPtr<ASmallArmsProjectileManager>& ProjectileManager)-> void
	{
		if (not WeakThis.IsValid())
		{
			return;
		}
		WeakThis->OnProjectileManagerLoaded(ProjectileManager);
	};
	GameState->RegisterCallbackForSmallArmsProjectileMgr(Callback, this);
}

void ACPPTurretsMaster::OnProjectileManagerLoaded(const TObjectPtr<ASmallArmsProjectileManager>& ProjectileManager)
{
	if (not IsValid(ProjectileManager))
	{
		RTSFunctionLibrary::ReportError("OnProjectileManagerLoaded: ProjectileManager is not valid"
			"\n For Turret: " + GetName());
		return;
	}
	M_ProjectileManager = ProjectileManager;
	for (const auto EachMountedWeapon : M_TWeapons)
	{
		FRTSWeaponHelpers::SetupProjectileManagerForWeapon(EachMountedWeapon, ProjectileManager);
	}
}

void ACPPTurretsMaster::UpdateIdleAnimationRotation()
{
	// Only update if idle animation mode is enabled.
	if (IdleAnimationState.M_IdleTurretRotationType != EIdleRotation::Idle_Animate)
	{
		return;
	}

	// Choose a new random yaw value between 0 and 360 degrees.
	const float RandomYaw = FMath::RandRange(0.f, 360.f);
	// Create a new target rotation with only yaw changed (no pitch or roll).
	SteeringState.M_TargetRotator = FRotator(0.f, RandomYaw, 0.f);

	// Mark that we have not yet rotated to the new target rotation.
	bM_IsFullyRotatedToTarget = false;

	// Restart the timer with a new delay: 5 ± random value between -3 and 3 seconds.
	const float TimerDelay = 5.f + FMath::RandRange(-3.f, 3.f);
	if (UWorld* World = GetWorld())
	{
		// Clear and restart the timer.
		World->GetTimerManager().ClearTimer(IdleAnimationState.IdleAnimationTimerHandle);
		World->GetTimerManager().SetTimer(
			IdleAnimationState.IdleAnimationTimerHandle,
			this,
			&ACPPTurretsMaster::UpdateIdleAnimationRotation,
			TimerDelay,
			false);
	}
}

void ACPPTurretsMaster::StartIdleAnimationTimer()
{
	if (IdleAnimationState.M_IdleTurretRotationType == EIdleRotation::Idle_Animate && GetWorld())
	{
		// Only start the timer if it is not already running.
		if (!GetWorld()->GetTimerManager().IsTimerActive(IdleAnimationState.IdleAnimationTimerHandle))
		{
			constexpr float BaseTime = DeveloperSettings::GamePlay::Turret::RotateAnimationInterval;
			constexpr float FluxTime = DeveloperSettings::GamePlay::Turret::RotateAnimationFlux;
			const float TimerDelay = BaseTime + FMath::RandRange(-FluxTime, FluxTime);
			GetWorld()->GetTimerManager().SetTimer(
				IdleAnimationState.IdleAnimationTimerHandle,
				this,
				&ACPPTurretsMaster::UpdateIdleAnimationRotation,
				TimerDelay,
				false);
		}
	}
}

void ACPPTurretsMaster::StopIdleAnimationTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(IdleAnimationState.IdleAnimationTimerHandle);
	}
}

void ACPPTurretsMaster::OnTurretIdle()
{
	bM_IsFullyRotatedToTarget = false;
	IdleSelectState.bIsIdle = true;

	switch (IdleAnimationState.M_IdleTurretRotationType)
	{
	case Idle_KeepLast:
		break;

	case Idle_Base:
		// key: set once, in local space
		SetIdleBaseLocalTarget();
		return;

	case Idle_Animate:
		if (IdleSelectState.bIsSelected)
		{
			// Selected while we normally animate → rotate to base (locally).
			SetIdleBaseLocalTarget();
		}
		else
		{
			// Not selected; animate idle turret.
			StartIdleAnimationTimer();
		}
		break;
	}
}

void ACPPTurretsMaster::OnTurretActive()
{
	if (IdleAnimationState.M_IdleTurretRotationType == EIdleRotation::Idle_Animate)
	{
		StopIdleAnimationTimer();
	}
	IdleSelectState.bIsIdle = false;
	// back to world-space steering for targets
	IdleAnimationState.bM_UseLocalYawTarget = false;
}

void ACPPTurretsMaster::OnTurretSelected()
{
	IdleSelectState.bIsSelected = true;
	if (not IdleSelectState.bIsIdle)
	{
		// Turret is not idle but active.
		return;
	}
	if (IdleAnimationState.M_IdleTurretRotationType == EIdleRotation::Idle_Animate)
	{
		StopIdleAnimationTimer();
		SetIdleBaseLocalTarget(); // rotate to base smoothly (local)
	}
	else if (IdleAnimationState.M_IdleTurretRotationType == EIdleRotation::Idle_Base)
	{
		SetIdleBaseLocalTarget();
	}
}

void ACPPTurretsMaster::OnTurretDeselected()
{
	IdleSelectState.bIsSelected = false;
	if (not IdleSelectState.bIsIdle)
	{
		return;
	}
	if (IdleAnimationState.M_IdleTurretRotationType == EIdleRotation::Idle_Animate)
	{
		// resume animation; go back to world drive for animate
		IdleAnimationState.bM_UseLocalYawTarget = false;
		bM_IsFullyRotatedToTarget = false;
		StartIdleAnimationTimer();
	}
}

void ACPPTurretsMaster::InitSelectionDelegatesOfOwner(AActor* OwnerOfTurret)
{
	if (not IsValid(OwnerOfTurret))
	{
		return;
	}
	USelectionComponent* SelectionComponent = OwnerOfTurret->FindComponentByClass<USelectionComponent>();
	if (IsValid(SelectionComponent))
	{
		TWeakObjectPtr<ACPPTurretsMaster> WeakThis(this);
		SelectionComponent->OnUnitSelected.AddLambda(
			[WeakThis]()-> void
			{
				if (WeakThis.IsValid())
				{
					WeakThis->OnTurretSelected();
				}
			});
		SelectionComponent->OnUnitDeselected.AddLambda(
			[WeakThis]()-> void
			{
				if (WeakThis.IsValid())
				{
					WeakThis->OnTurretDeselected();
				}
			});
	}
}

void ACPPTurretsMaster::AutoEngage()
{
	// 1) Early out if no valid target.
	if (not M_TargetingData.GetIsTargetValid())
	{
		StopAllWeaponsFire(true);
		ResetTarget();
		GetClosestTarget();
		return;
	}

	// 2) Distance gate to max range.
	const FVector MyLoc = GetActorLocation();
	const FVector TgtLoc = M_TargetingData.GetActiveTargetLocation();
	const bool bInRange = (FVector::DistSquared(TgtLoc, MyLoc) <= M_WeaponRangeData.M_MaxWeaponRangeSquared);
	if (not bInRange)
	{
		StopAllWeaponsFire(true);
		ResetTarget();
		GetClosestTarget();
		return;
	}

	// 3) Cheap aim selection tick before using location (may rotate aim point).
	M_TargetingData.TickAimSelection();

	// 4) Rotate and, if aligned, fire.
	RotateTurretToActor(nullptr /*unused now*/);
	if (bM_IsRotatedToEngage)
	{
		if constexpr (DeveloperSettings::Debugging::GTurret_Master_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString("Ready to engage", FColor::Green);
		}
		FireAllWeapons();
	}
}

void ACPPTurretsMaster::SpecificEngage()
{
	// Target invalid => switch to auto engage and notify owner if needed.
	if (not M_TargetingData.GetIsTargetValid() || not TurretOwner)
	{
		SpecificEngage_OnTargetInvalid();
		return;
	}

	// Distance check
	const FVector TgtLoc = M_TargetingData.GetActiveTargetLocation();
	const FVector MyLoc = GetActorLocation();
	const bool bInRange = (FVector::DistSquared(TgtLoc, MyLoc) <= M_WeaponRangeData.M_MaxWeaponRangeSquared);

	// Still rotate even if not in range; owner reacts to range state.
	RotateTurretToActor(nullptr /*unused*/);

	if (bInRange)
	{
		TurretOwner.GetInterface()->OnTurretInRange(this);
		if (bM_IsRotatedToEngage)
		{
			if constexpr (DeveloperSettings::Debugging::GTurret_Master_Compile_DebugSymbols)
			{
				RTSFunctionLibrary::PrintString("Ready to engage", FColor::Green);
			}
			FireAllWeapons();
		}
	}
	else
	{
		TurretOwner.GetInterface()->OnTurretOutOfRange(TgtLoc, this);
	}
}

void ACPPTurretsMaster::SpecificEngage_OnTargetInvalid()
{
	// Target is not alive; set to auto engage and notify owner.
	StopTurretRotation();
	SetAutoEngageTargets(false);
	ResetTarget();
	if (TurretOwner)
	{
		// Make sure to not trigger a voice line as we do not know who killed the target just that it is invalid.
		const bool bDestroyedByOwnWeapons = false;
		TurretOwner.GetInterface()->OnMountedWeaponTargetDestroyed(this, nullptr, ActorTarget, bDestroyedByOwnWeapons);
	}
}

FRotator ACPPTurretsMaster::GetBaseRotation() const
{
	switch (IdleAnimationState.M_IdleTurretRotationType)
	{
	case EIdleRotation::Idle_Base:
		if (TurretOwner.GetObject())
		{
			// Keep behavior as before; not used for Idle_Base driving anymore.
			return TurretOwner.GetInterface()->GetOwnerRotation();
		}
		return FRotator(0, 0, 0);

	case EIdleRotation::Idle_KeepLast:
		return SteeringState.M_TargetRotator;

	case Idle_Animate:
		if (TurretOwner.GetObject())
		{
			return TurretOwner.GetInterface()->GetOwnerRotation();
		}
		return FRotator(0, 0, 0);
	}
	return FRotator(0, 0, 0);
}

void ACPPTurretsMaster::ResetTarget()
{
	bM_IsRotatedToEngage = false;
	bM_IsFullyRotatedToTarget = false;
	M_TargetingData.ResetTarget();
	SteeringState.M_LastTargetPosition = FVector::ZeroVector;
	OnTurretIdle();
}

void ACPPTurretsMaster::SetTarget(AActor* NewTarget)
{
	if (not NewTarget)
	{
		return;
	}

	OnTurretActive();

	// Store on struct (loads offsets / sets active location).
	M_TargetingData.SetTargetActor(NewTarget);

	// Compute initial desired yaw from current turret transform to active location.
	const FTransform CurrentTransform = GetTurretTransform();
	SteeringState.M_TargetRotator = GetTargetRotation(M_TargetingData.GetActiveTargetLocation(),
	                                                  CurrentTransform.GetLocation());
	bM_IsRotatedToEngage = false;
	bM_IsFullyRotatedToTarget = false;

	if constexpr (DeveloperSettings::Debugging::GAsyncTargetFinding_Compile_DebugSymbols)
	{
		FString TargetType = "None";

		if (ATankMaster* Tank = Cast<ATankMaster>(NewTarget))
		{
			TargetType = "Tank";
		}
		else if (AHpPawnMaster* TestHpPawn = Cast<AHpPawnMaster>(NewTarget))
		{
			TargetType = Global_GetTargetPreferenceAsString(TestHpPawn->GetTargetPreference());
		}

		const FColor DebugColor = GetOwningPlayer() == 1 ? FColor::Green : FColor::Red;
		const FString DebugString = GetOwningPlayer() == 1 ? "Targeted by P1" : "Targeted By Enemy";
		const FVector DebugLocation = NewTarget->GetActorLocation() + FVector(0, 0, 300);

		DrawDebugString(GetWorld(), DebugLocation, TargetType + DebugString, nullptr, DebugColor, 2.0f, false);
	}
}

void ACPPTurretsMaster::RotateTurretToActor(const AActor* const /*Target*/)
{
	// TRACE_CPUPROFILER_EVENT_SCOPE(RotateTurretToActiveTarget);
	const FVector TargetLocation = M_TargetingData.GetActiveTargetLocation();

	bIsTargetRotatorUpdated = true;
	SteeringState.M_LastTargetPosition = TargetLocation;

	const FTransform CurrentTransform = GetTurretTransform();

	// Update pitch & fire direction using target LOCATION (not actor).
	const FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(
		CurrentTransform.GetLocation(), TargetLocation);
	SteeringState.M_TargetDirection = LookAtRotation.Vector();
	UpdateTargetPitchCPP(FMath::Clamp(LookAtRotation.Pitch, M_MinTurretPitch, M_MaxTurretPitch));

	// Update desired yaw
	SteeringState.M_TargetRotator = GetTargetRotation(TargetLocation, CurrentTransform.GetLocation());

	// New target rotator obtained so rotate turret.
	bM_IsFullyRotatedToTarget = false;

	if constexpr (DeveloperSettings::Debugging::GTurret_Master_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("new target rotator is set.");
	}
}

void ACPPTurretsMaster::RotateTurret(const float DeltaTime)
{
	// While in Idle_Base we rotate the mesh in local (tank) space towards a fixed local target.
	if (IdleAnimationState.bM_UseLocalYawTarget)
	{
		RotateTurret_LocalIdle(DeltaTime);
		return;
	}

	// World-space steering (targets / Idle_Animate).
	RotateTurret_LocalYaw_Root(DeltaTime);
}

void ACPPTurretsMaster::RotateTurret_LocalIdle(const float DeltaTime)
{
	if (not GetIsValidSceneSkeletalMesh())
	{
		return;
	}

	const FRotator CurrentLocal = SceneSkeletalMesh->GetRelativeRotation();
	const float DeltaYaw = FMath::FindDeltaAngleDegrees(CurrentLocal.Yaw, IdleAnimationState.M_TargetLocalYaw);

	if (FMath::IsNearlyZero(DeltaYaw, KINDA_SMALL_NUMBER))
	{
		bM_IsRotatedToEngage = true;
		bM_IsFullyRotatedToTarget = true;
		StopTurretRotation();
		return;
	}

	if (FMath::Abs(DeltaYaw) <= AllowedDegreesOffTarget)
	{
		bM_IsRotatedToEngage = true;
	}

	const float Step = FMath::Clamp(RotationSpeed * DeltaTime, 0.f, FMath::Abs(DeltaYaw));
	FRotator NewLocal = CurrentLocal;
	NewLocal.Yaw = NewLocal.Yaw + Step * FMath::Sign(DeltaYaw);
	SceneSkeletalMesh->SetRelativeRotation(NewLocal);
}

void ACPPTurretsMaster::RotateTurret_LocalYaw_Root(const float DeltaTime)
{
	USceneComponent* const TurretRootComponent = GetRootComponent();
	if (not IsValid(TurretRootComponent))
	{
		return;
	}

	const USceneComponent* const ParentAttachComponent = TurretRootComponent->GetAttachParent();

	// If we have no parent (not actually attached), fall back to world-space yaw on the root.
	// But we still only change yaw.
	if (not IsValid(ParentAttachComponent))
	{
		const FRotator CurrentWorldRotation = TurretRootComponent->GetComponentRotation();
		const float CurrentYaw = CurrentWorldRotation.Yaw;

		const float TargetWorldYaw = SteeringState.M_TargetRotator.Yaw;
		const float DeltaYawDegrees = FMath::FindDeltaAngleDegrees(CurrentYaw, TargetWorldYaw);
		const float AbsDeltaYawDegrees = FMath::Abs(DeltaYawDegrees);

		if (AbsDeltaYawDegrees <= AllowedDegreesOffTarget)
		{
			bM_IsRotatedToEngage = true;

			FRotator SnappedWorldRotation = CurrentWorldRotation;
			SnappedWorldRotation.Yaw = TargetWorldYaw;

			TurretRootComponent->SetWorldRotation(SnappedWorldRotation.GetNormalized());
			StopTurretRotation();
			return;
		}

		const float MaxStepThisFrame = RotationSpeed * DeltaTime; // deg/sec
		const float NewYaw = FMath::FixedTurn(CurrentYaw, TargetWorldYaw, MaxStepThisFrame);

		FRotator NewWorldRotation = CurrentWorldRotation;
		NewWorldRotation.Yaw = NewYaw;

		TurretRootComponent->SetWorldRotation(NewWorldRotation.GetNormalized());
		return;
	}

	// Convert world target rotation into the *parent attach component's local space*.
	const FQuat ParentWorldQuat = ParentAttachComponent->GetComponentQuat();
	const FQuat TargetWorldQuat = SteeringState.M_TargetRotator.Quaternion();
	const FQuat TargetLocalQuat = ParentWorldQuat.Inverse() * TargetWorldQuat;
	const float TargetLocalYaw = TargetLocalQuat.Rotator().Yaw;

	const FRotator CurrentRelativeRotation = TurretRootComponent->GetRelativeRotation();
	const float CurrentLocalYaw = CurrentRelativeRotation.Yaw;

	const float DeltaYawDegrees = FMath::FindDeltaAngleDegrees(CurrentLocalYaw, TargetLocalYaw);
	const float AbsDeltaYawDegrees = FMath::Abs(DeltaYawDegrees);

	if (AbsDeltaYawDegrees <= AllowedDegreesOffTarget)
	{
		bM_IsRotatedToEngage = true;

		FRotator SnappedRelativeRotation = CurrentRelativeRotation;
		SnappedRelativeRotation.Yaw = TargetLocalYaw;

		TurretRootComponent->SetRelativeRotation(SnappedRelativeRotation.GetNormalized());
		StopTurretRotation();
		return;
	}

	const float MaxStepThisFrame = RotationSpeed * DeltaTime; // deg/sec
	const float NewYaw = FMath::FixedTurn(CurrentLocalYaw, TargetLocalYaw, MaxStepThisFrame);

	FRotator NewRelativeRotation = CurrentRelativeRotation;
	NewRelativeRotation.Yaw = NewYaw;

	TurretRootComponent->SetRelativeRotation(NewRelativeRotation.GetNormalized());
}

void ACPPTurretsMaster::StopTurretRotation()
{
	bM_IsFullyRotatedToTarget = true;
	if constexpr (DeveloperSettings::Debugging::GTurret_Master_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Finished rotation!!!", FColor::Red);
	}
}

void ACPPTurretsMaster::SetIdleBaseLocalTarget()
{
	if (not GetIsValidSceneSkeletalMesh())
	{
		return;
	}
	IdleAnimationState.M_TargetLocalYaw = IdleAnimationState.M_BaseLocalYaw; // one fixed target (local)
	IdleAnimationState.bM_UseLocalYawTarget = true;

	bM_IsRotatedToEngage = false;
	bM_IsFullyRotatedToTarget = false;
}

bool ACPPTurretsMaster::GetIsValidSceneSkeletalMesh() const
{
	if (not IsValid(SceneSkeletalMesh))
	{
		return false;
	}
	return true;
}

void ACPPTurretsMaster::StopAllWeaponsFire(const bool InvalidateTarget)
{
	bM_IsRotatedToEngage = false;

	if (InvalidateTarget)
	{
		M_TargetingData.ResetTarget();
	}

	for (const auto EachWeapon : M_TWeapons)
	{
		if (EachWeapon)
		{
			EachWeapon->StopFire(false, true);
		}
	}
}

void ACPPTurretsMaster::DisableAllWeapons()
{
	for (const auto EachWeapon : M_TWeapons)
	{
		if (not EachWeapon)
		{
			continue;
		}
		EachWeapon->DisableWeapon();
	}
}

void ACPPTurretsMaster::FireAllWeapons()
{
	for (const auto EachWeapon : M_TWeapons)
	{
		if (EachWeapon)
		{
			EachWeapon->Fire(M_TargetingData.GetActiveTargetLocation());
		}
	}
}

void ACPPTurretsMaster::UpdateTurretRangeBasedOnWeapons()
{
	float FlameThrowerRange = 0.f;
	// Check if we have a flamethrower, if so set range to the range of that weapon.
	if (FRTSWeaponHelpers::GetAdjustedRangeIfFlameThrowerPresent(M_TWeapons, FlameThrowerRange))
	{
		M_WeaponRangeData.ForceSetRange(FlameThrowerRange);
		return;
	}
	for (const auto EachWeapon : M_TWeapons)
	{
		if (not IsValid(EachWeapon))
		{
			RTSFunctionLibrary::ReportError(
				"Invalid weapon for turret: " + GetName() +
				" at function: ACPPTurretsMaster::UpdateTurretRangeBasedOnWeapons");
		}
	}
	M_WeaponRangeData.RecalculateRangeFromWeapons(M_TWeapons);
}
