// Copyright (C) Bas Blokzijl - All rights reserved.

#include "FlareAimAbilityComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/NavCollision/RTSNavCollision.h"
#include "RTS_Survival/Units/Tanks/AITankMaster.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace FlareAimAbilityConstants
{
	// Without a launch socket the flare leaves from this height above the unit's origin.
	constexpr float FallbackLaunchHeight = 250.f;
	constexpr float GroundTraceHalfHeight = 10000.f;
	// The turret (a child actor) carries the launch socket and may spawn a few frames after the vehicle.
	constexpr float AttachLauncherIntervalSeconds = 0.2f;
	constexpr int32 MaxAttachLauncherAttempts = 25;
}

UFlareAimAbilityComponent::UFlareAimAbilityComponent()
	: UAimAbilityComponent()
{
	M_AimAbilitySettings.AimAbilityType = EAimAbilityType::FlareLaunch;
	M_AimAbilitySettings.AimAssistType = EPlayerAimAbilityTypes::LargeCrosshair;
	M_AimAbilitySettings.Cooldown = 60;
}

float UFlareAimAbilityComponent::GetAimAbilityRadius() const
{
	return M_FlareSettings.VisionRadius;
}

void UFlareAimAbilityComponent::PostInitProperties()
{
	Super::PostInitProperties();
	M_TankMaster = Cast<ATankMaster>(GetOwner());
}

void UFlareAimAbilityComponent::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_ValidateFlareSettings();
	BeginPlay_AttachLauncherMesh();
}

void UFlareAimAbilityComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_AttachLauncherTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void UFlareAimAbilityComponent::BeginPlay_AttachLauncherMesh()
{
	UWorld* World = GetWorld();
	if (not IsValid(M_FlareSettings.Visuals.LauncherMesh) || not World)
	{
		return;
	}
	FTimerDelegate AttachDelegate;
	TWeakObjectPtr<UFlareAimAbilityComponent> WeakThis(this);
	AttachDelegate.BindLambda([WeakThis]()
	{
		if (not WeakThis.IsValid())
		{
			return;
		}
		WeakThis->TryAttachLauncherMesh();
	});
	World->GetTimerManager().SetTimer(M_AttachLauncherTimerHandle, AttachDelegate,
	                                  FlareAimAbilityConstants::AttachLauncherIntervalSeconds, true);
}

void UFlareAimAbilityComponent::TryAttachLauncherMesh()
{
	USceneComponent* SocketComponent = FindLaunchSocketComponent();
	++M_AttachLauncherAttempts;
	if (not IsValid(SocketComponent))
	{
		if (M_AttachLauncherAttempts < FlareAimAbilityConstants::MaxAttachLauncherAttempts)
		{
			return;
		}
		RTSFunctionLibrary::ReportError("Flare aim ability on " + GetNameSafe(GetOwner()) + " found no socket " +
			M_FlareSettings.LaunchSocketName.ToString() + " to show its launcher on.");
	}
	else
	{
		M_LauncherMeshComponent = NewObject<UStaticMeshComponent>(GetOwner(), TEXT("FlareLauncherMesh"));
		M_LauncherMeshComponent->SetStaticMesh(M_FlareSettings.Visuals.LauncherMesh);
		M_LauncherMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M_LauncherMeshComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		M_LauncherMeshComponent->SetGenerateOverlapEvents(false);
		M_LauncherMeshComponent->SetCanEverAffectNavigation(false);
		M_LauncherMeshComponent->SetReceivesDecals(false);
		M_LauncherMeshComponent->RegisterComponent();
		// The socket sits on the turret, another actor; its pivot is the tube mouth, so it snaps onto the socket.
		M_LauncherMeshComponent->AttachToComponent(SocketComponent,
		                                           FAttachmentTransformRules::SnapToTargetIncludingScale,
		                                           M_FlareSettings.LaunchSocketName);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_AttachLauncherTimerHandle);
	}
}

bool UFlareAimAbilityComponent::GetUsesWeaponBehaviour() const
{
	return false;
}

bool UFlareAimAbilityComponent::GetAbilityMaxRange(float& OutMaxRange) const
{
	OutMaxRange = M_FlareSettings.FireRange;
	return OutMaxRange > 0.f;
}

