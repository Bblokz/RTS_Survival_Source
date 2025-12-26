// Copyright (C) Bas Blokzijl - All rights reserved.


#include "HullWeaponComponent.h"

#include "Kismet/KismetMathLibrary.h"
#include "RTS_Survival/Weapons/SmallArmsProjectileManager/SmallArmsProjectileManager.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/FlameThrowerWeapon/UWeaponStateFlameThrower.h"
#include "RTS_Survival/Weapons/LaserWeapon/UWeaponStateLaser.h"
#include "RTS_Survival/Weapons/WeaponAIState/WeaponAIState.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"
#include "RTS_Survival/Weapons/WeaponData/MultiTraceWeapon/WeaponStateMultiTrace.h"


UHullWeaponComponent::UHullWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	bM_IsPendingTargetSearch = false;
}

void UHullWeaponComponent::SetOwningPlayer(const int32 PlayerIndex)
{
	M_OwningPlayer = PlayerIndex;
	M_TargetingData.InitTargetStruct(M_OwningPlayer);
}

void UHullWeaponComponent::SetAutoEngageTargets(const bool bUseLastTarget)
{
	M_WeaponAIState = EWeaponAIState::AutoEngage;
	if (not bUseLastTarget)
	{
		M_TargetingData.ResetTarget();
	}
	AllWeaponsStopFire(false, false);
	InitiateAutoEngageTimers();
}

void UHullWeaponComponent::SetEngageSpecificTarget(AActor* Target)
{
	if (Target && M_TargetingData.GetTargetActor() == Target)
	{
		// Already targeting this actor.
		return;
	}
	M_WeaponAIState = EWeaponAIState::AutoEngage;
	AllWeaponsStopFire(false, false);
	M_TargetingData.SetTargetActor(Target);
	InitiateSpecificEngageTimers();
}

void UHullWeaponComponent::SetEngageGroundLocation(const FVector& GroundLocation)
{
	AllWeaponsStopFire(/*bStopReload*/false, /*bStopCoolDown*/false);
	M_WeaponAIState = EWeaponAIState::AutoEngage;

	// Push the ground aim into the target struct.
	M_TargetingData.SetTargetGround(GroundLocation);

	InitiateSpecificEngageTimers();
}
void UHullWeaponComponent::DisableHullWeapon()
{
	if (EnsureWorldIsValid() && M_WeaponSearchHandle.IsValid())
	{
		M_WorldSpawnedIn->GetTimerManager().ClearTimer(M_WeaponSearchHandle);
	}
	DisableAllWeapons();
}

void UHullWeaponComponent::InitHullWeaponComponent(UMeshComponent* HullWeaponMesh, const FHullWeaponSettings Settings)
{
	if (not IsValid(HullWeaponMesh))
	{
		RTSFunctionLibrary::ReportNullErrorInitialisation(this, "HullMeshComponent", "InitHullWeaponComponent");
		return;
	}
	M_HullWeaponMesh = HullWeaponMesh;
	M_HullWeaponSettings = Settings;

	if (GetIsValidHullWeaponMesh())
	{
		// Store our current relative rotation (offset from parent socket). Often identity, which is fine.
		M_BaseRelativeQuat = M_HullWeaponMesh->GetRelativeTransform().GetRotation();
		bM_BaseInit = true;
		M_TargetDirectionVectorWorldSpace = M_HullWeaponMesh->GetForwardVector(); // default
	}
	FRTS_CollisionSetup::SetupCollisionForHullMountedWeapon(HullWeaponMesh);
}

void UHullWeaponComponent::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_InitTurretOwner(GetOwner());
	BeginPlay_SetupCallbackToProjectileManager();
	BeginPlay_SetupGameUnitManager();
	BeginPlay_SetupMeshOrientationTimer();
}

void UHullWeaponComponent::OnWeaponAdded(const int32 WeaponIndex, UWeaponState* Weapon)
{
}

