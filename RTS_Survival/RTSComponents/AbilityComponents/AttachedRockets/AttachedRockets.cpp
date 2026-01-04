// AttachedRockets.cpp
// Copyright (C) Bas Blokzijl - All rights reserved.

#include "AttachedRockets.h"

#include "NiagaraFunctionLibrary.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/Projectile/Projectile.h"

UAttachedRockets::UAttachedRockets()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UAttachedRockets::ExecuteFireAllRockets()
{
	if (not ExecuteFire_EnsureFireReady())
	{
		return;
	}
	M_RocketsRemainingToFire = M_RocketsInstances->GetNumVisibleInstances();
	// Needs to be done first as if the salvo has only one rocket the first call will instantly start reloading where
	// We need to be able to swap the cancel for the reload variant of this ability.
	if (not M_Owner->SwapAbility(EAbilityID::IdFireRockets, EAbilityID::IdCancelRocketFire))
	{
		ReportError("Failed to swap ability from IdFireRockets to IdCancelRocketFire" +

			UEnum::GetValueAsString(EAbilityID::IdCancelRocketFire) + " in ExecuteFireAllRockets.");
	}
	// Fire one rocket immediately.
	FireRocket();
	// Decrement as we did not fire this with the regular Salvo Function and need to keep track.
	if (--M_RocketsRemainingToFire <= 0)
	{
		OnSalvoFinished();
	}
	// If there are more rockets to fire, we set up a timer to fire them in intervals.
	else if (UWorld* World = GetWorld())
	{
		const float CoolDown = GetCoolDownFromFlux();
		World->GetTimerManager().ClearTimer(M_CooldownTimerHandle);
		World->GetTimerManager().SetTimer(
			M_CooldownTimerHandle,
			this,
			&UAttachedRockets::OnFireRocketInSalvo,
			CoolDown,
			true);
	}
}

void UAttachedRockets::TerminateFireAllRockets()
{
	if (not EnsureOwnerIsValid() || M_RocketsRemainingToFire <= 0)
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_CooldownTimerHandle);
	}
	// If any rockets remain you first have to fire them before reloading.
	constexpr EAbilityID AbilityAfterTermination = EAbilityID::IdFireRockets;
	if (not M_Owner->SwapAbility(EAbilityID::IdCancelRocketFire, AbilityAfterTermination))
	{
		ReportError("Could not change cancel rocket fire ability to " +
			UEnum::GetValueAsString(AbilityAfterTermination));
	}
}

void UAttachedRockets::CancelFireRockets()
{
	if (not EnsureOwnerIsValid())
	{
		return;
	}
	if (M_RocketsRemainingToFire <= 0)
	{
		// Final rocket already fired.
		M_Owner->DoneExecutingCommand(EAbilityID::IdCancelRocketFire);
		return;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_CooldownTimerHandle);
	}
	// Ability swap already happened on Terminate of ExecuteFireAllRockets.
	M_Owner->DoneExecutingCommand(EAbilityID::IdCancelRocketFire);
}

void UAttachedRockets::SetupMeshManually(UStaticMeshComponent* MeshComp)
{
	M_ManuallySetupMesh = MeshComp;
}

void UAttachedRockets::BeginPlay()
{
	Super::BeginPlay();
	if (not EnsureRocketEnumIsValid() || not SetupOwningPlayer())
	{
		return;
	}
	BeginPlay_SetupCallbackToProjectileManager();
	BeginPlay_LoadDataFromGameState(M_OwningPlayer);
	BeginPlay_SetupSystemOnOwner();
}

void UAttachedRockets::BeginDestroy()
{
	Super::BeginDestroy();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_CooldownTimerHandle);
	}
}

bool UAttachedRockets::EnsureFrameMeshCompIsValid() const
{
	if (not IsValid(M_FrameMeshComponent))
	{
		ReportError(TEXT("Frame mesh component is not valid!"));
		return false;
	}
	return true;
}

bool UAttachedRockets::EnsureRocketInstancerIsValid() const
{
	if (not IsValid(M_RocketsInstances))
	{
		ReportError(TEXT("Rocket instancer is not valid!"));
		return false;
	}
	return true;
}

bool UAttachedRockets::EnsureOwnerIsValid() const
{
	if (M_Owner.IsValid())
	{
		return true;
	}
	ReportError(TEXT("No valid command interface!"));
	return false;
}

