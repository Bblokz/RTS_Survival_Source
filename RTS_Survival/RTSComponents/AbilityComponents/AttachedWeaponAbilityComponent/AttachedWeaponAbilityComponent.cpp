// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "AttachedWeaponAbilityComponent.h"

#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/WeaponData/MultiProjectileWeapon/UWeaponStateMultiProjectile.h"
#include "RTS_Survival/Weapons/WeaponData/MultiTraceWeapon/WeaponStateMultiTrace.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponSystems.h"
#include "RTS_Survival/Weapons/FlameThrowerWeapon/UWeaponStateFlameThrower.h"
#include "RTS_Survival/Weapons/LaserWeapon/UWeaponStateLaser.h"
#include "RTS_Survival/Weapons/LaserWeapon/UWeaponStateMultiHitLaser.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"

FAttachedWeaponAbilitySettings::FAttachedWeaponAbilitySettings()
	: WeaponAbilityType(EAttachWeaponAbilitySubType::Pz38AttachedMortarDefault)
	, PreferredAbilityIndex(INDEX_NONE)
	, TotalWeaponFireTime(2.0f)
	, Cooldown(5)
	, AimAssistType(EPlayerAimAbilityTypes::None)
	, Radius(300.0f)
{
}

UAttachedWeaponAbilityComponent::UAttachedWeaponAbilityComponent()
	: M_AttachedWeaponAbilitySettings()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAttachedWeaponAbilityComponent::PostInitProperties()
{
	Super::PostInitProperties();
	if (not GetOwner())
	{
		return;
	}
	ICommands* CommandsInterface = Cast<ICommands>(GetOwner());
	if (CommandsInterface)
	{
		M_OwnerCommandsInterface.SetInterface(CommandsInterface);
		M_OwnerCommandsInterface.SetObject(GetOwner());
	}
}

void UAttachedWeaponAbilityComponent::BeginPlay()
{
	Super::BeginPlay();
	(void)GetIsValidOwnerCommandsInterface();
	BeginPlay_AddAbility();
}

void UAttachedWeaponAbilityComponent::ExecuteAttachedWeaponAbility(const FVector& TargetLocation)
{
	if (TargetLocation.IsNearlyZero())
	{
		return;
	}

	if (M_AttachedWeapons.IsEmpty())
	{
		return;
	}

	if (M_FireState.bM_IsFiring)
	{
		StopAttachedWeaponAbilityFire();
	}

	M_FireState.M_TargetLocation = TargetLocation;
	M_TargetingData.SetTargetGround(TargetLocation);
	ForceReloadAllWeapons();
	StartFireLoop();
}

void UAttachedWeaponAbilityComponent::StopAttachedWeaponAbilityFire()
{
	ClearFireTimers();
	StopAllWeaponsFire();
	M_FireState.bM_IsFiring = false;
}

EAttachWeaponAbilitySubType UAttachedWeaponAbilityComponent::GetAttachedWeaponAbilityType() const
{
	return M_AttachedWeaponAbilitySettings.WeaponAbilityType;
}

EPlayerAimAbilityTypes UAttachedWeaponAbilityComponent::GetAimAssistType() const
{
	return M_AttachedWeaponAbilitySettings.AimAssistType;
}

float UAttachedWeaponAbilityComponent::GetAttachedWeaponAbilityRange() const
{
	return M_AbilityRange;
}

float UAttachedWeaponAbilityComponent::GetAttachedWeaponAbilityRadius() const
{
	return M_AttachedWeaponAbilitySettings.Radius;
}

void UAttachedWeaponAbilityComponent::InitWithExistingMesh(UMeshComponent* ExistingMesh)
{
	if (not IsValid(ExistingMesh))
	{
		RTSFunctionLibrary::ReportError("AttachedWeaponAbilityComponent: Existing mesh is invalid.");
		M_WeaponMeshSetupState = EAttachedWeaponAbilityMeshSetup::Uninitialized;
		return;
	}

	M_WeaponMeshSetupState = EAttachedWeaponAbilityMeshSetup::ExistingMesh;
	M_ExistingWeaponMesh = ExistingMesh;
	ProcessPendingWeaponSetups();
}

