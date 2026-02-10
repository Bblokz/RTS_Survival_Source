// Copyright (C) Bas Blokzijl - All rights reserved.

#include "InstancedDestrucablesEnvActor.h"

#include "NiagaraFunctionLibrary.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/AssetManager.h"
#include "Engine/DamageEvents.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Environment/InstanceHelpers/FRTSInstanceHelpers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

AInstancedDestrucablesEnvActor::AInstancedDestrucablesEnvActor(const FObjectInitializer& ObjectInitializer)
	: ADestructableEnvActor(ObjectInitializer),
	  CollapseFX(nullptr),
	  CollapseSound(nullptr),
	  CollapseSoundAttenuation(nullptr),
	  CollapseSoundConcurrency(nullptr)
{
	PrimaryActorTick.bCanEverTick = false;
}

void AInstancedDestrucablesEnvActor::BeginPlay()
{
	Super::BeginPlay();

	// Always destroy any previous destroyed mesh instancer before creating a new one.
	if (M_DestroyedMeshComponent)
	{
		M_DestroyedMeshComponent->DestroyComponent();
		M_DestroyedMeshComponent = nullptr;
	}

	if (not IsValid(InstancedMeshComponent))
	{
		return;
	}
	// Create a new destroyed mesh instancer matching the type of the regular instancer.
	if (InstancedMeshComponent->IsA(UHierarchicalInstancedStaticMeshComponent::StaticClass()))
	{
		M_DestroyedMeshComponent = NewObject<UHierarchicalInstancedStaticMeshComponent>(
			this, UHierarchicalInstancedStaticMeshComponent::StaticClass(),
			TEXT("DestroyedInstancedMeshComponent"));
	}
	else
	{
		M_DestroyedMeshComponent = NewObject<UInstancedStaticMeshComponent>(
			this, UInstancedStaticMeshComponent::StaticClass(), TEXT("DestroyedInstancedMeshComponent"));
	}
	if (not IsValid(M_DestroyedMeshComponent))
	{
		RTSFunctionLibrary::ReportError(
			"Failed to create destroyed mesh instancer for instanced destructable: " + GetName());
		return;
	}
	M_DestroyedMeshComponent->SetupAttachment(RootComponent);
	M_DestroyedMeshComponent->RegisterComponent();
	M_DestroyedMeshComponent->SetCanEverAffectNavigation(false);
	M_DestroyedMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	M_DestroyedMeshComponent->SetSimulatePhysics(false);
	AsyncLoadDestroyedMesh();
}

void AInstancedDestrucablesEnvActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (InstancedMeshComponent)
	{
		const int32 NumInstances = InstancedMeshComponent->GetInstanceCount();
		M_Health.SetNum(NumInstances);
		for (int32 i = 0; i < NumInstances; ++i)
		{
			M_Health[i] = HealthPerInstance;
			FTransform InstanceTransform;
			InstancedMeshComponent->GetInstanceTransform(i, InstanceTransform, true);
			M_InstanceLocations.Add(InstanceTransform.GetLocation());
		}
	}
}

void AInstancedDestrucablesEnvActor::SetupHierarchicalInstanceBox(UInstancedStaticMeshComponent* InstStaticMesh,
                                                                  const FVector2D BoxBounds, const float DistanceBetweenInstances)
{
	FRTSInstanceHelpers::SetupHierarchicalInstanceBox(
		InstStaticMesh, BoxBounds, DistanceBetweenInstances, this);
}

void AInstancedDestrucablesEnvActor::SetupHierarchicalInstanceAlongRoad(const FRTSRoadInstanceSetup RoadSetup,
                                                                        UInstancedStaticMeshComponent* InstStaticMesh) const
{
	FRTSInstanceHelpers::SetupHierarchicalInstanceAlongRoad(
		RoadSetup,
		InstStaticMesh,
		this);
}

float AInstancedDestrucablesEnvActor::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
                                                 AController* EventInstigator, AActor* DamageCauser)
{
	// Access point damage data so we can get the hit location and instance index.
	const FPointDamageEvent* const PointDamageEvent = DamageEvent.IsOfType(FPointDamageEvent::ClassID)
		                                                  ? static_cast<const FPointDamageEvent*>(&DamageEvent)
		                                                  : nullptr;
	if (!PointDamageEvent)
	{
		// Not a point damage event; ignore damage.
		return 1.f;
	}

	const FHitResult& Hit = PointDamageEvent->HitInfo;

	// Prepare call back for the closest instance.
	TWeakObjectPtr<AInstancedDestrucablesEnvActor> WeakThis(this);
	auto OnFoundClosest = [WeakThis, DamageAmount](const int Index)-> void
	{
		if (WeakThis.IsValid())
		{
			WeakThis->OnFoundClosestInstance(Index, DamageAmount);
		}
	};

	auto Locations = M_InstanceLocations;
	const FVector HitLocation = Hit.Location;
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [WeakThis, OnFoundClosest, Locations, HitLocation]()-> void
	{
		// find the index of the closest location to HitLocation in the provided Locations.
		int32 ClosestIndex = 0;
		float ClosestDistance = FVector::DistSquared(HitLocation, Locations[0]);
		for (int32 i = 1; i < Locations.Num(); ++i)
		{
			const float Distance = FVector::DistSquared(HitLocation, Locations[i]);
			if (Distance < ClosestDistance)
			{
				ClosestIndex = i;
				ClosestDistance = Distance;
			}
		}
		AsyncTask(ENamedThreads::GameThread, [OnFoundClosest, ClosestIndex]()
		{
			OnFoundClosest(ClosestIndex);
		});
	});
	return DamageAmount;
}