bool UAttachedRockets::EnsureRocketEnumIsValid() const
{
	if (RocketAbilityType != ERocketAbility::None)
	{
		return true;
	}
	ReportError(TEXT("RocketAbilityType is not set!"));
	return false;
}

bool UAttachedRockets::SetupOwningPlayer()
{
	AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return false;
	}
	const URTSComponent* RTSComponent = Cast<URTSComponent>(Owner->GetComponentByClass(URTSComponent::StaticClass()));
	if (RTSComponent == nullptr)
	{
		ReportError(TEXT("Owner does not have a URTSComponent!"));
		return false;
	}
	M_OwningPlayer = RTSComponent->GetOwningPlayer();
	if (M_OwningPlayer < 0)
	{
		ReportError(TEXT("Owning player is loaded from RTS but invalid!"));
		return false;
	}
	return true;
}

void UAttachedRockets::BeginPlay_LoadDataFromGameState(const int32 PlayerOwningRockets)
{
	const ACPPGameState* GameState = FRTS_Statics::GetGameState(this);
	if (not IsValid(GameState))
	{
		ReportError("Failed to load GameState in LoadDataFromGameState.");
		return;
	}
	M_RocketData = GameState->GetAttachedRocketDataOfPlayer(PlayerOwningRockets, RocketAbilityType);
	M_ProjectileVfxSettings.WeaponCaliber = 1;
	// The correct rocket mesh will be set on the niagara system after spawning the rocket.
	M_ProjectileVfxSettings.ProjectileNiagaraSystem = EProjectileNiagaraSystem::AttachedRocket;
}

bool UAttachedRockets::SetupOnOwner_InitInterface()
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return false;
	}
	M_Owner = Cast<ICommands>(OwnerActor);
	return M_Owner.IsValid();
}

void UAttachedRockets::BeginPlay_SetupSystemOnOwner()
{
	if (!SetupOnOwner_InitInterface())
	{
		ReportError(TEXT("Failed to set up owner interface!"));
		return;
	}
	if (not IsValid(M_ManuallySetupMesh))
	{
		// We start loading the frame mesh once we found the correct socket for it.
		FindMeshComponentWithSocket();
	}
	else
	{
		// If the mesh was set manually, we use that instead of searching for it.
		OnFrameSetupManually();
	}
}

void UAttachedRockets::FindMeshComponentWithSocket()
{
	AActor* OwnerActor = GetOwner();
	if (OwnerActor == nullptr)
	{
		ReportError(TEXT("Owner is null in FindMeshComponentWithSocket"));
		return;
	}

	TArray<UMeshComponent*> MeshComponents;
	OwnerActor->GetComponents<UMeshComponent>(MeshComponents);

	for (UMeshComponent* MeshComp : MeshComponents)
	{
		if (MeshComp && MeshComp->DoesSocketExist(FrameSocketName))
		{
			OnFoundMeshComponentForFrame(MeshComp);
			return;
		}
	}
	// check for child actor component and see if that actor has mesh components that have the socket.
	if (UChildActorComponent* ChildActorComp = OwnerActor->FindComponentByClass<UChildActorComponent>())
	{
		AActor* ChildActor = ChildActorComp->GetChildActor();
		if (ChildActor)
		{
			TArray<UMeshComponent*> ChildMeshComponents;
			ChildActor->GetComponents<UMeshComponent>(ChildMeshComponents);
			for (UMeshComponent* MeshComp : ChildMeshComponents)
			{
				if (MeshComp && MeshComp->DoesSocketExist(FrameSocketName))
				{
					OnFoundMeshComponentForFrame(MeshComp);
					return;
				}
			}
		}
	}

	ReportError(FString::Printf(
		TEXT("No mesh component with socket '%s' found on owner cannot attach rocket system!"),
		*FrameSocketName.ToString()
	));
}

void UAttachedRockets::OnFoundMeshComponentForFrame(UMeshComponent* MeshComponent)
{
	if (!EnsureOwnerIsValid() || MeshComponent == nullptr)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		ReportError(TEXT("Owner is invalid in OnFoundMeshComponentForFrame"));
		return;
	}

	M_FrameMeshComponent = NewObject<UStaticMeshComponent>(OwnerActor);
	if (M_FrameMeshComponent == nullptr)
	{
		ReportError(TEXT("Failed to allocate frame mesh component."));
		return;
	}

	M_FrameMeshComponent->AttachToComponent(
		MeshComponent,
		FAttachmentTransformRules::KeepRelativeTransform,
		FrameSocketName
	);
	M_FrameMeshComponent->SetRelativeLocation(LocalFrameOffset);
	// no collision and disable nav mesh affect.
	SetFrameMeshCollision();
	M_FrameMeshComponent->RegisterComponent();

	StartAsyncLoadFrameMesh();
}