void UAttachedWeaponAbilityComponent::InitWithSpawnedMesh(UStaticMesh* MeshToSpawn, const FTransform& RelativeTransform)
{
	M_WeaponMeshSetupState = EAttachedWeaponAbilityMeshSetup::SpawnedMesh;

	if (not IsValid(MeshToSpawn))
	{
		RTSFunctionLibrary::ReportError("AttachedWeaponAbilityComponent: MeshToSpawn is invalid.");
		M_WeaponMeshSetupState = EAttachedWeaponAbilityMeshSetup::Uninitialized;
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (not IsValid(OwnerActor))
	{
		RTSFunctionLibrary::ReportError("AttachedWeaponAbilityComponent: Owner is invalid when spawning mesh.");
		M_WeaponMeshSetupState = EAttachedWeaponAbilityMeshSetup::Uninitialized;
		return;
	}

	USceneComponent* RootComponent = OwnerActor->GetRootComponent();
	if (not IsValid(RootComponent))
	{
		RTSFunctionLibrary::ReportError("AttachedWeaponAbilityComponent: Owner root component is invalid.");
		M_WeaponMeshSetupState = EAttachedWeaponAbilityMeshSetup::Uninitialized;
		return;
	}

	UStaticMeshComponent* NewMeshComponent = NewObject<UStaticMeshComponent>(OwnerActor);
	if (not IsValid(NewMeshComponent))
	{
		RTSFunctionLibrary::ReportError("AttachedWeaponAbilityComponent: Failed to create spawned mesh component.");
		M_WeaponMeshSetupState = EAttachedWeaponAbilityMeshSetup::Uninitialized;
		return;
	}

	NewMeshComponent->SetStaticMesh(MeshToSpawn);
	NewMeshComponent->SetupAttachment(RootComponent);
	NewMeshComponent->SetRelativeTransform(RelativeTransform);
	NewMeshComponent->RegisterComponent();

	M_SpawnedWeaponMesh = NewMeshComponent;
	ProcessPendingWeaponSetups();
}

TArray<UWeaponState*> UAttachedWeaponAbilityComponent::GetWeapons()
{
	TArray<UWeaponState*> Weapons;
	Weapons.Reserve(M_AttachedWeapons.Num());
	for (const TObjectPtr<UWeaponState>& Weapon : M_AttachedWeapons)
	{
		if (IsValid(Weapon))
		{
			Weapons.Add(Weapon);
		}
	}
	return Weapons;
}

void UAttachedWeaponAbilityComponent::RegisterIgnoreActor(AActor* ActorToIgnore, const bool bRegister)
{
	if (not RTSFunctionLibrary::RTSIsValid(ActorToIgnore))
	{
		return;
	}
	for (const TObjectPtr<UWeaponState>& Weapon : M_AttachedWeapons)
	{
		if (not IsValid(Weapon))
		{
			continue;
		}
		Weapon->RegisterActorToIgnore(ActorToIgnore, bRegister);
	}
}

void UAttachedWeaponAbilityComponent::OnWeaponAdded(const int32 WeaponIndex, UWeaponState* Weapon)
{
}

FVector& UAttachedWeaponAbilityComponent::GetFireDirection(const int32 WeaponIndex)
{
	return M_TargetDirectionVectorWorldSpace;
}

FVector& UAttachedWeaponAbilityComponent::GetTargetLocation(const int32 WeaponIndex)
{
	return M_TargetingData.GetActiveTargetLocation();
}

bool UAttachedWeaponAbilityComponent::AllowWeaponToReload(const int32 WeaponIndex) const
{
	return true;
}

void UAttachedWeaponAbilityComponent::OnWeaponKilledActor(const int32 WeaponIndex, AActor* KilledActor)
{
}

void UAttachedWeaponAbilityComponent::PlayWeaponAnimation(const int32 WeaponIndex, const EWeaponFireMode FireMode)
{
}

void UAttachedWeaponAbilityComponent::OnReloadStart(const int32 WeaponIndex, const float ReloadTime)
{
}

void UAttachedWeaponAbilityComponent::OnReloadFinished(const int32 WeaponIndex)
{
}

void UAttachedWeaponAbilityComponent::ForceSetAllWeaponsFullyReloaded()
{
	ForceReloadAllWeapons();
}

void UAttachedWeaponAbilityComponent::OnProjectileHit(const bool bBounced)
{
}

void UAttachedWeaponAbilityComponent::SetupDirectHitWeapon(FInitWeaponStateDirectHit DirectHitWeaponParameters)
{
	if (not TryPrepareWeaponParameters(DirectHitWeaponParameters, "SetupDirectHitWeapon"))
	{
		if (M_WeaponMeshSetupState == EAttachedWeaponAbilityMeshSetup::Uninitialized)
		{
			M_PendingDirectHitWeapons.Add(DirectHitWeaponParameters);
		}
		return;
	}
	SetupDirectHitWeaponInternal(DirectHitWeaponParameters);
}

void UAttachedWeaponAbilityComponent::SetupTraceWeapon(FInitWeaponStatTrace TraceWeaponParameters)
{
	if (not TryPrepareWeaponParameters(TraceWeaponParameters, "SetupTraceWeapon"))
	{
		if (M_WeaponMeshSetupState == EAttachedWeaponAbilityMeshSetup::Uninitialized)
		{
			M_PendingTraceWeapons.Add(TraceWeaponParameters);
		}
		return;
	}
	SetupTraceWeaponInternal(TraceWeaponParameters);
}

void UAttachedWeaponAbilityComponent::SetupMultiTraceWeapon(FInitWeaponStateMultiTrace MultiTraceWeaponParameters)
{
	if (not TryPrepareWeaponParameters(MultiTraceWeaponParameters, "SetupMultiTraceWeapon"))
	{
		if (M_WeaponMeshSetupState == EAttachedWeaponAbilityMeshSetup::Uninitialized)
		{
			M_PendingMultiTraceWeapons.Add(MultiTraceWeaponParameters);
		}
		return;
	}
	SetupMultiTraceWeaponInternal(MultiTraceWeaponParameters);
}

void UAttachedWeaponAbilityComponent::SetupLaserWeapon(const FInitWeaponStateLaser& LaserWeaponParameters)
{
	FInitWeaponStateLaser Parameters = LaserWeaponParameters;
	if (not TryPrepareWeaponParameters(Parameters, "SetupLaserWeapon"))
	{
		if (M_WeaponMeshSetupState == EAttachedWeaponAbilityMeshSetup::Uninitialized)
		{
			M_PendingLaserWeapons.Add(Parameters);
		}
		return;
	}
	SetupLaserWeaponInternal(Parameters);
}

void UAttachedWeaponAbilityComponent::SetupMultiHitLaserWeapon(const FInitWeaponStateMultiHitLaser& LaserWeaponParameters)
{
	FInitWeaponStateMultiHitLaser Parameters = LaserWeaponParameters;
	if (not TryPrepareWeaponParameters(Parameters, "SetupMultiHitLaserWeapon"))
	{
		if (M_WeaponMeshSetupState == EAttachedWeaponAbilityMeshSetup::Uninitialized)
		{
			M_PendingMultiHitLaserWeapons.Add(Parameters);
		}
		return;
	}
	SetupMultiHitLaserWeaponInternal(Parameters);
}

void UAttachedWeaponAbilityComponent::SetupFlameThrowerWeapon(const FInitWeaponStateFlameThrower& FlameWeaponParameters)
{
	FInitWeaponStateFlameThrower Parameters = FlameWeaponParameters;
	if (not TryPrepareWeaponParameters(Parameters, "SetupFlameThrowerWeapon"))
	{
		if (M_WeaponMeshSetupState == EAttachedWeaponAbilityMeshSetup::Uninitialized)
		{
			M_PendingFlameThrowerWeapons.Add(Parameters);
		}
		return;
	}
	SetupFlameThrowerWeaponInternal(Parameters);
}

void UAttachedWeaponAbilityComponent::SetupProjectileWeapon(FInitWeaponStateProjectile ProjectileWeaponParameters)
{
	if (not TryPrepareWeaponParameters(ProjectileWeaponParameters, "SetupProjectileWeapon"))
	{
		if (M_WeaponMeshSetupState == EAttachedWeaponAbilityMeshSetup::Uninitialized)
		{
			M_PendingProjectileWeapons.Add(ProjectileWeaponParameters);
		}
		return;
	}
	SetupProjectileWeaponInternal(ProjectileWeaponParameters);
}

void UAttachedWeaponAbilityComponent::SetupRocketProjectileWeapon(FInitWeaponStateRocketProjectile RocketProjectileParameters)
{
	if (not TryPrepareWeaponParameters(RocketProjectileParameters, "SetupRocketProjectileWeapon"))
	{
		if (M_WeaponMeshSetupState == EAttachedWeaponAbilityMeshSetup::Uninitialized)
		{
			M_PendingRocketWeapons.Add(RocketProjectileParameters);
		}
		return;
	}
	SetupRocketProjectileWeaponInternal(RocketProjectileParameters);
}

void UAttachedWeaponAbilityComponent::SetupMultiProjectileWeapon(FInitWeaponStateMultiProjectile MultiProjectileState)
{
	if (not TryPrepareWeaponParameters(MultiProjectileState, "SetupMultiProjectileWeapon"))
	{
		if (M_WeaponMeshSetupState == EAttachedWeaponAbilityMeshSetup::Uninitialized)
		{
			M_PendingMultiProjectileWeapons.Add(MultiProjectileState);
		}
		return;
	}
	SetupMultiProjectileWeaponInternal(MultiProjectileState);
}

void UAttachedWeaponAbilityComponent::SetupArchProjectileWeapon(FInitWeaponStateArchProjectile ArchProjParameters)
{
	if (not TryPrepareWeaponParameters(ArchProjParameters, "SetupArchProjectileWeapon"))
	{
		if (M_WeaponMeshSetupState == EAttachedWeaponAbilityMeshSetup::Uninitialized)
		{
			M_PendingArchProjectileWeapons.Add(ArchProjParameters);
		}
		return;
	}
	SetupArchProjectileWeaponInternal(ArchProjParameters);
}

void UAttachedWeaponAbilityComponent::SetupPooledArchProjectileWeapon(FInitWeaponStateArchProjectile ArchProjParameters)
{
	SetupArchProjectileWeapon(ArchProjParameters);
}

void UAttachedWeaponAbilityComponent::BeginPlay_AddAbility()
{
	AActor* OwnerActor = GetOwner();
	if (not IsValid(OwnerActor))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not World)
	{
		return;
	}

	FTimerDelegate NextTickDelegate;
	TWeakObjectPtr<UAttachedWeaponAbilityComponent> WeakThis(this);
	NextTickDelegate.BindLambda([WeakThis]()
	{
		if (not WeakThis.IsValid())
		{
			return;
		}
		WeakThis->AddAbilityToCommands();
	});
	World->GetTimerManager().SetTimerForNextTick(NextTickDelegate);
}