void UHullWeaponComponent::RegisterIgnoreActor(AActor* ActorToIgnore, const bool bRegister)
{
	if(not RTSFunctionLibrary::RTSIsValid(ActorToIgnore))
	{
		return;
	}
	for (const auto EachWeapon : M_TWeapons)
	{
		if (not GetIsValidWeapon(EachWeapon))
		{
			continue;
		}
		EachWeapon->RegisterActorToIgnore(ActorToIgnore, bRegister);
	}
}

FVector& UHullWeaponComponent::GetFireDirection(const int32 WeaponIndex)
{
	return M_TargetDirectionVectorWorldSpace;
}

FVector& UHullWeaponComponent::GetTargetLocation(const int32 WeaponIndex)
{
	return M_TargetingData.GetActiveTargetLocation();
}

bool UHullWeaponComponent::AllowWeaponToReload(const int32 WeaponIndex) const
{
	return true;
}

void UHullWeaponComponent::OnWeaponKilledActor(const int32 WeaponIndex, AActor* KilledActor)
{
	if (not M_TargetingData.WasKilledActorCurrentTarget(KilledActor))
	{
		return;
	}

	if (TurretOwner)
	{
		// Owner will dictate turret behaviour after this.
		TurretOwner.GetInterface()->OnMountedWeaponTargetDestroyed(nullptr, this, M_TargetingData.GetTargetActor(),
		                                                           true);
	}
	else
	{
		SetAutoEngageTargets(false);
	}
}


void UHullWeaponComponent::PlayWeaponAnimation(const int32 WeaponIndex, const EWeaponFireMode FireMode)
{
	// No animations for hull components.
}

void UHullWeaponComponent::OnProjectileHit(const bool bBounced)
{
}

void UHullWeaponComponent::OnReloadStart(const int32 WeaponIndex, const float ReloadTime)
{
	// No reload animations for hull components.
}

void UHullWeaponComponent::OnReloadFinished(const int32 WeaponIndex)
{
	// no reload animations for hull components.
}

void UHullWeaponComponent::SetupDirectHitWeapon(FInitWeaponStateDirectHit DirectHitWeaponParameters)
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
	UpdateHullWeaponRangeBasedOnWeapons();
}

void UHullWeaponComponent::SetupTraceWeapon(FInitWeaponStatTrace TraceWeaponParameters)
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
	UpdateHullWeaponRangeBasedOnWeapons();

	// Did we already obtain the projectile manager from the callback; if so set the reference now.
	if (M_ProjectileManager.IsValid())
	{
		FRTSWeaponHelpers::SetupProjectileManagerForWeapon(Trace, M_ProjectileManager.Get());
	}
}

void UHullWeaponComponent::SetupMultiTraceWeapon(FInitWeaponStateMultiTrace MultiTraceWeaponParameters)
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
	UpdateHullWeaponRangeBasedOnWeapons();
	// Did we already obtain the projectile manager from the callback; if so set the reference now.
	if (M_ProjectileManager.IsValid())
	{
		FRTSWeaponHelpers::SetupProjectileManagerForWeapon(MultiTrace, M_ProjectileManager.Get());
	}
}

void UHullWeaponComponent::SetupLaserWeapon(const FInitWeaponStateLaser& LaserWeaponParameters)
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


	M_TWeapons.Add(LaserWeapon);
	UpdateHullWeaponRangeBasedOnWeapons();
}

void UHullWeaponComponent::SetupFlameThrowerWeapon(const FInitWeaponStateFlameThrower& FlameWeaponParameters)
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

	M_TWeapons.Add(FlameWeapon);
	UpdateHullWeaponRangeBasedOnWeapons();
}


void UHullWeaponComponent::SetupProjectileWeapon(FInitWeaponStateProjectile ProjectileWeaponParameters)
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
	UpdateHullWeaponRangeBasedOnWeapons();

	// Did we already obtain the projectile manager from the callback; if so set the reference now.
	if (M_ProjectileManager.IsValid())
	{
		FRTSWeaponHelpers::SetupProjectileManagerForWeapon(Projectile, M_ProjectileManager.Get());
	}
}