void UAttachedRockets::StartAsyncLoadFrameMesh()
{
	if (FrameMesh.IsNull())
	{
		ReportError(TEXT("FrameMesh is null; cannot load frame."));
		return;
	}

	bIsLoadingFrame = true;
	FSoftObjectPath FrameMeshPath = FrameMesh.ToSoftObjectPath();
	FStreamableManager& Streamable = UAssetManager::GetStreamableManager();

	FrameMeshHandle = Streamable.RequestAsyncLoad(
		FrameMeshPath,
		FStreamableDelegate::CreateUObject(this, &UAttachedRockets::OnFrameLoadedAsync)
	);
}

void UAttachedRockets::OnFrameLoadedAsync()
{
	bIsLoadingFrame = false;
	if (FrameMesh.Get() == nullptr)
	{
		ReportError(TEXT("Failed to load frame mesh asynchronously."));
		return;
	}

	if (M_FrameMeshComponent == nullptr)
	{
		ReportError(TEXT("FrameMeshComponent is null in OnFrameLoadedAsync."));
		return;
	}

	M_FrameMeshComponent->SetStaticMesh(FrameMesh.Get());
	M_FrameSocketNames = M_FrameMeshComponent->GetAllSocketNames();
	M_NumSockets = M_FrameSocketNames.Num();

	if (M_NumSockets == 0)
	{
		ReportError(TEXT("Frame mesh has zero sockets; cannot spawn rockets."));
		return;
	}

	StartAsyncLoadRocketMesh();
}

void UAttachedRockets::OnFrameSetupManually()
{
	if (!IsValid(M_ManuallySetupMesh))
	{
		ReportError(TEXT("Invalid manually setup mesh"));
		return;
	}

	M_FrameMeshComponent = M_ManuallySetupMesh.Get();
	M_ManuallySetupMesh = nullptr;

	if (!M_FrameMeshComponent)
	{
		ReportError(TEXT("FrameMeshComponent is null in OnFrameSetupManually."));
		return;
	}

	// Ensure it's registered & visible
	if (!M_FrameMeshComponent->IsRegistered())
	{
		M_FrameMeshComponent->RegisterComponent();
	}
	M_FrameMeshComponent->SetVisibility(true, true);
	M_FrameMeshComponent->SetHiddenInGame(false, true);

	// If FrameMesh asset is set, enforce it (in case user forgot in editor)
	if (FrameMesh.IsValid())
	{
		M_FrameMeshComponent->SetStaticMesh(FrameMesh.Get());
	}

	M_FrameSocketNames = M_FrameMeshComponent->GetAllSocketNames();
	M_NumSockets = M_FrameSocketNames.Num();
	bM_WasSetupManually = true;

	if (M_NumSockets == 0)
	{
		ReportError(TEXT("Frame mesh has zero sockets; cannot spawn rockets."));
		return;
	}

	StartAsyncLoadRocketMesh();
}

void UAttachedRockets::StartAsyncLoadRocketMesh()
{
	if (RocketMesh.IsNull())
	{
		ReportError(TEXT("RocketMesh is null; cannot load rocket mesh."));
		return;
	}

	bIsLoadingRocket = true;
	FSoftObjectPath RocketMeshPath = RocketMesh.ToSoftObjectPath();
	FStreamableManager& Streamable = UAssetManager::GetStreamableManager();

	RocketMeshHandle = Streamable.RequestAsyncLoad(
		RocketMeshPath,
		FStreamableDelegate::CreateUObject(this, &UAttachedRockets::OnRocketLoadedAsync)
	);
}

void UAttachedRockets::OnRocketLoadedAsync()
{
	bIsLoadingRocket = false;
	if (RocketMesh.Get() == nullptr)
	{
		ReportError(TEXT("Failed to load rocket mesh asynchronously."));
		return;
	}

	CreateRocketInstances();
}