void UAttachedWeaponAbilityComponent::AddAbilityToCommands()
{
	if (not GetIsValidOwnerCommandsInterface())
	{
		return;
	}

	FUnitAbilityEntry AbilityEntry;
	AbilityEntry.AbilityId = EAbilityID::IdAttachedWeapon;
	AbilityEntry.CustomType = static_cast<int32>(M_AttachedWeaponAbilitySettings.WeaponAbilityType);
	AbilityEntry.CooldownDuration = M_AttachedWeaponAbilitySettings.Cooldown;
	M_OwnerCommandsInterface->AddAbility(AbilityEntry, M_AttachedWeaponAbilitySettings.PreferredAbilityIndex);
}

bool UAttachedWeaponAbilityComponent::GetIsValidOwnerCommandsInterface() const
{
	if (not IsValid(M_OwnerCommandsInterface.GetObject()))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"M_OwnerCommandsInterface",
			"UAttachedWeaponAbilityComponent::GetIsValidOwnerCommandsInterface",
			this);
		return false;
	}

	return true;
}

void UAttachedWeaponAbilityComponent::StartFireLoop()
{
	if (M_FireState.bM_IsFiring)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not World)
	{
		return;
	}

	if (M_AttachedWeaponAbilitySettings.TotalWeaponFireTime <= 0.0f)
	{
		RTSFunctionLibrary::ReportError("AttachedWeaponAbilityComponent: TotalWeaponFireTime must be > 0.");
		return;
	}

	HandleFireLoop();

	FTimerDelegate FireLoopDelegate;
	TWeakObjectPtr<UAttachedWeaponAbilityComponent> WeakThis(this);
	FireLoopDelegate.BindLambda([WeakThis]()
	{
		if (not WeakThis.IsValid())
		{
			return;
		}
		WeakThis->HandleFireLoop();
	});
	World->GetTimerManager().SetTimer(
		M_FireState.M_FireLoopTimerHandle,
		FireLoopDelegate,
		M_FireLoopIntervalSeconds,
		true);

	FTimerDelegate StopFireDelegate;
	StopFireDelegate.BindLambda([WeakThis]()
	{
		if (not WeakThis.IsValid())
		{
			return;
		}
		WeakThis->HandleStopFiring();
	});
	World->GetTimerManager().SetTimer(
		M_FireState.M_StopFireTimerHandle,
		StopFireDelegate,
		M_AttachedWeaponAbilitySettings.TotalWeaponFireTime,
		false);

	M_FireState.bM_IsFiring = true;
}

void UAttachedWeaponAbilityComponent::HandleFireLoop()
{
	if (not IsValid(GetOwner()))
	{
		return;
	}

	const FVector OwnerLocation = GetOwner()->GetActorLocation();
	const FVector TargetOffset = M_FireState.M_TargetLocation - OwnerLocation;
	const float TargetOffsetSize = TargetOffset.Size();
	if (TargetOffsetSize <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	M_TargetDirectionVectorWorldSpace = TargetOffset / TargetOffsetSize;

	for (const TObjectPtr<UWeaponState>& Weapon : M_AttachedWeapons)
	{
		if (not IsValid(Weapon))
		{
			continue;
		}
		Weapon->Fire(M_FireState.M_TargetLocation);
	}
}

void UAttachedWeaponAbilityComponent::HandleStopFiring()
{
	StopAttachedWeaponAbilityFire();
}

void UAttachedWeaponAbilityComponent::ClearFireTimers()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_FireState.M_FireLoopTimerHandle);
		World->GetTimerManager().ClearTimer(M_FireState.M_StopFireTimerHandle);
	}
}

void UAttachedWeaponAbilityComponent::ForceReloadAllWeapons()
{
	for (const TObjectPtr<UWeaponState>& Weapon : M_AttachedWeapons)
	{
		if (not IsValid(Weapon))
		{
			continue;
		}
		Weapon->ForceInstantReload();
	}
}