void UHullWeaponComponent::SetupMultiProjectileWeapon(FInitWeaponStateMultiProjectile MultiProjectileState)
{
	SetOwningPlayer(MultiProjectileState.OwningPlayer);
	const int32 WeaponIndex = M_TWeapons.Num();
	UWorld* World = GetWorld();
	if (!World)
	{
		RTSFunctionLibrary::ReportError("World is null for hull weapon: " + GetName());
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
	UpdateHullWeaponRangeBasedOnWeapons();
	// Did we already obtain the projectile manager from the callback; if so set the reference now.
	if (M_ProjectileManager.IsValid())
	{
		FRTSWeaponHelpers::SetupProjectileManagerForWeapon(MultiProjectile, M_ProjectileManager.Get());
	}
}

void UHullWeaponComponent::SetupArchProjectileWeapon(FInitWeaponStateArchProjectile ArchProjParameters)
{
        RTSFunctionLibrary::ReportError("ArchProjectile weapons are not supported for HullWeaponComponent: " + GetName());
}

void UHullWeaponComponent::SetupPooledArchProjectileWeapon(FInitWeaponStateArchProjectile ArchProjParameters)
{
        SetupArchProjectileWeapon(ArchProjParameters);
}

bool UHullWeaponComponent::EnsureWorldIsValid()
{
	if (IsValid(M_WorldSpawnedIn))
	{
		return true;
	}
	M_WorldSpawnedIn = GetWorld();
	return IsValid(M_WorldSpawnedIn);
}

bool UHullWeaponComponent::GetIsValidTurretOwner() const
{
	if (IsValid(TurretOwner.GetObject()))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("TurretOwner is not valid for HullWeaponComponent: " + GetName());
	return false;
}

void UHullWeaponComponent::InitiateAutoEngageTimers()
{
	if (not EnsureWorldIsValid())
	{
		return;
	}
	M_WorldSpawnedIn->GetTimerManager().ClearTimer(M_WeaponSearchHandle);
	M_WeaponSearchDel.BindUObject(this, &UHullWeaponComponent::AutoEngage);
	M_WorldSpawnedIn->GetTimerManager().SetTimer(M_WeaponSearchHandle, M_WeaponSearchDel, M_WeaponSearchTimeInterval,
	                                             true);
}


void UHullWeaponComponent::InitiateSpecificEngageTimers()
{
	if (not EnsureWorldIsValid())
	{
		return;
	}
	M_WorldSpawnedIn->GetTimerManager().ClearTimer(M_WeaponSearchHandle);
	M_WeaponSearchDel.BindUObject(this, &UHullWeaponComponent::SpecificEngage);
	M_WorldSpawnedIn->GetTimerManager().SetTimer(M_WeaponSearchHandle, M_WeaponSearchDel, M_WeaponSearchTimeInterval,
	                                             true);
}

bool UHullWeaponComponent::GetIsValidHullWeaponMesh() const
{
	if (M_HullWeaponMesh.IsValid())
	{
		return true;
	}
	const FString OwnerName = IsValid(GetOwner()) ? GetOwner()->GetName() : "nullptr";
	RTSFunctionLibrary::ReportError("HullWeaponMesh is not valid for HullWeaponComponent: " + GetName() +
		"\n Owner: " + OwnerName);
	return false;
}

void UHullWeaponComponent::BeginPlay_InitTurretOwner(AActor* NewTurretOwner)
{
	if (not NewTurretOwner->Implements<UTurretOwner>())
	{
		const FString OwnerName = IsValid(NewTurretOwner) ? NewTurretOwner->GetName() : "nullptr";
		RTSFunctionLibrary::ReportError(
			"Owner provided for HullWeapon does not implement Turret owner interface: " + OwnerName);
		return;
	}
	TurretOwner.SetInterface(Cast<ITurretOwner>(NewTurretOwner));
	TurretOwner.SetObject(NewTurretOwner);
}

bool UHullWeaponComponent::GetIsValidWeapon(const UWeaponState* Weapon) const
{
	if (IsValid(Weapon))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("Invalid weapon state on HullWeaponComponent : " + GetName());
	return false;
}

void UHullWeaponComponent::AllWeaponsStopFire(const bool bStopReload, const bool bStopCoolDown)
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

void UHullWeaponComponent::AllWeaponsFire()
{
	AActor* TgtActor = M_TargetingData.GetTargetActor(); // may be null for Ground
	for (const auto EachWeapon : M_TWeapons)
	{
		if (not GetIsValidWeapon(EachWeapon))
		{
			continue;
		}
		EachWeapon->Fire(M_TargetingData.GetActiveTargetLocation());
	}
}

void UHullWeaponComponent::DisableAllWeapons()
{
	for (auto EachWeapon : M_TWeapons)
	{
		if (not GetIsValidWeapon(EachWeapon))
		{
			continue;
		}
		EachWeapon->DisableWeapon();
	}
}

void UHullWeaponComponent::BeginPlay_SetupMeshOrientationTimer()
{
	if (not EnsureWorldIsValid())
	{
		return;
	}
	M_WorldSpawnedIn->GetTimerManager().ClearTimer(M_WeaponMeshOrientationHandle);
	M_WorldSpawnedIn->GetTimerManager().SetTimer(M_WeaponMeshOrientationHandle, this,
	                                             &UHullWeaponComponent::OnUpdateMeshOrientation, 0.33, true);
}

void UHullWeaponComponent::OnUpdateMeshOrientation()
{
	if (!EnsureWorldIsValid() || !GetIsValidHullWeaponMesh() || !bM_BaseInit)
	{
		return;
	}

	const bool bHasAnyTarget = M_TargetingData.GetIsTargetValid();

	if (!bHasAnyTarget)
	{
		FTransform ParentSocketWorld;
		if (!GetParentSocketWorldTransform(ParentSocketWorld))
		{
			return;
		}
		M_DesiredWorldQuat = ParentSocketWorld.GetRotation() * M_BaseRelativeQuat;
		M_HullWeaponMesh->SetWorldRotation(M_DesiredWorldQuat);
		M_TargetDirectionVectorWorldSpace = M_DesiredWorldQuat.GetAxisX();
		return;
	}

	// Tick aim selection before reading location (may rotate to next point).
	M_TargetingData.TickAimSelection();

	FQuat DesiredQuat;
	double OutYawDeg = 0.0, OutPitchDeg = 0.0;
	const FVector TargetLoc = M_TargetingData.GetActiveTargetLocation();

	if (ComputeDesiredAim(TargetLoc, DesiredQuat, OutYawDeg, OutPitchDeg))
	{
		M_DesiredWorldQuat = DesiredQuat;
		M_HullWeaponMesh->SetWorldRotation(M_DesiredWorldQuat);
		M_TargetDirectionVectorWorldSpace = M_DesiredWorldQuat.GetAxisX();
	}
}


void UHullWeaponComponent::AutoEngage()
{
	if (not GetIsValidHullWeaponMesh() || not IsValid(M_HullWeaponMesh->GetAttachParent()))
	{
		return;
	}
	if (not M_TargetingData.GetIsTargetValid())
	{
		OnTargetInvalidOrOutOfReach();
		return;
	}

	const FVector MeshLocation = M_HullWeaponMesh->GetComponentLocation();
	M_TargetingData.TickAimSelection();
	const FVector TargetLocation = M_TargetingData.GetActiveTargetLocation();

	if (not GetIsTargetInRange(TargetLocation, MeshLocation))
	{
		OnTargetInvalidOrOutOfReach();
		return;
	}

	if (DeveloperSettings::Debugging::GHull_Weapons_Compile_DebugSymbols)
	{
		DrawDebugLine(GetWorld(), MeshLocation,
		              MeshLocation + M_TargetDirectionVectorWorldSpace * 100, FColor::Purple, false, 0.33, 1, 5);
	}

	if (not GetIsTargetWithinYaw())
	{
		Debug_HullWeapons("Target not within yaw", FColor::Red);
		AllWeaponsStopFire(false, false);
		return;
	}
	AllWeaponsFire();
}

void UHullWeaponComponent::SpecificEngage()
{
	if (not GetIsValidTurretOwner())
	{
		return;
	}
	if (not M_TargetingData.GetIsTargetValid())
	{
		SpecificEngage_OnTargetInvalid();
		return;
	}

	M_TargetingData.TickAimSelection();
	const FVector TargetLocation = M_TargetingData.GetActiveTargetLocation();
	const FVector OwnerLocation = GetOwner()->GetActorLocation();
	const bool bIsInRange = GetIsTargetInRange(TargetLocation, OwnerLocation);

	if (not GetIsTargetWithinYaw())
	{
		Debug_HullWeapons("Target not within yaw or range", FColor::Red);
		AllWeaponsStopFire(false, false);
		return;
	}
	AllWeaponsFire();
}

void UHullWeaponComponent::SpecificEngage_OnTargetInvalid()
{
	SetAutoEngageTargets(false);
	OnTargetInvalidOrOutOfReach();
	if (TurretOwner)
	{
		// Make sure to not trigger a voice line as we do not know who killed the target just that it is invalid.
		const bool bDestroyedByOwnWeapons = false;
		TurretOwner.GetInterface()->OnMountedWeaponTargetDestroyed(nullptr, this, M_TargetingData.GetTargetActor(),
		                                                           bDestroyedByOwnWeapons);
	}
}

bool UHullWeaponComponent::GetIsTargetInRange(const FVector& TargetLocation, const FVector& WeaponLocation) const
{
	const float DistanceSquared = FVector::DistSquared(TargetLocation, WeaponLocation);
	Debug_HullWeapons("Distance squared to target: " + FString::SanitizeFloat(DistanceSquared)
	                  + "\n Squared range: " + FString::SanitizeFloat(M_WeaponRangeData.M_MaxWeaponRangeSquared),
	                  FColor::Blue);
	if (DistanceSquared > M_WeaponRangeData.M_MaxWeaponRangeSquared)
	{
		return false;
	}
	return true;
}

bool UHullWeaponComponent::GetIsTargetWithinYaw()
{
	if (not GetIsValidHullWeaponMesh())
	{
		return false;
	}
	const bool bHasAnyTarget = M_TargetingData.GetIsTargetValid();
	if (not bHasAnyTarget)
	{
		return false;
	}

	FTransform ParentSocketWorld;
	if (!GetParentSocketWorldTransform(ParentSocketWorld))
	{
		return false;
	}

	const FQuat BaseWorldQuat = ParentSocketWorld.GetRotation() * M_BaseRelativeQuat;
	const FVector BaseFwd = SafePlaneProject(BaseWorldQuat.GetAxisX(), BaseWorldQuat.GetAxisZ());
	const FVector UpAxis = BaseWorldQuat.GetAxisZ();

	const FVector ToTarget = (M_TargetingData.GetActiveTargetLocation() - M_HullWeaponMesh->GetComponentLocation()).
		GetSafeNormal();
	const FVector TgtFlat = SafePlaneProject(ToTarget, UpAxis);

	const double YawDeg = SignedAngleDegAroundAxis(BaseFwd, TgtFlat, UpAxis);

	double MinYaw = M_HullWeaponSettings.MinMaxYaw.X;
	double MaxYaw = M_HullWeaponSettings.MinMaxYaw.Y;
	if (MinYaw > MaxYaw) { Swap(MinYaw, MaxYaw); }

	// (0,0) means "no limit"
	if (FMath::IsNearlyZero(MinYaw) && FMath::IsNearlyZero(MaxYaw))
	{
		return true;
	}

	return (YawDeg >= MinYaw) && (YawDeg <= MaxYaw);
}


void UHullWeaponComponent::OnTargetInvalidOrOutOfReach()
{
	AllWeaponsStopFire(false, false);
	ResetTarget();
	GetClosestTarget();
}

void UHullWeaponComponent::GetClosestTarget()
{
	if (not GetIsValidOwner() || bM_IsPendingTargetSearch || not GetIsValidGameUnitManager())
	{
		return;
	}
	TWeakObjectPtr<UHullWeaponComponent> WeakThis(this);
	const FVector SearchLocation = GetOwner()->GetActorLocation();
	const float SearchRadius = M_WeaponRangeData.M_WeaponSearchRadius;
	const int32 NumTargets = 3;
	bM_IsPendingTargetSearch = true;

	// Request targets from the game unit manager
	M_GameUnitManager->RequestClosestTargets(
		SearchLocation,
		SearchRadius,
		NumTargets,
		M_OwningPlayer,
		TargetPreference,
		[WeakThis](const TArray<AActor*>& Targets)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnTargetsFound(Targets);
			}
		});
}