void UAttachedRockets::CreateRocketInstances()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		ReportError(TEXT("Owner is invalid in CreateRocketInstances."));
		return;
	}

	// A manual setup may have occured on a child actor comp; so we ensure to attach to the frame mesh as root.
	USceneComponent* RootComp = bM_WasSetupManually ? M_FrameMeshComponent : OwnerActor->GetRootComponent();
	if (!RootComp)
	{
		ReportError(TEXT("Owner has no RootComponent to attach rocket instancer to."));
		return;
	}

	M_RocketsInstances = NewObject<URTSHidableInstancedStaticMeshComponent>(OwnerActor);
	if (M_RocketsInstances == nullptr)
	{
		ReportError(TEXT("Failed to allocate UInstancedStaticMeshComponent for rockets."));
		return;
	}
	if (IsValid(Optional_RocketMaterial) && RocketMesh.IsValid())
	{
		RocketMesh.Get()->SetMaterial(0, Optional_RocketMaterial);
	}
	SetRocketInstancesCollision();

	M_RocketsInstances->AttachToComponent(RootComp, FAttachmentTransformRules::KeepRelativeTransform);
	M_RocketsInstances->SetStaticMesh(RocketMesh.Get());
	M_RocketsInstances->RegisterComponent();

	const int32 ToSpawn = FMath::Min(M_NumSockets, MaxRockets);
	for (int32 Index = 0; Index < ToSpawn; ++Index)
	{
		const FName& SockName = M_FrameSocketNames[Index];

		FTransform SocketCompXform = M_FrameMeshComponent->GetSocketTransform(SockName, RTS_Component);
		FVector LocCompSpace = SocketCompXform.GetLocation() + LocalRocketOffsetPerSocket;
		FQuat RotCompSpace = SocketCompXform.GetRotation();

		FTransform FrameWorldXform = M_FrameMeshComponent->GetComponentTransform();
		FVector WorldLoc = FrameWorldXform.TransformPosition(LocCompSpace);
		FQuat WorldRot = FrameWorldXform.TransformRotation(RotCompSpace);

		FTransform InstWorldXform(WorldRot, WorldLoc);
		const int32 InstanceRocketIndex = M_RocketsInstances->AddInstance(InstWorldXform, true);
		M_RocketIndexToSocketName.Add(InstanceRocketIndex, SockName);
	}
}


void UAttachedRockets::ReportError(const FString& Message) const
{
	const FString OwnerName = IsValid(GetOwner()) ? GetOwner()->GetName() : TEXT("Unknown Owner");
	const FString InterfaceStatus = M_Owner.IsValid()
		                                ? TEXT("Has valid command interface.")
		                                : TEXT("No valid command interface.");
	RTSFunctionLibrary::ReportError(
		Message +
		TEXT("\nFrom Component: ") + GetName() +
		TEXT("\nOwner: ") + OwnerName +
		TEXT("\nInterface status: ") + InterfaceStatus
	);
}

FRotator UAttachedRockets::ApplyAccuracyDeviation(const FRotator& BaseRotation, const int32 Accuracy) const
{
	if (Accuracy == 100)
	{
		// No deviation if accuracy is perfect.
		return BaseRotation;
	}

	const float DeviationAngle = ((100.0f - Accuracy)) / 10.f *
		DeveloperSettings::GameBalance::Weapons::RocketProjectileAccuracyMlt;

	// Random deviation within the possible range
	const float RandomPitch = FMath::RandRange(-DeviationAngle, DeviationAngle);
	const float RandomYaw = FMath::RandRange(-DeviationAngle, DeviationAngle);

	return BaseRotation + FRotator(RandomPitch, RandomYaw, 0.0f);
}

bool UAttachedRockets::ExecuteFire_EnsureFireReady() const
{
	if (not IsValid(GetOwner()) || not EnsureOwnerIsValid() || not EnsureFrameMeshCompIsValid() ||
		not EnsureRocketInstancerIsValid() || not EnsureIsValidProjectileManager())
	{
		RTSFunctionLibrary::ReportError("Failed to Execute Fire All Rockets!");
		return false;
	}
	if (bIsLoadingFrame || bIsLoadingRocket)
	{
		RTSFunctionLibrary::ReportError("Rockets are still loading; cannot fire yet.");
		return false;
	}
	if (M_RocketsInstances->GetInstanceCount() == 0)
	{
		RTSFunctionLibrary::ReportError("No rockets to fire; instance count is zero.");
		return false;
	}
	if (M_RocketsInstances->GetNumVisibleInstances() <= 0)
	{
		RTSFunctionLibrary::ReportError("No visible rockets to fire; all instances are hidden.");
		return false;
	}
	return true;
}

FRotator UAttachedRockets::FireRocket_GetLaunchRotation(const FRotator& SocketRotationInWorld) const
{
	return ApplyAccuracyDeviation(SocketRotationInWorld, M_RocketData.Accuracy);
}