void UAttachedWeaponAbilityComponent::StopAllWeaponsFire()
{
	for (const TObjectPtr<UWeaponState>& Weapon : M_AttachedWeapons)
	{
		if (not IsValid(Weapon))
		{
			continue;
		}
		Weapon->StopFire(false, true);
	}
}

void UAttachedWeaponAbilityComponent::UpdateAbilityRangeFromWeapons()
{
	float MaxRange = 0.0f;
	for (const TObjectPtr<UWeaponState>& Weapon : M_AttachedWeapons)
	{
		if (not IsValid(Weapon))
		{
			continue;
		}
		MaxRange = FMath::Max(MaxRange, Weapon->GetRange());
	}
	M_AbilityRange = MaxRange;
}

void UAttachedWeaponAbilityComponent::ProcessPendingWeaponSetups()
{
	if (M_WeaponMeshSetupState == EAttachedWeaponAbilityMeshSetup::Uninitialized)
	{
		return;
	}

	ProcessPendingDirectHitWeapons();
	ProcessPendingTraceWeapons();
	ProcessPendingMultiTraceWeapons();
	ProcessPendingLaserWeapons();
	ProcessPendingMultiHitLaserWeapons();
	ProcessPendingFlameThrowerWeapons();
	ProcessPendingProjectileWeapons();
	ProcessPendingRocketWeapons();
	ProcessPendingMultiProjectileWeapons();
	ProcessPendingArchProjectileWeapons();
}

void UAttachedWeaponAbilityComponent::ProcessPendingDirectHitWeapons()
{
	for (const FInitWeaponStateDirectHit& PendingDirectHit : M_PendingDirectHitWeapons)
	{
		FInitWeaponStateDirectHit Parameters = PendingDirectHit;
		if (TryPrepareWeaponParameters(Parameters, "ProcessPendingDirectHitWeapons"))
		{
			SetupDirectHitWeaponInternal(Parameters);
		}
	}
	M_PendingDirectHitWeapons.Reset();
}

void UAttachedWeaponAbilityComponent::ProcessPendingTraceWeapons()
{
	for (const FInitWeaponStatTrace& PendingTrace : M_PendingTraceWeapons)
	{
		FInitWeaponStatTrace Parameters = PendingTrace;
		if (TryPrepareWeaponParameters(Parameters, "ProcessPendingTraceWeapons"))
		{
			SetupTraceWeaponInternal(Parameters);
		}
	}
	M_PendingTraceWeapons.Reset();
}

void UAttachedWeaponAbilityComponent::ProcessPendingMultiTraceWeapons()
{
	for (const FInitWeaponStateMultiTrace& PendingTrace : M_PendingMultiTraceWeapons)
	{
		FInitWeaponStateMultiTrace Parameters = PendingTrace;
		if (TryPrepareWeaponParameters(Parameters, "ProcessPendingMultiTraceWeapons"))
		{
			SetupMultiTraceWeaponInternal(Parameters);
		}
	}
	M_PendingMultiTraceWeapons.Reset();
}

void UAttachedWeaponAbilityComponent::ProcessPendingLaserWeapons()
{
	for (const FInitWeaponStateLaser& PendingLaser : M_PendingLaserWeapons)
	{
		FInitWeaponStateLaser Parameters = PendingLaser;
		if (TryPrepareWeaponParameters(Parameters, "ProcessPendingLaserWeapons"))
		{
			SetupLaserWeaponInternal(Parameters);
		}
	}
	M_PendingLaserWeapons.Reset();
}

void UAttachedWeaponAbilityComponent::ProcessPendingMultiHitLaserWeapons()
{
	for (const FInitWeaponStateMultiHitLaser& PendingLaser : M_PendingMultiHitLaserWeapons)
	{
		FInitWeaponStateMultiHitLaser Parameters = PendingLaser;
		if (TryPrepareWeaponParameters(Parameters, "ProcessPendingMultiHitLaserWeapons"))
		{
			SetupMultiHitLaserWeaponInternal(Parameters);
		}
	}
	M_PendingMultiHitLaserWeapons.Reset();
}

void UAttachedWeaponAbilityComponent::ProcessPendingFlameThrowerWeapons()
{
	for (const FInitWeaponStateFlameThrower& PendingFlame : M_PendingFlameThrowerWeapons)
	{
		FInitWeaponStateFlameThrower Parameters = PendingFlame;
		if (TryPrepareWeaponParameters(Parameters, "ProcessPendingFlameThrowerWeapons"))
		{
			SetupFlameThrowerWeaponInternal(Parameters);
		}
	}
	M_PendingFlameThrowerWeapons.Reset();
}

void UAttachedWeaponAbilityComponent::ProcessPendingProjectileWeapons()
{
	for (const FInitWeaponStateProjectile& PendingProjectile : M_PendingProjectileWeapons)
	{
		FInitWeaponStateProjectile Parameters = PendingProjectile;
		if (TryPrepareWeaponParameters(Parameters, "ProcessPendingProjectileWeapons"))
		{
			SetupProjectileWeaponInternal(Parameters);
		}
	}
	M_PendingProjectileWeapons.Reset();
}

void UAttachedWeaponAbilityComponent::ProcessPendingRocketWeapons()
{
	for (const FInitWeaponStateRocketProjectile& PendingRocket : M_PendingRocketWeapons)
	{
		FInitWeaponStateRocketProjectile Parameters = PendingRocket;
		if (TryPrepareWeaponParameters(Parameters, "ProcessPendingRocketWeapons"))
		{
			SetupRocketProjectileWeaponInternal(Parameters);
		}
	}
	M_PendingRocketWeapons.Reset();
}

void UAttachedWeaponAbilityComponent::ProcessPendingMultiProjectileWeapons()
{
	for (const FInitWeaponStateMultiProjectile& PendingMultiProjectile : M_PendingMultiProjectileWeapons)
	{
		FInitWeaponStateMultiProjectile Parameters = PendingMultiProjectile;
		if (TryPrepareWeaponParameters(Parameters, "ProcessPendingMultiProjectileWeapons"))
		{
			SetupMultiProjectileWeaponInternal(Parameters);
		}
	}
	M_PendingMultiProjectileWeapons.Reset();
}

