// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "PooledRadiusActor.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "RTS_Survival/RTSComponents/RadiusComp/RadiusComp.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

APooledRadiusActor::APooledRadiusActor()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// Ensure we always have a root (fixes "Owner has no RootComponent" in URadiusComp::BeginPlay / ShowRadius).
	M_RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	if (not IsValid(M_RootScene))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, TEXT("Root"), TEXT("APooledRadiusActor::APooledRadiusActor"));
	}
	else
	{
		SetRootComponent(M_RootScene);
	}

	M_RadiusComp = CreateDefaultSubobject<URadiusComp>(TEXT("RadiusComp"));
	if (not IsValid(M_RadiusComp))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, TEXT("RadiusComp"),
		                                             TEXT("APooledRadiusActor::APooledRadiusActor"));
		return;
	}

	// Keep pooled actors lightweight.
	SetActorEnableCollision(false);
}

void APooledRadiusActor::BeginPlay()
{
	Super::BeginPlay();
	SetActorHiddenInGame(true);
}

bool APooledRadiusActor::GetIsValidRadiusComp() const
{
	if (IsValid(M_RadiusComp))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, TEXT("M_RadiusComp"), TEXT("GetIsValidRadiusComp"), this);
	return false;
}

bool APooledRadiusActor::GetIsValidRootScene() const
{
	if (IsValid(M_RootScene))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, TEXT("M_RootScene"), TEXT("GetIsValidRootScene"), this);
	return false;
}

void APooledRadiusActor::InitRadiusActor(UStaticMesh* RadiusMesh, const float StartingRadius, const float UnitsPerScale,
                                         const float ZScale, const float RenderHeight)
{
	if (not GetIsValidRadiusComp() || not GetIsValidRootScene())
	{
		return;
	}

	M_RadiusComp->InitRadiusComp(RadiusMesh, StartingRadius, UnitsPerScale, ZScale, RenderHeight);
	M_RadiusComp->HideRadius();

	SetActorHiddenInGame(true);
	bM_InUse = false;
}

void APooledRadiusActor::ActivateRadiusAt(const FVector& WorldLocation, const float Radius,
                                          UMaterialInterface* Material)
{
	if (not GetIsValidRadiusComp())
	{
		return;
	}

	// Ensure detached activation (caller may attach later).
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	SetActorLocation(WorldLocation);

	if (IsValid(Material))
	{
		M_RadiusComp->SetNewMaterial(Material);
	}

	M_RadiusComp->UpdateRadius(Radius);
	M_RadiusComp->ShowRadius();

	SetActorHiddenInGame(false);
	bM_InUse = true;

	if (const UWorld* World = GetWorld())
	{
		M_LastUsedWorldSeconds = World->GetTimeSeconds();
	}
	else
	{
		M_LastUsedWorldSeconds = 0.0f;
	}
}

void APooledRadiusActor::DeactivateRadius()
{
	if (not GetIsValidRadiusComp())
	{
		return;
	}

	// If attached, keep world transform and detach so Hide is always safe.
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	M_RadiusComp->HideRadius();
	SetActorHiddenInGame(true);
	bM_InUse = false;
}

void APooledRadiusActor::ApplyMaterial(UMaterialInterface* NewMaterial)
{
	if (not GetIsValidRadiusComp())
	{
		return;
	}
	M_RadiusComp->SetNewMaterial(NewMaterial);
}

void APooledRadiusActor::AttachToTargetActor(AActor* TargetActor, const FVector& RelativeOffset)
{
	if (not IsValid(TargetActor))
	{
		RTSFunctionLibrary::ReportError(TEXT("APooledRadiusActor::AttachToTargetActor - TargetActor invalid."));
		return;
	}

	// Attach and apply requested local offset; zero rotation by default.
	AttachToActor(TargetActor, FAttachmentTransformRules::KeepWorldTransform);
	SetActorRelativeLocation(RelativeOffset);
	SetActorRelativeRotation(FRotator::ZeroRotator);
}