void UFlareAimAbilityComponent::ExecuteAbilityInRange(const FVector& TargetLocation)
{
	LaunchFlareAt(TargetLocation);
	// Fire and forget: the flare lives on by itself, the unit is free for its next command.
	HandleBehaviourDurationFinished(true);
}

void UFlareAimAbilityComponent::OnAbilityInRangeFinished()
{
	// No weapons were taken over, so there is nothing to give back.
}

void UFlareAimAbilityComponent::RequestMoveToLocation(const FVector& MoveToLocation)
{
	if (not GetIsValidTankMaster())
	{
		return;
	}
	if (not M_TankMaster->GetIsValidAIController())
	{
		return;
	}
	if (M_TankMaster->GetIsValidRTSNavCollision())
	{
		M_TankMaster->RTSNavCollision->EnableAffectNavmesh(false);
	}
	AAITankMaster* TankController = M_TankMaster->GetAIController();
	if (TankController->MoveToLocationWithGoalAcceptance(MoveToLocation))
	{
		return;
	}
	if (M_TankMaster->GetIsValidRTSNavCollision())
	{
		M_TankMaster->RTSNavCollision->EnableAffectNavmesh(true);
	}
}

void UFlareAimAbilityComponent::StopMovementForAbility()
{
	if (not GetIsValidTankMaster())
	{
		return;
	}
	if (not M_TankMaster->GetIsValidAIController())
	{
		return;
	}
	M_TankMaster->GetAIController()->StopMovement();
	if (M_TankMaster->GetIsValidRTSNavCollision())
	{
		M_TankMaster->RTSNavCollision->EnableAffectNavmesh(true);
	}
}

void UFlareAimAbilityComponent::BeginPlay_ValidateFlareSettings() const
{
	if (M_AimAbilitySettings.AimAbilityType != EAimAbilityType::FlareLaunch)
	{
		RTSFunctionLibrary::ReportError("Flare aim ability on " + GetNameSafe(GetOwner()) +
			" does not use the FlareLaunch aim ability type; its command card button would show another ability.");
	}
	if (M_FlareSettings.LaunchSocketName.IsNone())
	{
		RTSFunctionLibrary::ReportError("Flare aim ability on " + GetNameSafe(GetOwner()) +
			" has no launch socket name; the flare will leave from above the unit.");
	}
	if (not IsValid(M_FlareSettings.Visuals.BurnEffect) || not IsValid(M_FlareSettings.Visuals.FlareMesh))
	{
		RTSFunctionLibrary::ReportError("Flare aim ability on " + GetNameSafe(GetOwner()) +
			" misses its burn effect or flare mesh.");
	}
}

USceneComponent* UFlareAimAbilityComponent::FindLaunchSocketComponent() const
{
	AActor* Owner = GetOwner();
	if (not IsValid(Owner) || M_FlareSettings.LaunchSocketName.IsNone())
	{
		return nullptr;
	}
	TArray<USceneComponent*> SceneComponents;
	// The socket usually sits on the turret, which is a child actor of the vehicle.
	constexpr bool bIncludeFromChildActors = true;
	Owner->GetComponents<USceneComponent>(SceneComponents, bIncludeFromChildActors);
	for (USceneComponent* SceneComponent : SceneComponents)
	{
		if (IsValid(SceneComponent) && SceneComponent->DoesSocketExist(M_FlareSettings.LaunchSocketName))
		{
			return SceneComponent;
		}
	}
	return nullptr;
}

USceneComponent* UFlareAimAbilityComponent::GetLaunchTransform(FVector& OutLocation, FVector& OutDirection) const
{
	USceneComponent* SocketComponent = FindLaunchSocketComponent();
	if (IsValid(SocketComponent))
	{
		const FTransform SocketTransform = SocketComponent->GetSocketTransform(M_FlareSettings.LaunchSocketName);
		OutLocation = SocketTransform.GetLocation();
		OutDirection = SocketTransform.GetUnitAxis(EAxis::X);
		return SocketComponent;
	}

	RTSFunctionLibrary::ReportError("Flare aim ability on " + GetNameSafe(GetOwner()) + " found no socket " +
		M_FlareSettings.LaunchSocketName.ToString() + "; the flare leaves from above the unit.");
	const AActor* Owner = GetOwner();
	OutLocation = IsValid(Owner)
		              ? Owner->GetActorLocation() + FVector(0.f, 0.f, FlareAimAbilityConstants::FallbackLaunchHeight)
		              : FVector::ZeroVector;
	OutDirection = FVector::UpVector;
	return nullptr;
}