void UAttachedWeaponAbilityComponent::ProcessPendingArchProjectileWeapons()
{
	for (const FInitWeaponStateArchProjectile& PendingArch : M_PendingArchProjectileWeapons)
	{
		FInitWeaponStateArchProjectile Parameters = PendingArch;
		if (TryPrepareWeaponParameters(Parameters, "ProcessPendingArchProjectileWeapons"))
		{
			SetupArchProjectileWeaponInternal(Parameters);
		}
	}
	M_PendingArchProjectileWeapons.Reset();
}

void UAttachedWeaponAbilityComponent::ReportMissingInit(const FString& SetupFunctionName) const
{
	RTSFunctionLibrary::ReportError(
		"AttachedWeaponAbilityComponent: Init function must be called before " + SetupFunctionName);
}

bool UAttachedWeaponAbilityComponent::TryPrepareWeaponParameters(FInitWeaponStateDirectHit& WeaponParameters,
                                                                 const FString& FunctionName)
{
	CacheWeaponOwnerInParameters(WeaponParameters);

	if (M_WeaponMeshSetupState == EAttachedWeaponAbilityMeshSetup::Uninitialized)
	{
		ReportMissingInit(FunctionName);
		return false;
	}

	UMeshComponent* MeshComponent = WeaponParameters.MeshComponent;
	const bool bMeshReady = (M_WeaponMeshSetupState == EAttachedWeaponAbilityMeshSetup::ExistingMesh)
		? PrepareExistingMeshParameters(MeshComponent, FunctionName)
		: PrepareSpawnedMeshParameters(MeshComponent, FunctionName);
	WeaponParameters.MeshComponent = MeshComponent;
	return bMeshReady;
}

bool UAttachedWeaponAbilityComponent::TryPrepareWeaponParameters(FInitWeaponStatTrace& WeaponParameters,
                                                                 const FString& FunctionName)
{
	CacheWeaponOwnerInParameters(WeaponParameters);

	if (M_WeaponMeshSetupState == EAttachedWeaponAbilityMeshSetup::Uninitialized)
	{
		ReportMissingInit(FunctionName);
		return false;
	}

	UMeshComponent* MeshComponent = WeaponParameters.MeshComponent;
	const bool bMeshReady = (M_WeaponMeshSetupState == EAttachedWeaponAbilityMeshSetup::ExistingMesh)
		? PrepareExistingMeshParameters(MeshComponent, FunctionName)
		: PrepareSpawnedMeshParameters(MeshComponent, FunctionName);
	WeaponParameters.MeshComponent = MeshComponent;
	return bMeshReady;
}

bool UAttachedWeaponAbilityComponent::TryPrepareWeaponParameters(FInitWeaponStateMultiTrace& WeaponParameters,
                                                                 const FString& FunctionName)
{
	CacheWeaponOwnerInParameters(WeaponParameters);

	if (M_WeaponMeshSetupState == EAttachedWeaponAbilityMeshSetup::Uninitialized)
	{
		ReportMissingInit(FunctionName);
		return false;
	}

	UMeshComponent* MeshComponent = WeaponParameters.MeshComponent;
	const bool bMeshReady = (M_WeaponMeshSetupState == EAttachedWeaponAbilityMeshSetup::ExistingMesh)
		? PrepareExistingMeshParameters(MeshComponent, FunctionName)
		: PrepareSpawnedMeshParameters(MeshComponent, FunctionName);
	WeaponParameters.MeshComponent = MeshComponent;
	return bMeshReady;
}

bool UAttachedWeaponAbilityComponent::TryPrepareWeaponParameters(FInitWeaponStateProjectile& WeaponParameters,
                                                                 const FString& FunctionName)
{
	CacheWeaponOwnerInParameters(WeaponParameters);

	if (M_WeaponMeshSetupState == EAttachedWeaponAbilityMeshSetup::Uninitialized)
	{
		ReportMissingInit(FunctionName);
		return false;
	}

	UMeshComponent* MeshComponent = WeaponParameters.MeshComponent;
	const bool bMeshReady = (M_WeaponMeshSetupState == EAttachedWeaponAbilityMeshSetup::ExistingMesh)
		? PrepareExistingMeshParameters(MeshComponent, FunctionName)
		: PrepareSpawnedMeshParameters(MeshComponent, FunctionName);
	WeaponParameters.MeshComponent = MeshComponent;
	return bMeshReady;
}

bool UAttachedWeaponAbilityComponent::TryPrepareWeaponParameters(FInitWeaponStateRocketProjectile& WeaponParameters,
                                                                 const FString& FunctionName)
{
	CacheWeaponOwnerInParameters(WeaponParameters);

	if (M_WeaponMeshSetupState == EAttachedWeaponAbilityMeshSetup::Uninitialized)
	{
		ReportMissingInit(FunctionName);
		return false;
	}

	UMeshComponent* MeshComponent = WeaponParameters.MeshComponent;
	const bool bMeshReady = (M_WeaponMeshSetupState == EAttachedWeaponAbilityMeshSetup::ExistingMesh)
		? PrepareExistingMeshParameters(MeshComponent, FunctionName)
		: PrepareSpawnedMeshParameters(MeshComponent, FunctionName);
	WeaponParameters.MeshComponent = MeshComponent;
	return bMeshReady;
}

bool UAttachedWeaponAbilityComponent::TryPrepareWeaponParameters(FInitWeaponStateMultiProjectile& WeaponParameters,
                                                                 const FString& FunctionName)
{
	CacheWeaponOwnerInParameters(WeaponParameters);

	if (M_WeaponMeshSetupState == EAttachedWeaponAbilityMeshSetup::Uninitialized)
	{
		ReportMissingInit(FunctionName);
		return false;
	}

	UMeshComponent* MeshComponent = WeaponParameters.MeshComponent;
	const bool bMeshReady = (M_WeaponMeshSetupState == EAttachedWeaponAbilityMeshSetup::ExistingMesh)
		? PrepareExistingMeshParameters(MeshComponent, FunctionName)
		: PrepareSpawnedMeshParameters(MeshComponent, FunctionName);
	WeaponParameters.MeshComponent = MeshComponent;
	return bMeshReady;
}