void UAttachedRockets::OnFireRocketInSalvo()
{
	FireRocket();
	M_RocketsRemainingToFire--;
	UWorld* World = GetWorld();
	if (M_RocketsRemainingToFire <= 0 && World)
	{
		OnSalvoFinished();
	}
}

void UAttachedRockets::FireRocket() const
{
	FTransform LocalInstTransform;
	int32 RocketIndex = 0;
	M_RocketsInstances->GetFirstNonHiddenInstance(LocalInstTransform, RocketIndex);

	const FTransform& InstancerToWorld = M_RocketsInstances->GetComponentTransform();
	const FVector LaunchLocation = InstancerToWorld.TransformPosition(LocalInstTransform.GetLocation());

	const FQuat WorldQuat = InstancerToWorld.TransformRotation(LocalInstTransform.GetRotation());
	const FRotator LaunchRotation = FireRocket_GetLaunchRotation(FRotator(WorldQuat));

	// DrawDebugSphere(GetWorld(), LaunchLocation, 10.0f, 12, FColor::Red, false, 5.0f);

	if (not FireRocket_LaunchPoolProjectile(LaunchLocation, LaunchRotation))
	{
		ReportError(TEXT("Failed to launch rocket projectile!"));
		return;
	}

	CreateLaunchFX(LaunchLocation, LaunchRotation, RocketIndex);
	M_RocketsInstances->SetInstanceHidden(RocketIndex, true);
}


void UAttachedRockets::CreateLaunchFX(const FVector& LaunchLocation, const FRotator& LaunchRotation,
                                      const int32 RocketInstanceIndex) const
{
	const UWorld* World = GetWorld();
	if (not World)
	{
		return;
	}
	const FName RocketSocketName = M_RocketIndexToSocketName.Contains(RocketInstanceIndex)
		                               ? M_RocketIndexToSocketName[RocketInstanceIndex]
		                               : NAME_None;
	UGameplayStatics::PlaySoundAtLocation(World, RocketLaunchSound, LaunchLocation, FRotator::ZeroRotator, 1,
	                                      1, 0, RocketLaunchSoundAttenuation, RocketLaunchSoundConcurrency);

	// Attach the burst launch effect particle system to the mesh component, preserving scale.
	UNiagaraComponent* SpawnedSystem = UNiagaraFunctionLibrary::SpawnSystemAttached(
		RocketLaunchEffect, M_FrameMeshComponent, RocketSocketName, FVector(0), FRotator(LaunchRotation),
		LaunchScale, EAttachLocation::SnapToTarget, true, ENCPoolMethod::None, true, true);
}

bool UAttachedRockets::FireRocket_LaunchPoolProjectile(const FVector& LaunchLocation,
                                                       const FRotator& LaunchRotation) const
{
	AProjectile* Projectile = M_ProjectileManager->GetDormantTankProjectile();
	if (not IsValid(Projectile))
	{
		ReportError("Failed to get a dormant projectile from the pool.");
		return false;
	}
	const float Damage = GetDamageFromFlux();
	const float ArmorPen = GetArmorPenFromFlux();
	Projectile->SetupProjectileForNewLaunch(
		nullptr,
		M_RocketData.DamageType,
		M_RocketData.Range,
		Damage,
		ArmorPen,
		ArmorPen, 0, 0, 0,
		0,
		M_OwningPlayer,
		ImpactSurfaceData,
		BounceEffect,
		BounceSound,
		ImpactScale,
		BounceScale,
		M_RocketData.ProjectileSpeed,
		LaunchLocation,
		LaunchRotation,
		ImpactAttenuation,
		ImpactConcurrency, M_ProjectileVfxSettings, EWeaponShellType::Shell_HEAT, {},
		M_RocketData.WeaponCalibre);
	Projectile->OverwriteGravityScale(DeveloperSettings::GameBalance::Weapons::RocketProjectileGravityScale * FMath::RandRange(0.2, 1.5));
	if (RocketMesh.IsValid())
	{
		Projectile->SetupAttachedRocketMesh(RocketMesh.Get());
	}
	return true;
}

void UAttachedRockets::OnSalvoFinished()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_CooldownTimerHandle);
	}
	if (not EnsureOwnerIsValid())
	{
		return;
	}
	if (not M_Owner->SwapAbility(EAbilityID::IdCancelRocketFire, EAbilityID::IdRocketsReloading))
	{
		ReportError("Failed to change CancelRocketFire ability to IdRocketsReloading On Salvo finished!");
	}
	StartReload();
	M_Owner->DoneExecutingCommand(EAbilityID::IdFireRockets);
}