FVector UFlareAimAbilityComponent::GetGroundTargetLocation(const FVector& TargetLocation) const
{
	UWorld* World = GetWorld();
	if (not World)
	{
		return TargetLocation;
	}
	const FVector TraceOffset(0.f, 0.f, FlareAimAbilityConstants::GroundTraceHalfHeight);
	FHitResult GroundHit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FlareGroundTrace), false);
	QueryParams.AddIgnoredActor(GetOwner());
	if (World->LineTraceSingleByChannel(GroundHit, TargetLocation + TraceOffset, TargetLocation - TraceOffset,
	                                    COLLISION_TRACE_LANDSCAPE, QueryParams))
	{
		return GroundHit.ImpactPoint;
	}
	return TargetLocation;
}

AFlareProjectile* UFlareAimAbilityComponent::LaunchFlareAt(const FVector& TargetLocation)
{
	UWorld* World = GetWorld();
	if (not World)
	{
		return nullptr;
	}

	FVector LaunchLocation;
	FVector LaunchDirection;
	USceneComponent* SocketComponent = GetLaunchTransform(LaunchLocation, LaunchDirection);

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = GetOwner();
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AFlareProjectile* Flare = World->SpawnActor<AFlareProjectile>(AFlareProjectile::StaticClass(), LaunchLocation,
	                                                              LaunchDirection.Rotation(), SpawnParameters);
	if (not IsValid(Flare))
	{
		RTSFunctionLibrary::ReportError("Flare aim ability could not spawn its flare for " + GetNameSafe(GetOwner()));
		return nullptr;
	}

	FFlareProjectileSettings ProjectileSettings;
	ProjectileSettings.ApexHeight = M_FlareSettings.ApexHeight;
	ProjectileSettings.HangTimeSeconds = M_FlareSettings.HangTimeSeconds;
	ProjectileSettings.VisionRadius = M_FlareSettings.VisionRadius;
	ProjectileSettings.AscentSpeed = M_FlareSettings.AscentSpeed;
	ProjectileSettings.Visuals = M_FlareSettings.Visuals;
	Flare->LaunchFlare(LaunchLocation, LaunchDirection, GetGroundTargetLocation(TargetLocation), ProjectileSettings,
	                   GetOwningPlayer());
	M_LastLaunchedFlare = Flare;

	PlayLaunchEffects(SocketComponent, LaunchLocation, LaunchDirection);
	return Flare;
}

void UFlareAimAbilityComponent::PlayLaunchEffects(USceneComponent* SocketComponent, const FVector& LaunchLocation,
                                                  const FVector& LaunchDirection) const
{
	const FFlareVisualSettings& Visuals = M_FlareSettings.Visuals;
	if (IsValid(Visuals.LaunchEffect))
	{
		if (IsValid(SocketComponent))
		{
			// Attached so the smoke leaves the tube with the turret as it turns.
			UNiagaraFunctionLibrary::SpawnSystemAttached(Visuals.LaunchEffect, SocketComponent,
			                                             M_FlareSettings.LaunchSocketName, FVector::ZeroVector,
			                                             FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true);
		}
		else
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Visuals.LaunchEffect, LaunchLocation,
			                                               LaunchDirection.Rotation());
		}
	}
	if (IsValid(Visuals.LaunchSound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, Visuals.LaunchSound, LaunchLocation);
	}
}

uint8 UFlareAimAbilityComponent::GetOwningPlayer() const
{
	const AActor* Owner = GetOwner();
	const URTSComponent* RTSComponent = IsValid(Owner) ? Owner->FindComponentByClass<URTSComponent>() : nullptr;
	if (IsValid(RTSComponent))
	{
		return RTSComponent->GetOwningPlayer();
	}
	RTSFunctionLibrary::ReportError("Flare aim ability owner " + GetNameSafe(Owner) +
		" has no RTS component; its flare gives no player vision.");
	return 0;
}

bool UFlareAimAbilityComponent::GetIsValidTankMaster() const
{
	if (not M_TankMaster.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_TankMaster",
		                                                             "UFlareAimAbilityComponent::GetIsValidTankMaster",
		                                                             this);
		return false;
	}

	return true;
}