bool UAttachedWeaponAbilityComponent::TryPrepareWeaponParameters(FInitWeaponStateArchProjectile& WeaponParameters,
                                                                 const FString& FunctionName)
{
	CacheWeaponOwnerInParameters(WeaponParameters);

	if (M_WeaponMeshSetupState == EAttachedWeaponAbilityMeshSetup::Uninitialized)
	{
		ReportMissingInit(FunctionName);
		return false;
	}

	UMeshComponent* MeshComponent = WeaponParameters.MeshComponent;
	const bool bMeshReady = (M_WeaponMeshSetupState == EAttachedWeaponAbilityMeshSetup::ExistingMesh)
		? PrepareExistingMeshParameters(MeshComponent, FunctionName)
		: PrepareSpawnedMeshParameters(MeshComponent, FunctionName);
	WeaponParameters.MeshComponent = MeshComponent;
	return bMeshReady;
}

bool UAttachedWeaponAbilityComponent::TryPrepareWeaponParameters(FInitWeaponStateLaser& WeaponParameters,
                                                                 const FString& FunctionName)
{
	CacheWeaponOwnerInParameters(WeaponParameters);

	if (M_WeaponMeshSetupState == EAttachedWeaponAbilityMeshSetup::Uninitialized)
	{
		ReportMissingInit(FunctionName);
		return false;
	}

	UMeshComponent* MeshComponent = WeaponParameters.MeshComponent;
	const bool bMeshReady = (M_WeaponMeshSetupState == EAttachedWeaponAbilityMeshSetup::ExistingMesh)
		? PrepareExistingMeshParameters(MeshComponent, FunctionName)
		: PrepareSpawnedMeshParameters(MeshComponent, FunctionName);
	WeaponParameters.MeshComponent = MeshComponent;
	return bMeshReady;
}

bool UAttachedWeaponAbilityComponent::TryPrepareWeaponParameters(FInitWeaponStateMultiHitLaser& WeaponParameters,
                                                                 const FString& FunctionName)
{
	CacheWeaponOwnerInParameters(WeaponParameters);

	if (M_WeaponMeshSetupState == EAttachedWeaponAbilityMeshSetup::Uninitialized)
	{
		ReportMissingInit(FunctionName);
		return false;
	}

	UMeshComponent* MeshComponent = WeaponParameters.MeshComponent;
	const bool bMeshReady = (M_WeaponMeshSetupState == EAttachedWeaponAbilityMeshSetup::ExistingMesh)
		? PrepareExistingMeshParameters(MeshComponent, FunctionName)
		: PrepareSpawnedMeshParameters(MeshComponent, FunctionName);
	WeaponParameters.MeshComponent = MeshComponent;
	return bMeshReady;
}

bool UAttachedWeaponAbilityComponent::TryPrepareWeaponParameters(FInitWeaponStateFlameThrower& WeaponParameters,
                                                                 const FString& FunctionName)
{
	CacheWeaponOwnerInParameters(WeaponParameters);

	if (M_WeaponMeshSetupState == EAttachedWeaponAbilityMeshSetup::Uninitialized)
	{
		ReportMissingInit(FunctionName);
		return false;
	}

	UMeshComponent* MeshComponent = WeaponParameters.MeshComponent;
	const bool bMeshReady = (M_WeaponMeshSetupState == EAttachedWeaponAbilityMeshSetup::ExistingMesh)
		? PrepareExistingMeshParameters(MeshComponent, FunctionName)
		: PrepareSpawnedMeshParameters(MeshComponent, FunctionName);
	WeaponParameters.MeshComponent = MeshComponent;
	return bMeshReady;
}

bool UAttachedWeaponAbilityComponent::PrepareExistingMeshParameters(UMeshComponent*& MeshComponent,
                                                                    const FString& FunctionName) const
{
	if (not GetIsValidExistingMesh())
	{
		return false;
	}

	if (not IsValid(MeshComponent))
	{
		RTSFunctionLibrary::ReportError(
			"AttachedWeaponAbilityComponent: Existing mesh was not provided in setup parameters for " + FunctionName);
	}

	MeshComponent = M_ExistingWeaponMesh.Get();
	return IsValid(MeshComponent);
}

bool UAttachedWeaponAbilityComponent::PrepareSpawnedMeshParameters(UMeshComponent*& MeshComponent,
                                                                   const FString& FunctionName) const
{
	if (not GetIsValidSpawnedMesh())
	{
		RTSFunctionLibrary::ReportError(
			"AttachedWeaponAbilityComponent: Spawned weapon mesh is invalid for " + FunctionName);
		return false;
	}

	MeshComponent = M_SpawnedWeaponMesh;
	return true;
}

bool UAttachedWeaponAbilityComponent::GetIsValidExistingMesh() const
{
	if (not M_ExistingWeaponMesh.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"M_ExistingWeaponMesh",
			"UAttachedWeaponAbilityComponent::GetIsValidExistingMesh",
			this);
		return false;
	}

	return true;
}

bool UAttachedWeaponAbilityComponent::GetIsValidSpawnedMesh() const
{
	if (not IsValid(M_SpawnedWeaponMesh))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"M_SpawnedWeaponMesh",
			"UAttachedWeaponAbilityComponent::GetIsValidSpawnedMesh",
			this);
		return false;
	}

	return true;
}

void UAttachedWeaponAbilityComponent::SetupDirectHitWeaponInternal(
	const FInitWeaponStateDirectHit& DirectHitWeaponParameters)
{
	const int32 WeaponIndex = M_AttachedWeapons.Num();
	M_OwningPlayer = DirectHitWeaponParameters.OwningPlayer;
	M_TargetingData.InitTargetStruct(M_OwningPlayer);

	UWeaponStateDirectHit* DirectHitWeapon = NewObject<UWeaponStateDirectHit>(this);
	DirectHitWeapon->InitDirectHitWeapon(
		DirectHitWeaponParameters.OwningPlayer,
		WeaponIndex,
		DirectHitWeaponParameters.WeaponName,
		DirectHitWeaponParameters.WeaponBurstMode,
		DirectHitWeaponParameters.WeaponOwner,
		DirectHitWeaponParameters.MeshComponent,
		DirectHitWeaponParameters.FireSocketName,
		GetWorld(),
		DirectHitWeaponParameters.WeaponVFX,
		DirectHitWeaponParameters.WeaponShellCase,
		DirectHitWeaponParameters.BurstCooldown,
		DirectHitWeaponParameters.SingleBurstAmountMaxBurstAmount,
		DirectHitWeaponParameters.MinBurstAmount,
		DirectHitWeaponParameters.CreateShellCasingOnEveryRandomBurst);

	M_AttachedWeapons.Add(DirectHitWeapon);
	UpdateAbilityRangeFromWeapons();
}

