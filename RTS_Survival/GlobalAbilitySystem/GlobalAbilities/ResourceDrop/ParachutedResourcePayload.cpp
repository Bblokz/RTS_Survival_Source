// Copyright (C) Bas Blokzijl - All rights reserved.

#include "ParachutedResourcePayload.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"
#include "RTS_Survival/Resources/Resource.h"
#include "RTS_Survival/Resources/ResourceComponent/ResourceComponent.h"
#include "RTS_Survival/Scavenging/AutoScavengeTrigger/AutoScavengeTriggerComponent.h"
#include "RTS_Survival/Scavenging/ScavengeObject/ScavengableObject.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace ParachutedResourcePayloadConstants
{
	constexpr float ParachuteOpenFraction = 0.12f;
	constexpr float ParachuteClosedScale = 0.05f;
	constexpr float SwayAmplitudeDegrees = 6.f;
	constexpr float SwayPeriodSeconds = 2.4f;
	// The canopy brakes the fall early on: most of the height goes in the first part of the time.
	constexpr float FallEaseExponent = 1.6f;
	// Gap between the top of the payload and the canopy's lines.
	constexpr float ParachuteGapAbovePayload = 40.f;
	// The sway dies out over the last part of the fall.
	constexpr float SwaySettleStartFraction = 0.8f;
	// Name of the scavenge mesh component of BP_ScavengeableObject; any static mesh is used when it is missing.
	const FName ContainerMeshComponentName(TEXT("Mesh"));
	const FName DepositTotalAmountPropertyName(TEXT("TotalAmount"));
}

AParachutedResourcePayload::AParachutedResourcePayload()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetCanBeDamaged(false);

	M_Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(M_Root);
	M_FallingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FallingMesh"));
	M_FallingMesh->SetupAttachment(M_Root);
	M_ParachuteMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ParachuteMesh"));
	M_ParachuteMesh->SetupAttachment(M_Root);

	for (UStaticMeshComponent* Mesh : {M_FallingMesh.Get(), M_ParachuteMesh.Get()})
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->SetReceivesDecals(false);
	}
}

void AParachutedResourcePayload::StartFall(const FVector& ReleaseLocation, const FVector& GroundLocation,
                                           UClass* PayloadClass, const FResourceDropSettings& Settings)
{
	M_ReleaseLocation = ReleaseLocation;
	M_GroundLocation = GroundLocation;
	M_PayloadClass = PayloadClass;
	M_Settings = Settings;
	M_FallingMesh->SetStaticMesh(Settings.FallSettings.FallingMesh);
	M_FallingMesh->SetRelativeScale3D(FVector(Settings.FallSettings.FallingMeshScale));
	M_ParachuteMesh->SetStaticMesh(Settings.FallSettings.ParachuteMesh);
	StartFall_PlaceParachuteAboveMesh();
	SetActorLocation(ReleaseLocation);
	SetActorTickEnabled(true);
}

void AParachutedResourcePayload::StartFall_PlaceParachuteAboveMesh() const
{
	using namespace ParachutedResourcePayloadConstants;
	const UStaticMesh* FallingMesh = M_FallingMesh->GetStaticMesh();
	const float PayloadTop = IsValid(FallingMesh)
		                         ? FallingMesh->GetBoundingBox().Max.Z * M_Settings.FallSettings.FallingMeshScale
		                         : 0.f;
	M_ParachuteMesh->SetRelativeLocation(FVector(0.f, 0.f, PayloadTop + ParachuteGapAbovePayload));
	M_ParachuteMesh->SetRelativeScale3D(FVector(ParachuteClosedScale));
}