void UAttachedRockets::BeginPlay_SetupCallbackToProjectileManager()
{
	ACPPGameState* GameState = FRTS_Statics::GetGameState(this);
	if (not GameState)
	{
		return;
	}
	TWeakObjectPtr<UAttachedRockets> WeakThis(this);
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

bool UAttachedRockets::EnsureIsValidProjectileManager() const
{
	if (M_ProjectileManager.IsValid())
	{
		return true;
	}
	ReportError("Invalid projectile manager!");
	return false;
}

void UAttachedRockets::OnProjectileManagerLoaded(const TObjectPtr<ASmallArmsProjectileManager>& ProjectileManager)
{
	if (not IsValid(ProjectileManager))
	{
		ReportError("OnProjectileManagerLoaded: ProjectileManager is not valid");
	}
	M_ProjectileManager = ProjectileManager;
}

float UAttachedRockets::GetDamageFromFlux() const
{
	const float Min = M_RocketData.Damage * (1.0f - M_RocketData.DamageFluxMlt);
	const float Max = M_RocketData.Damage * (1.0f + M_RocketData.DamageFluxMlt);
	return FMath::RandRange(Min, Max);
}

float UAttachedRockets::GetArmorPenFromFlux() const
{
	const float Min = M_RocketData.ArmorPen * (1.0f - M_RocketData.ArmorPenFluxMlt);
	const float Max = M_RocketData.ArmorPen * (1.0f + M_RocketData.ArmorPenFluxMlt);
	return FMath::RandRange(Min, Max);
}

float UAttachedRockets::GetCoolDownFromFlux() const
{
	const float Min = M_RocketData.Cooldown * (1.0f - M_RocketData.CoolDownFluxMlt);
	const float Max = M_RocketData.Cooldown * (1.0f + M_RocketData.CoolDownFluxMlt);
	return FMath::RandRange(Min, Max);
}

float UAttachedRockets::GetReloadFromFlux() const
{
	const float Min = M_RocketData.ReloadSpeed * (1.0f - M_RocketData.CoolDownFluxMlt);
	const float Max = M_RocketData.ReloadSpeed * (1.0f + M_RocketData.CoolDownFluxMlt);
	return FMath::RandRange(Min, Max);
}

void UAttachedRockets::StartReload()
{
	if (UWorld* World = GetWorld())
	{
		const float ReloadTime = GetReloadFromFlux();
		FTimerDelegate ReloadDelegate;
		ReloadDelegate.BindWeakLambda(this, [this]()-> void { OnReloadFinished(); });
		World->GetTimerManager().SetTimer(
			M_ReloadTimerHandle,
			ReloadDelegate,
			ReloadTime,
			false
		);
	}
}

void UAttachedRockets::OnReloadFinished() const
{
	if (not EnsureOwnerIsValid())
	{
		return;
	}
	SetAllRocketsVisible();
	const FUnitAbilityEntry RocketAbilityEntry = FAbilityHelpers::GetRocketAbilityForRocketType(RocketAbilityType);
	M_Owner->SwapAbility(EAbilityID::IdRocketsReloading, RocketAbilityEntry);
}

void UAttachedRockets::SetAllRocketsVisible() const
{
	if (not EnsureRocketInstancerIsValid())
	{
		return;
	}
	for (int32 Index = 0; Index < M_RocketsInstances->GetInstanceCount(); ++Index)
	{
		M_RocketsInstances->SetInstanceHidden(Index, false);
	}
}

void UAttachedRockets::SetFrameMeshCollision() const
{
	if (not EnsureFrameMeshCompIsValid())
	{
		return;
	}
	M_FrameMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// disable overlap.
	M_FrameMeshComponent->SetGenerateOverlapEvents(false);
	// Disable nav mesh affect.
	M_FrameMeshComponent->SetCanEverAffectNavigation(false);
}

void UAttachedRockets::SetRocketInstancesCollision() const
{
	if (not EnsureRocketInstancerIsValid())
	{
		return;
	}
	M_RocketsInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// disable overlap.
	M_RocketsInstances->SetGenerateOverlapEvents(false);
	// Disable nav mesh affect.
	M_RocketsInstances->SetCanEverAffectNavigation(false);
}