void UAttachedWeaponAbilityComponent::SetupTraceWeaponInternal(const FInitWeaponStatTrace& TraceWeaponParameters)
{
	const int32 WeaponIndex = M_AttachedWeapons.Num();
	M_OwningPlayer = TraceWeaponParameters.OwningPlayer;
	M_TargetingData.InitTargetStruct(M_OwningPlayer);

	UWeaponStateTrace* TraceWeapon = NewObject<UWeaponStateTrace>(this);
	TraceWeapon->InitTraceWeapon(
		TraceWeaponParameters.OwningPlayer,
		WeaponIndex,
		TraceWeaponParameters.WeaponName,
		TraceWeaponParameters.WeaponBurstMode,
		TraceWeaponParameters.WeaponOwner,
		TraceWeaponParameters.MeshComponent,
		TraceWeaponParameters.FireSocketName,
		GetWorld(),
		TraceWeaponParameters.WeaponVFX,
		TraceWeaponParameters.WeaponShellCase,
		TraceWeaponParameters.BurstCooldown,
		TraceWeaponParameters.SingleBurstAmountMaxBurstAmount,
		TraceWeaponParameters.MinBurstAmount,
		TraceWeaponParameters.CreateShellCasingOnEveryRandomBurst,
		TraceWeaponParameters.TraceProjectileType);

	M_AttachedWeapons.Add(TraceWeapon);
	UpdateAbilityRangeFromWeapons();
}

void UAttachedWeaponAbilityComponent::SetupMultiTraceWeaponInternal(
	const FInitWeaponStateMultiTrace& MultiTraceWeaponParameters)
{
	const int32 WeaponIndex = M_AttachedWeapons.Num();
	M_OwningPlayer = MultiTraceWeaponParameters.OwningPlayer;
	M_TargetingData.InitTargetStruct(M_OwningPlayer);

	UWeaponStateMultiTrace* MultiTraceWeapon = NewObject<UWeaponStateMultiTrace>(this);
	MultiTraceWeapon->InitMultiTraceWeapon(
		MultiTraceWeaponParameters.OwningPlayer,
		WeaponIndex,
		MultiTraceWeaponParameters.WeaponName,
		MultiTraceWeaponParameters.WeaponBurstMode,
		MultiTraceWeaponParameters.WeaponOwner,
		MultiTraceWeaponParameters.MeshComponent,
		GetWorld(),
		MultiTraceWeaponParameters.FireSocketNames,
		MultiTraceWeaponParameters.bUseOwnerFireDirection,
		MultiTraceWeaponParameters.WeaponVFX,
		MultiTraceWeaponParameters.WeaponShellCase,
		MultiTraceWeaponParameters.BurstCooldown,
		MultiTraceWeaponParameters.SingleBurstAmountMaxBurstAmount,
		MultiTraceWeaponParameters.MinBurstAmount,
		MultiTraceWeaponParameters.CreateShellCasingOnEveryRandomBurst,
		MultiTraceWeaponParameters.TraceProjectileType);

	M_AttachedWeapons.Add(MultiTraceWeapon);
	UpdateAbilityRangeFromWeapons();
}

void UAttachedWeaponAbilityComponent::SetupLaserWeaponInternal(const FInitWeaponStateLaser& LaserWeaponParameters)
{
	const int32 WeaponIndex = M_AttachedWeapons.Num();
	M_OwningPlayer = LaserWeaponParameters.OwningPlayer;
	M_TargetingData.InitTargetStruct(M_OwningPlayer);

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

	M_AttachedWeapons.Add(LaserWeapon);
	UpdateAbilityRangeFromWeapons();
}

void UAttachedWeaponAbilityComponent::SetupMultiHitLaserWeaponInternal(
	const FInitWeaponStateMultiHitLaser& LaserWeaponParameters)
{
	const int32 WeaponIndex = M_AttachedWeapons.Num();
	M_OwningPlayer = LaserWeaponParameters.OwningPlayer;
	M_TargetingData.InitTargetStruct(M_OwningPlayer);

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

	M_AttachedWeapons.Add(LaserWeapon);
	UpdateAbilityRangeFromWeapons();
}

void UAttachedWeaponAbilityComponent::SetupFlameThrowerWeaponInternal(
	const FInitWeaponStateFlameThrower& FlameWeaponParameters)
{
	const int32 WeaponIndex = M_AttachedWeapons.Num();
	M_OwningPlayer = FlameWeaponParameters.OwningPlayer;
	M_TargetingData.InitTargetStruct(M_OwningPlayer);

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

	M_AttachedWeapons.Add(FlameWeapon);
	UpdateAbilityRangeFromWeapons();
}

void UAttachedWeaponAbilityComponent::SetupProjectileWeaponInternal(
	const FInitWeaponStateProjectile& ProjectileWeaponParameters)
{
	const int32 WeaponIndex = M_AttachedWeapons.Num();
	M_OwningPlayer = ProjectileWeaponParameters.OwningPlayer;
	M_TargetingData.InitTargetStruct(M_OwningPlayer);

	UWeaponStateProjectile* ProjectileWeapon = NewObject<UWeaponStateProjectile>(this);
	ProjectileWeapon->InitProjectileWeapon(
		ProjectileWeaponParameters.OwningPlayer,
		WeaponIndex,
		ProjectileWeaponParameters.WeaponName,
		ProjectileWeaponParameters.WeaponBurstMode,
		ProjectileWeaponParameters.WeaponOwner,
		ProjectileWeaponParameters.MeshComponent,
		ProjectileWeaponParameters.FireSocketName,
		GetWorld(),
		ProjectileWeaponParameters.ProjectileSystem,
		ProjectileWeaponParameters.WeaponVFX,
		ProjectileWeaponParameters.WeaponShellCase,
		ProjectileWeaponParameters.BurstCooldown,
		ProjectileWeaponParameters.SingleBurstAmountMaxBurstAmount,
		ProjectileWeaponParameters.MinBurstAmount,
		ProjectileWeaponParameters.CreateShellCasingOnEveryRandomBurst);

	M_AttachedWeapons.Add(ProjectileWeapon);
	UpdateAbilityRangeFromWeapons();
}