void AParachutedResourcePayload::Tick(const float DeltaSeconds)
{
	using namespace ParachutedResourcePayloadConstants;
	Super::Tick(DeltaSeconds);
	M_SecondsFalling += DeltaSeconds;
	const float FallAlpha = FMath::Clamp(M_SecondsFalling / M_Settings.FallSettings.FallSeconds, 0.f, 1.f);
	if (FallAlpha >= 1.f)
	{
		Land();
		return;
	}

	const float HeightAlpha = FMath::InterpEaseOut(0.f, 1.f, FallAlpha, FallEaseExponent);
	SetActorLocation(FMath::Lerp(M_ReleaseLocation, M_GroundLocation, HeightAlpha));

	const float OpenAlpha = FMath::Clamp(FallAlpha / ParachuteOpenFraction, 0.f, 1.f);
	const float OpenScale = FMath::Lerp(ParachuteClosedScale, M_Settings.FallSettings.ParachuteScale, OpenAlpha);
	M_ParachuteMesh->SetRelativeScale3D(FVector(OpenScale));
	// Settles in the last part of the fall so the payload touches down upright.
	const float SettleAlpha = 1.f - FMath::Clamp(
		(FallAlpha - SwaySettleStartFraction) / (1.f - SwaySettleStartFraction), 0.f, 1.f);
	const float SwayAngle = SwayAmplitudeDegrees * OpenAlpha * SettleAlpha * FMath::Sin(
		M_SecondsFalling * UE_TWO_PI / SwayPeriodSeconds);
	SetActorRotation(FRotator(SwayAngle, GetActorRotation().Yaw, 0.f));
}

void AParachutedResourcePayload::Land()
{
	SetActorTickEnabled(false);
	if (IsValid(M_Settings.FallSettings.LandingEffect))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, M_Settings.FallSettings.LandingEffect, M_GroundLocation);
	}
	if (IsValid(M_Settings.FallSettings.LandingSound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, M_Settings.FallSettings.LandingSound, M_GroundLocation);
	}
	AActor* SpawnedResource = SpawnLandedResource();
	OnPayloadLanded.Broadcast(this, SpawnedResource);
	Destroy();
}

AActor* AParachutedResourcePayload::SpawnLandedResource() const
{
	UWorld* World = GetWorld();
	if (not IsValid(World) || not IsValid(M_PayloadClass))
	{
		RTSFunctionLibrary::ReportError(TEXT("AParachutedResourcePayload landed without a world or payload class."));
		return nullptr;
	}
	const FTransform LandedTransform(FRotator(0.f, GetActorRotation().Yaw, 0.f), M_GroundLocation);
	switch (M_Settings.PayloadKind)
	{
	case EResourceDropPayloadKind::ScavengeContainer:
		return ResourceDropSpawning::SpawnScavengeContainer(*World, M_PayloadClass, LandedTransform,
		                                                    M_Settings.ContainerSettings);
	case EResourceDropPayloadKind::HarvestableDeposit:
		return ResourceDropSpawning::SpawnHarvestableDeposit(*World, M_PayloadClass, LandedTransform,
		                                                     M_Settings.DepositSettings);
	}
	return nullptr;
}

namespace ResourceDropSpawningPrivate
{
	UStaticMeshComponent* FindContainerMesh(const AScavengeableObject& Container)
	{
		TArray<UStaticMeshComponent*> MeshComponents;
		Container.GetComponents<UStaticMeshComponent>(MeshComponents);
		for (UStaticMeshComponent* MeshComponent : MeshComponents)
		{
			if (IsValid(MeshComponent)
				&& MeshComponent->GetFName() == ParachutedResourcePayloadConstants::ContainerMeshComponentName)
			{
				return MeshComponent;
			}
		}
		return MeshComponents.IsEmpty() ? nullptr : MeshComponents[0];
	}

	void ApplyContainerMesh(AScavengeableObject& Container, const FResourceDropContainerSettings& Settings)
	{
		if (not IsValid(Settings.ContainerMesh))
		{
			return;
		}
		UStaticMeshComponent* MeshComponent = FindContainerMesh(Container);
		if (not IsValid(MeshComponent))
		{
			RTSFunctionLibrary::ReportError(TEXT("Resource drop container has no static mesh component for its mesh: ")
				+ Container.GetName());
			return;
		}
		MeshComponent->SetStaticMesh(Settings.ContainerMesh);
		MeshComponent->SetRelativeScale3D(FVector(Settings.ContainerMeshScale));
	}

	void AddAutoScavengeTrigger(AScavengeableObject& Container, const float Radius)
	{
		if (Radius <= 0.f)
		{
			return;
		}
		UAutoScavengeTriggerComponent* Trigger = NewObject<UAutoScavengeTriggerComponent>(
			&Container, TEXT("ResourceDropAutoScavengeTrigger"));
		Trigger->SetupAttachment(Container.GetRootComponent());
		Trigger->SetSphereRadius(Radius);
		Container.AddInstanceComponent(Trigger);
		// The container has begun play, so registering also begins play and binds the overlap.
		Trigger->RegisterComponent();
	}