void UHullWeaponComponent::ResetTarget()
{
	M_TargetingData.ResetTarget();
}

void UHullWeaponComponent::OnTargetsFound(const TArray<AActor*>& Targets)
{
	bM_IsPendingTargetSearch = false;
	for (AActor* Target : Targets)
	{
		if (RTSFunctionLibrary::RTSIsVisibleTarget(Target, M_OwningPlayer))
		{
			M_TargetingData.SetTargetActor(Target);
			return;
		}
	}
}


void UHullWeaponComponent::BeginPlay_SetupCallbackToProjectileManager()
{
	ACPPGameState* GameState = FRTS_Statics::GetGameState(this);
	if (not GameState)
	{
		return;
	}
	TWeakObjectPtr<UHullWeaponComponent> WeakThis(this);
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

void UHullWeaponComponent::OnProjectileManagerLoaded(const TObjectPtr<ASmallArmsProjectileManager>& ProjectileManager)
{
	if (not IsValid(ProjectileManager))
	{
		RTSFunctionLibrary::ReportError("OnProjectileManagerLoaded: ProjectileManager is not valid"
			"\n For HullWeaponComponent: " + GetName());
	}
	M_ProjectileManager = ProjectileManager;
	for (auto EachWeapon : M_TWeapons)
	{
		FRTSWeaponHelpers::SetupProjectileManagerForWeapon(EachWeapon, ProjectileManager);
	}
}

void UHullWeaponComponent::UpdateHullWeaponRangeBasedOnWeapons()
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
			continue;
		}
		M_WeaponRangeData.AdjustRangeForNewWeapon(EachWeapon->GetRange());
	}
}