void UAttachedWeaponAbilityComponent::SetupRocketProjectileWeaponInternal(
	const FInitWeaponStateRocketProjectile& RocketProjectileParameters)
{
	const int32 WeaponIndex = M_AttachedWeapons.Num();
	M_OwningPlayer = RocketProjectileParameters.OwningPlayer;
	M_TargetingData.InitTargetStruct(M_OwningPlayer);

	UWeaponStateRocketProjectile* RocketWeapon = NewObject<UWeaponStateRocketProjectile>(this);
	RocketWeapon->InitRocketProjectileWeapon(
		RocketProjectileParameters.OwningPlayer,
		WeaponIndex,
		RocketProjectileParameters.WeaponName,
		RocketProjectileParameters.WeaponBurstMode,
		RocketProjectileParameters.WeaponOwner,
		RocketProjectileParameters.MeshComponent,
		RocketProjectileParameters.FireSocketName,
		GetWorld(),
		RocketProjectileParameters.ProjectileSystem,
		RocketProjectileParameters.WeaponVFX,
		RocketProjectileParameters.WeaponShellCase,
		RocketProjectileParameters.RocketSettings,
		RocketProjectileParameters.BurstCooldown,
		RocketProjectileParameters.SingleBurstAmountMaxBurstAmount,
		RocketProjectileParameters.MinBurstAmount,
		RocketProjectileParameters.CreateShellCasingOnEveryRandomBurst);

	M_AttachedWeapons.Add(RocketWeapon);
	UpdateAbilityRangeFromWeapons();
}

void UAttachedWeaponAbilityComponent::SetupMultiProjectileWeaponInternal(
	const FInitWeaponStateMultiProjectile& MultiProjectileState)
{
	const int32 WeaponIndex = M_AttachedWeapons.Num();
	M_OwningPlayer = MultiProjectileState.OwningPlayer;
	M_TargetingData.InitTargetStruct(M_OwningPlayer);

	UWeaponStateMultiProjectile* MultiProjectileWeapon = NewObject<UWeaponStateMultiProjectile>(this);
	MultiProjectileWeapon->InitMultiProjectileWeapon(
		MultiProjectileState.OwningPlayer,
		WeaponIndex,
		MultiProjectileState.WeaponName,
		MultiProjectileState.WeaponBurstMode,
		MultiProjectileState.WeaponOwner,
		MultiProjectileState.MeshComponent,
		GetWorld(),
		MultiProjectileState.FireSocketNames,
		MultiProjectileState.ProjectileSystem,
		MultiProjectileState.WeaponVFX,
		MultiProjectileState.WeaponShellCase,
		MultiProjectileState.BurstCooldown,
		MultiProjectileState.SingleBurstAmountMaxBurstAmount,
		MultiProjectileState.MinBurstAmount,
		MultiProjectileState.CreateShellCasingOnEveryRandomBurst);

	M_AttachedWeapons.Add(MultiProjectileWeapon);
	UpdateAbilityRangeFromWeapons();
}

void UAttachedWeaponAbilityComponent::SetupArchProjectileWeaponInternal(
	const FInitWeaponStateArchProjectile& ArchProjParameters)
{
	const int32 WeaponIndex = M_AttachedWeapons.Num();
	M_OwningPlayer = ArchProjParameters.OwningPlayer;
	M_TargetingData.InitTargetStruct(M_OwningPlayer);

	UWeaponStateArchProjectile* ArchWeapon = NewObject<UWeaponStateArchProjectile>(this);
	ArchWeapon->InitArchProjectileWeapon(
		ArchProjParameters.OwningPlayer,
		WeaponIndex,
		ArchProjParameters.WeaponName,
		ArchProjParameters.WeaponBurstMode,
		ArchProjParameters.WeaponOwner,
		ArchProjParameters.MeshComponent,
		ArchProjParameters.FireSocketName,
		GetWorld(),
		ArchProjParameters.ProjectileSystem,
		ArchProjParameters.WeaponVFX,
		ArchProjParameters.WeaponShellCase,
		ArchProjParameters.BurstCooldown,
		ArchProjParameters.SingleBurstAmountMaxBurstAmount,
		ArchProjParameters.MinBurstAmount,
		ArchProjParameters.CreateShellCasingOnEveryRandomBurst,
		ArchProjParameters.MortarSettings);

	M_AttachedWeapons.Add(ArchWeapon);
	UpdateAbilityRangeFromWeapons();
}

void UAttachedWeaponAbilityComponent::CacheWeaponOwnerInParameters(FInitWeaponStateDirectHit& WeaponParameters) const
{
	WeaponParameters.WeaponOwner = this;
}

void UAttachedWeaponAbilityComponent::CacheWeaponOwnerInParameters(FInitWeaponStatTrace& WeaponParameters) const
{
	WeaponParameters.WeaponOwner = this;
}

void UAttachedWeaponAbilityComponent::CacheWeaponOwnerInParameters(FInitWeaponStateMultiTrace& WeaponParameters) const
{
	WeaponParameters.WeaponOwner = this;
}

void UAttachedWeaponAbilityComponent::CacheWeaponOwnerInParameters(FInitWeaponStateProjectile& WeaponParameters) const
{
	WeaponParameters.WeaponOwner = this;
}

void UAttachedWeaponAbilityComponent::CacheWeaponOwnerInParameters(FInitWeaponStateRocketProjectile& WeaponParameters) const
{
	WeaponParameters.WeaponOwner = this;
}

void UAttachedWeaponAbilityComponent::CacheWeaponOwnerInParameters(FInitWeaponStateMultiProjectile& WeaponParameters) const
{
	WeaponParameters.WeaponOwner = this;
}

void UAttachedWeaponAbilityComponent::CacheWeaponOwnerInParameters(FInitWeaponStateArchProjectile& WeaponParameters) const
{
	WeaponParameters.WeaponOwner = this;
}

void UAttachedWeaponAbilityComponent::CacheWeaponOwnerInParameters(FInitWeaponStateLaser& WeaponParameters) const
{
	WeaponParameters.WeaponOwner = this;
}

void UAttachedWeaponAbilityComponent::CacheWeaponOwnerInParameters(FInitWeaponStateMultiHitLaser& WeaponParameters) const
{
	WeaponParameters.WeaponOwner = this;
}

void UAttachedWeaponAbilityComponent::CacheWeaponOwnerInParameters(FInitWeaponStateFlameThrower& WeaponParameters) const
{
	WeaponParameters.WeaponOwner = this;
}
