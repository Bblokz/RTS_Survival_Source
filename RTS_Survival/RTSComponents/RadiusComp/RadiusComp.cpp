// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "RadiusComp.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

URadiusComp::URadiusComp(): M_ZScale(0), M_RenderHeight(0), bM_IsEnabled(true)
{
	PrimaryComponentTick.bCanEverTick = false;
	M_RadiusMesh = nullptr;
	M_Radius = 0.0f;
	M_UnitsPerScale = 1.0f;
	RadiusMeshComponent = nullptr;
}

void URadiusComp::BeginPlay()
{
	Super::BeginPlay();
	// Ensure the owner has a valid root component
	if (!GetOwner()->GetRootComponent())
	{
		RTSFunctionLibrary::ReportError("Owner has no RootComponent in URadiusComp::BeginPlay");
	}
}

void URadiusComp::SetNewMaterial(UMaterialInterface* NewMaterial) 
{
	if(!IsValid(NewMaterial))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "NewMaterial", "RadiusComp::SetNewMaterial");
	}
	if(IsValid(RadiusMeshComponent))
	{
		RadiusMeshComponent->SetMaterial(0, NewMaterial);
	}
	else
	{
		M_Material = NewMaterial;
	}
}

void URadiusComp::InitRadiusComp(UStaticMesh* RadiusMesh, const float Radius, const float UnitsPerScale,
                                 const float ZScale, const float RenderHeight)
{
	if (IsValid(RadiusMesh) && Radius > 0.0f && UnitsPerScale > 0.0f)
	{
		M_RadiusMesh = RadiusMesh;
		M_Radius = Radius;
		M_UnitsPerScale = UnitsPerScale;
		M_ZScale = ZScale > 0 ? ZScale : 1.0f;
		M_RenderHeight = RenderHeight > 0 ? RenderHeight : 0.0f;
	}
	else
	{
		const FString OwnerName = GetOwner() ? GetOwner()->GetName() : TEXT("NoOwner");
		RTSFunctionLibrary::ReportError(
			"Invalid parameters provided to URadiusComp::InitRadiusComp\nOwner: " + OwnerName);
	}
}

void URadiusComp::UpdateRadius(float Radius)
{
	if (Radius > 0.0f)
	{
		M_Radius = Radius;
		if (IsValid(RadiusMeshComponent))
		{
			ShowRadius();
		}
	}
	else
	{
		RTSFunctionLibrary::ReportError("Invalid radius provided to URadiusComp::UpdateRadius");
	}
}



void URadiusComp::SetEnabled(const bool bIsEnabled)
{
	bM_IsEnabled = bIsEnabled;
}

void URadiusComp::ShowRadius()
{
	if (M_RadiusMesh && M_Radius > 0.0f && M_UnitsPerScale > 0.0f)
	{
		// Check if the mesh component already exists
		if (!IsValid(RadiusMeshComponent))
		{
			// Create a new UStaticMeshComponent
			RadiusMeshComponent = NewObject<UStaticMeshComponent>(this);
			if (RadiusMeshComponent)
			{
				if(M_Material)
				{
					RadiusMeshComponent->SetMaterial(0, M_Material);
					M_Material = nullptr;
				}
				// Set up the mesh component
				RadiusMeshComponent->SetSimulatePhysics(false);
				RadiusMeshComponent->SetStaticMesh(M_RadiusMesh);
				RadiusMeshComponent->AttachToComponent(
					GetOwner()->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
				RadiusMeshComponent->SetRelativeLocation(
					RadiusMeshComponent->GetRelativeLocation() + FVector(0, 0, M_RenderHeight));

				float ScaleValue = 2 * M_Radius / M_UnitsPerScale;
				FVector NewScale(ScaleValue, ScaleValue, M_ZScale);
				RadiusMeshComponent->SetRelativeScale3D(NewScale);

				// Set collision settings
				RadiusMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				RadiusMeshComponent->SetCollisionProfileName(TEXT("NoCollision"));
				RadiusMeshComponent->SetCanEverAffectNavigation(false);

				RadiusMeshComponent->SetCastShadow(false);

				RadiusMeshComponent->RegisterComponent();
			}
			else
			{
				RTSFunctionLibrary::ReportError(
					"Failed to create Radius Mesh Component in URadiusComp::ShowRadius");
			}
		}
		else
		{
			RadiusMeshComponent->SetHiddenInGame(false);
			// If component exists, make sure it's visible and update scale in case parameters changed
			RadiusMeshComponent->SetVisibility(true);

			// Recalculate the scale in case M_Radius or M_UnitsPerScale have changed
			float ScaleValue = 2 * M_Radius / M_UnitsPerScale;
			FVector NewScale(ScaleValue, ScaleValue, M_ZScale);
			RadiusMeshComponent->SetRelativeScale3D(NewScale);
		}
		RTSFunctionLibrary::PrintString("My render height: " + FString::SanitizeFloat(RadiusMeshComponent->GetRelativeLocation().Z)
			+ "\n Set value: " + FString::SanitizeFloat(M_RenderHeight));
	}
	else
	{
		const FString OwnerName = GetOwner() ? GetOwner()->GetName() : TEXT("NoOwner");
		RTSFunctionLibrary::ReportError(
			"Invalid parameters in URadiusComp::ShowRadius\nOwner: " + OwnerName);
	}
}

void URadiusComp::HideRadius()
{
	if (IsValid(RadiusMeshComponent))
	{
		RadiusMeshComponent->SetHiddenInGame(true);
	}
}