void UHullWeaponComponent::BeginPlay_SetupGameUnitManager()
{
	const auto GameUnitManager = FRTS_Statics::GetGameUnitManager(this);
	if (not GameUnitManager)
	{
		return;
	}
	M_GameUnitManager = GameUnitManager;
}

bool UHullWeaponComponent::GetIsValidGameUnitManager() const
{
	if (M_GameUnitManager.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(
		"GameUnitManager is not initialized Properly for HullWeaponComponent: " + GetName());
	return false;
}

bool UHullWeaponComponent::GetIsValidOwner() const
{
	if (IsValid(GetOwner()))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("Owner is not valid for HullWeaponComponent: " + GetName());
	return false;
}


bool UHullWeaponComponent::GetParentSocketWorldTransform(FTransform& OutParentSocketWorld) const
{
	if (!GetIsValidHullWeaponMesh()) return false;

	const USceneComponent* Parent = M_HullWeaponMesh->GetAttachParent();
	if (!Parent)
	{
		OutParentSocketWorld = M_HullWeaponMesh->GetComponentTransform();
		return true;
	}

	const FName SocketName = M_HullWeaponMesh->GetAttachSocketName();
	// GetSocketTransform is safe; for components without sockets it returns component transform.
	OutParentSocketWorld = Parent->GetSocketTransform(SocketName, RTS_World);
	return true;
}

FVector UHullWeaponComponent::SafePlaneProject(const FVector& V, const FVector& PlaneNormal)
{
	const FVector P = FVector::VectorPlaneProject(V, PlaneNormal);
	return P.IsNearlyZero() ? V.GetSafeNormal() : P.GetSafeNormal();
}

double UHullWeaponComponent::SignedAngleDegAroundAxis(const FVector& From, const FVector& To, const FVector& Axis) const
{
	// atan2( sign * |cross| , dot ) gives signed angle in [-180, 180]
	const FVector F = From.GetSafeNormal();
	const FVector T = To.GetSafeNormal();
	const FVector C = FVector::CrossProduct(F, T);
	const double d = FMath::Clamp(FVector::DotProduct(F, T), -1.0, 1.0);
	const double s = FVector::DotProduct(Axis.GetSafeNormal(), C);
	const double AngleRad = FMath::Atan2(s, d);
	return FMath::RadiansToDegrees(AngleRad);
}

bool UHullWeaponComponent::ComputeDesiredAim(const FVector& TargetWorld, FQuat& OutDesiredQuat, double& OutYawDeg,
                                             double& OutPitchDeg) const
{
	if (!GetIsValidHullWeaponMesh()) return false;

	//  Get parent socket world transform so the "base" travels with the vehicle.
	FTransform ParentSocketWorld;
	if (!GetParentSocketWorldTransform(ParentSocketWorld)) return false;

	//  Base world orientation: current socket * our initial relative offset
	const FQuat BaseWorldQuat = ParentSocketWorld.GetRotation() * M_BaseRelativeQuat;

	//  Define axes from the base orientation.
	const FVector BaseFwd = BaseWorldQuat.GetAxisX();
	const FVector BaseUp = BaseWorldQuat.GetAxisZ();

	//  Direction to target
	const FVector FromPos = M_HullWeaponMesh->GetComponentLocation();
	const FVector ToTarget = (TargetWorld - FromPos).GetSafeNormal();
	if (!ToTarget.IsNearlyZero())
	{
		const FVector FwdFlat = SafePlaneProject(BaseFwd, BaseUp);
		const FVector TgtFlat = SafePlaneProject(ToTarget, BaseUp);

		double DesiredYawDeg = SignedAngleDegAroundAxis(FwdFlat, TgtFlat, BaseUp);

		double MinYaw = M_HullWeaponSettings.MinMaxYaw.X;
		double MaxYaw = M_HullWeaponSettings.MinMaxYaw.Y;

		double ClampedYawDeg = FMath::Clamp(DesiredYawDeg, MinYaw, MaxYaw);

		//  Apply yaw about BaseUp to build a yaw-only orientation.
		const FQuat YawQuat(BaseUp, FMath::DegreesToRadians(ClampedYawDeg));
		FQuat YawWorldQuat = YawQuat * BaseWorldQuat;

		// PITCH: now measure around the *current right* axis after yaw
		const FVector YawFwd = YawWorldQuat.GetAxisX();
		const FVector YawRight = YawWorldQuat.GetAxisY();

		// Signed pitch from yaw-aligned forward to full target, around Right
		double DesiredPitchDeg = SignedAngleDegAroundAxis(YawFwd, ToTarget, YawRight);


		double MinPitch = M_HullWeaponSettings.MinMaxPitch.X;
		double MaxPitch = M_HullWeaponSettings.MinMaxPitch.Y;
		double ClampedPitchDeg = FMath::Clamp(DesiredPitchDeg, MinPitch, MaxPitch);

		// 8) Apply pitch about Right
		const FQuat PitchQuat(YawRight, FMath::DegreesToRadians(ClampedPitchDeg));
		OutDesiredQuat = PitchQuat * YawWorldQuat;

		OutYawDeg = ClampedYawDeg;
		OutPitchDeg = ClampedPitchDeg;
		return true;
	}
	return false;
}


void UHullWeaponComponent::Debug_HullWeapons(const FString& DebugMessage, const FColor& Color) const
{
	if (DeveloperSettings::Debugging::GHull_Weapons_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString(DebugMessage, Color);
	}
}