	void RemoveContainerOnceEmptied(AScavengeableObject& Container, const float RemoveAfterSeconds)
	{
		TWeakObjectPtr<AScavengeableObject> WeakContainer(&Container);
		Container.OnScavengedOrDestroyed.AddWeakLambda(&Container, [WeakContainer, RemoveAfterSeconds]()
		{
			AScavengeableObject* EmptiedContainer = WeakContainer.Get();
			UWorld* World = IsValid(EmptiedContainer) ? EmptiedContainer->GetWorld() : nullptr;
			if (not IsValid(World))
			{
				return;
			}
			// The reward text is a component of the container: hide the crate now, keep the actor for the text.
			TArray<UStaticMeshComponent*> MeshComponents;
			EmptiedContainer->GetComponents<UStaticMeshComponent>(MeshComponents);
			for (UStaticMeshComponent* MeshComponent : MeshComponents)
			{
				MeshComponent->SetVisibility(false);
				MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
			FTimerHandle RemoveHandle;
			World->GetTimerManager().SetTimer(RemoveHandle, FTimerDelegate::CreateWeakLambda(
				                                  EmptiedContainer, [WeakContainer]()
				                                  {
					                                  if (WeakContainer.IsValid())
					                                  {
						                                  WeakContainer->Destroy();
					                                  }
				                                  }), FMath::Max(RemoveAfterSeconds, KINDA_SMALL_NUMBER), false);
		});
	}
}

AScavengeableObject* ResourceDropSpawning::SpawnScavengeContainer(UWorld& World, UClass* ContainerClass,
                                                                 const FTransform& Transform,
                                                                 const FResourceDropContainerSettings& Settings)
{
	using namespace ResourceDropSpawningPrivate;
	if (not IsValid(ContainerClass) || not ContainerClass->IsChildOf(AScavengeableObject::StaticClass()))
	{
		RTSFunctionLibrary::ReportError(TEXT("Resource drop container class is not a scavengeable object."));
		return nullptr;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AScavengeableObject* Container = World.SpawnActor<AScavengeableObject>(ContainerClass, Transform,
	                                                                      SpawnParameters);
	if (not IsValid(Container))
	{
		return nullptr;
	}
	// Blueprint components only exist after spawning; the scavenge collision is on the component, not the mesh.
	ApplyContainerMesh(*Container, Settings);
	Container->SetFixedScavengeRewards(Settings.RewardsPerContainer, Settings.ScavengeSeconds);
	AddAutoScavengeTrigger(*Container, Settings.AutoScavengeRadius);
	RemoveContainerOnceEmptied(*Container, Settings.RemoveAfterEmptiedSeconds);
	return Container;
}

ACPPResourceMaster* ResourceDropSpawning::SpawnHarvestableDeposit(UWorld& World, UClass* DepositClass,
                                                                 const FTransform& Transform,
                                                                 const FResourceDropDepositSettings& Settings)
{
	if (not IsValid(DepositClass) || not DepositClass->IsChildOf(ACPPResourceMaster::StaticClass()))
	{
		RTSFunctionLibrary::ReportError(TEXT("Resource drop deposit class is not a resource."));
		return nullptr;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	FTransform ScaledTransform = Transform;
	ScaledTransform.SetScale3D(FVector(Settings.DepositActorScale));
	ACPPResourceMaster* Deposit = World.SpawnActor<ACPPResourceMaster>(DepositClass, ScaledTransform,
	                                                                  SpawnParameters);
	UResourceComponent* ResourceComponent = IsValid(Deposit) ? Deposit->GetResourceComponent() : nullptr;
	if (not IsValid(ResourceComponent))
	{
		RTSFunctionLibrary::ReportError(TEXT("Resource drop deposit spawned without a resource component."));
		return Deposit;
	}
	// The amount is protected and rolled by the construction script; the drop always holds the configured amount.
	// Only the depletion visuals still scale with the rolled amount, which the crystal Blueprints do not use.
	const FIntProperty* TotalAmountProperty = FindFProperty<FIntProperty>(
		UResourceComponent::StaticClass(), ParachutedResourcePayloadConstants::DepositTotalAmountPropertyName);
	if (TotalAmountProperty == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("UResourceComponent::TotalAmount not found; the deposit keeps its own amount."));
		return Deposit;
	}
	TotalAmountProperty->SetPropertyValue_InContainer(ResourceComponent, Settings.DepositAmount);
	return Deposit;
}