void AInstancedDestrucablesEnvActor::OnInstanceDestroyed(const int32 InstanceIndex) const
{
	if (!InstancedMeshComponent)
	{
		return;
	}

	FTransform InstanceTransform;
	if (!InstancedMeshComponent->GetInstanceTransform(InstanceIndex, InstanceTransform, true))
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to get instance transform for destroyed instance."));
		return;
	}
	FTransform NewInstanceTransform = InstanceTransform + FTransform(
		FRotator::ZeroRotator, FVector(0, 0, -500), FVector::OneVector);
	NewInstanceTransform.Rotator().Normalize();
	// Remove the intact instance.
	if (not InstancedMeshComponent->UpdateInstanceTransform(InstanceIndex, NewInstanceTransform, true, true, true))
	{
		RTSFunctionLibrary::ReportError(
			"Could not move instance on (H)ISM component, a function AInstancedDestructablesEnvActor"
			"\n Index: " + FString::FromInt(InstanceIndex));
	}
	// Add an instance with the same transform to the destroyed mesh instancer.
	if (M_DestroyedMeshComponent)
	{
		InstanceTransform.SetLocation(InstanceTransform.GetLocation() + FVector(0,0,ZOffsetForDestroyedMesh));
		M_DestroyedMeshComponent->AddInstance(InstanceTransform);
		M_DestroyedMeshComponent->MarkRenderStateDirty();
	}
	else
	{
		RTSFunctionLibrary::ReportError(TEXT("Destroyed mesh instancer is not valid."));
	}

	// Play collapse effects at the instance location offset by FXOffset.
	if (CollapseFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), CollapseFX,
		                                               InstanceTransform.GetLocation() + FXOffset);
	}
	if (CollapseSound)
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), CollapseSound,
		                                      InstanceTransform.GetLocation() + FXOffset, FRotator::ZeroRotator,
		                                      1.f, 1.f, 0.f, CollapseSoundAttenuation, CollapseSoundConcurrency);
	}
}


void AInstancedDestrucablesEnvActor::AsyncLoadDestroyedMesh() const
{
	// Set the static mesh for the destroyed instances.
	if (not IsValid(M_DestroyedMeshComponent))
	{
		RTSFunctionLibrary::ReportError(TEXT("Destroyed Mesh component is not valid"));
		return;
	}
	if (UStaticMesh* const LoadedMesh = DestroyedMesh.Get())
	{
		M_DestroyedMeshComponent->SetStaticMesh(LoadedMesh);
		return;
	}
	FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
	const FSoftObjectPath MeshPath = DestroyedMesh.ToSoftObjectPath();
	TWeakObjectPtr<AInstancedDestrucablesEnvActor> WeakInstancedDestructableActor = this;
	Streamable.RequestAsyncLoad(MeshPath, FStreamableDelegate::CreateLambda([WeakInstancedDestructableActor]()
	{
		if (not WeakInstancedDestructableActor.IsValid())
		{
			return;
		}

		AInstancedDestrucablesEnvActor* StrongInstancedDestructableActor = WeakInstancedDestructableActor.Get();
		if (UStaticMesh* const LoadedMesh = StrongInstancedDestructableActor->DestroyedMesh.Get())
		{
			if (StrongInstancedDestructableActor->M_DestroyedMeshComponent)
			{
				StrongInstancedDestructableActor->M_DestroyedMeshComponent->SetStaticMesh(LoadedMesh);
			}
		}
	}));
}

void AInstancedDestrucablesEnvActor::OnFoundClosestInstance(const int32 InstanceIndex, const float DamageToDeal)
{
	if (not IsValid(InstancedMeshComponent))
	{
		return;
	}
	if (InstanceIndex < 0 or InstanceIndex >= M_Health.Num() || not M_Health.IsValidIndex(InstanceIndex))
	{
		RTSFunctionLibrary::ReportError("Invalid instance index found: " + FString::FromInt(InstanceIndex) +
			"\n could also be invalid on cached health array which has size: " + FString::FromInt(M_Health.Num()) +
			"\n Will not adjust health of any instance on : " + GetName());
		return;
	}
	
	if(M_Health[InstanceIndex] <= 0)
	{
		if constexpr (DeveloperSettings::Debugging::GDestructableActors_Compile_DebugSymbols)
		{
				RTSFunctionLibrary::PrintString("Instance already destroyed: " + GetName() + " at index: " + FString::FromInt(InstanceIndex));
			return;
		}
	}
	M_Health[InstanceIndex] -= DamageToDeal;
	if(M_Health[InstanceIndex] <= 0)
	{
		OnInstanceDestroyed(InstanceIndex);
	}
	if constexpr (DeveloperSettings::Debugging::GDestructableActors_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Damage taken for actor: " + GetName() +
			"\n at Instance Index: " + FString::FromInt(InstanceIndex));
	}
	
}
