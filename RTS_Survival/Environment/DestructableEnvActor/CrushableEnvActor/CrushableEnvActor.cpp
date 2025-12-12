// Copyright (C) Bas Blokzijl - All rights reserved.

#include "CrushableEnvActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollection.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "DrawDebugHelpers.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Environment/InstanceHelpers/FRTSInstanceHelpers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h" // Contains RTSFunctionLibrary::ReportError

ACrushableEnvActor::ACrushableEnvActor(const FObjectInitializer& ObjectInitializer)
	: ADestructableEnvActor(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	// Set tick to 2.0 seconds.
	PrimaryActorTick.TickInterval = 2.0f;
}

void ACrushableEnvActor::BeginPlay()
{
	Super::BeginPlay();
}

void ACrushableEnvActor::CreateGeometryComponentsForInstances(UInstancedStaticMeshComponent* InstStaticMesh, const float ZOffsetGeoComponents)
{
	if (!InstStaticMesh)
	{
		return;
	}

	// Get the number of instances.
	int32 InstanceCount = InstStaticMesh->GetInstanceCount();

	FInstancedGeoMapping NewMapping;
	NewMapping.GeoComponents.Empty();
	NewMapping.bIsUsingGeometry = false; 

	UStaticMesh* StaticMesh = InstStaticMesh->GetStaticMesh();
	if (!StaticMesh)
	{
		RTSFunctionLibrary::ReportError(
			"Instanced static mesh component has no static mesh assigned for crushable actor: " + GetName());
		return;
	}

	UGeometryCollection* GeoCollection = nullptr;
	if (StaticToGeometryMapping.Contains(StaticMesh))
	{
		GeoCollection = StaticToGeometryMapping[StaticMesh];
	}
	else
	{
		RTSFunctionLibrary::ReportError(
			"No Geometry Collection mapping found for static mesh: " + StaticMesh->GetName());
		return;
	}

	// For each instance in the instanced mesh, create a corresponding geometry component.
	for (int32 i = 0; i < InstanceCount; i++)
	{
		FTransform InstanceTransform;
		if (InstStaticMesh->GetInstanceTransform(i, InstanceTransform, true))
		{
			InstanceTransform.SetLocation( InstanceTransform.GetLocation() + FVector(0, 0, ZOffsetGeoComponents) );
			UGeometryCollectionComponent* GeoComp = NewObject<UGeometryCollectionComponent>(this);
			if (GeoComp)
			{
				GeoComp->SetSimulatePhysics(true);
				GeoComp->RegisterComponent();
				GeoComp->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
				GeoComp->SetWorldTransform(InstanceTransform);
				GeoComp->SetRestCollection(GeoCollection);
				GeoComp->SetCanEverAffectNavigation(false);
				GeoComp->SetLinearDamping(1);
				GeoComp->SetAngularDamping(1);
				GeoComp->SetEnableGravity(false);
				GeoComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				GeoComp->SetVisibility(false, true);

				NewMapping.GeoComponents.Add(GeoComp);
			}
		}
	}

	// Save the mapping: associate this instanced static mesh component with its geometry components and state.
	InstanceToGeometryMap.Add(InstStaticMesh, NewMapping);
}

void ACrushableEnvActor::SetupHierarchicalInstanceBox(UInstancedStaticMeshComponent* InstStaticMesh,
                                                      const FVector2D BoxBounds,
                                                      const float DistanceBetweenInstances, const float ZOffsetForGeoComponents)
{
	// --- Clear previously created geometry components ---
	for (auto& Pair : InstanceToGeometryMap)
	{
		for (UGeometryCollectionComponent* GeoComp : Pair.Value.GeoComponents)
		{
			if (GeoComp)
			{
				GeoComp->DestroyComponent();
			}
		}
	}
	// Use library function to create the box of instances.
	FRTSInstanceHelpers::SetupHierarchicalInstanceBox(InstStaticMesh, BoxBounds, DistanceBetweenInstances, this);
	// Create geometry components for each instance and update the mapping.
	CreateGeometryComponentsForInstances(InstStaticMesh, ZOffsetForGeoComponents);
}

void ACrushableEnvActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Get the player controller and its pawn location.
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !PC->GetPawn())
	{
		return;
	}
	FVector PlayerLocation = PC->GetPawn()->GetActorLocation();

	// Compute the distance between the player and this actor.
	const float Distance = FVector::Dist(PlayerLocation, GetActorLocation());
	if(DeveloperSettings::Debugging::GCrushableActors_Compile_DebugSymbols)
	{
	// at  700 units above the actor prinst string the distance:
	DrawDebugString(GetWorld(), GetActorLocation() + FVector(0, 0, 700),
	                TEXT("Distance: ") + FString::SanitizeFloat(Distance), nullptr, FColor::White, DeltaTime, true);
		
	}
	// If within the specified range, we want to switch to geometry mode for the entire component.
	const bool bShouldUseGeometry = (Distance <= RangeSwitchToGeometry);

	// Process each instanced static mesh component.
	for (auto& Pair : InstanceToGeometryMap)
	{
		UInstancedStaticMeshComponent* InstSM = Pair.Key;
		FInstancedGeoMapping& GeoMapping = Pair.Value;

		if (!InstSM)
		{
			continue;
		}

		// Only switch the entire component if its current mode is different from the desired mode.
		if (GeoMapping.bIsUsingGeometry != bShouldUseGeometry)
		{
			FVector CompLocation = InstSM->GetComponentLocation();
			if (bShouldUseGeometry)
			{
				// Switch to geometry: hide the instanced mesh and enable all geometry components.
				InstSM->SetHiddenInGame(true);
				if(DeveloperSettings::Debugging::GCrushableActors_Compile_DebugSymbols)
				{
				DrawDebugString(GetWorld(), CompLocation + FVector(0, 0, 400), TEXT("Switch to Geometry"), nullptr,
				                FColor::Green, 0.f, true);
				
				}
				for (UGeometryCollectionComponent* GeoComp : GeoMapping.GeoComponents)
				{
					if (GeoComp)
					{
						GeoComp->CastShadow = 1;
						GeoComp->SetVisibility(true, true);
						GeoComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
						GeoComp->SetLinearDamping(1);
						GeoComp->SetAngularDamping(1);
						GeoComp->SetSimulatePhysics(true);
						if(DeveloperSettings::Debugging::GCrushableActors_Compile_DebugSymbols)
						{
							const float AngularDamping = GeoComp->GetAngularDamping();
							const float LinearDamping = GeoComp->GetLinearDamping();
							RTSFunctionLibrary::PrintString("angular damping: " + FString::SanitizeFloat(AngularDamping));
							RTSFunctionLibrary::PrintString("linear damping: " + FString::SanitizeFloat(LinearDamping));
							
						}
					}
				}
			}
			else
			{
				// Switch to instance: show the instanced mesh and disable all geometry components.
				InstSM->SetHiddenInGame(false);
				
				if(DeveloperSettings::Debugging::GCrushableActors_Compile_DebugSymbols)
				{
					DrawDebugString(GetWorld(), CompLocation + FVector(0, 0, 400), TEXT("Switch to Instance"), nullptr,
									FColor::Red, 0.f, true);
					
				}
				for (UGeometryCollectionComponent* GeoComp : GeoMapping.GeoComponents)
				{
					if (GeoComp)
					{
						GeoComp->CastShadow = 0;
						GeoComp->SetVisibility(false, true);
						GeoComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					}
				}
			}
			// Update the stored state for this component.
			GeoMapping.bIsUsingGeometry = bShouldUseGeometry;
		}
	}
}
