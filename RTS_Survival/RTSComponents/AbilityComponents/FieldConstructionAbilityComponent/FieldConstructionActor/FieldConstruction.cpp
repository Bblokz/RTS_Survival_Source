// Copyright (C) Bas Blokzijl - All rights reserved.


#include "FieldConstruction.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "TimerManager.h"


// Sets default values
AFieldConstruction::AFieldConstruction(const FObjectInitializer& ObjectInitializer)
	: ADestructableEnvActor(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void AFieldConstruction::BeginPlay()
{
	Super::BeginPlay();
}

void AFieldConstruction::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	FieldConstructionMesh = FindComponentByClass<UStaticMeshComponent>();
}

bool AFieldConstruction::GetIsValidFieldConstructionMesh()
{
	if (IsValid(FieldConstructionMesh))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"FieldConstructionMesh",
		"AFieldConstruction::GetIsValidFieldConstructionMesh",
		this);
	return false;
}

void AFieldConstruction::SetupCollision(const int32 OwningPlayer, const bool bBlockPlayerProjectiles)
{
	TArray<UMeshComponent*> MeshComponents;
	GetComponents<UMeshComponent>(MeshComponents);
	for (UMeshComponent* MeshComp : MeshComponents)
	{
		if (not IsValid(MeshComp))
		{
			continue;
		}
		FRTS_CollisionSetup::SetupFieldConstructionMeshCollision(MeshComp, OwningPlayer, bBlockPlayerProjectiles);
	}
}

void AFieldConstruction::InitialiseFromPreview(UStaticMeshComponent* PreviewMeshComponent, float ConstructionDuration)
{
	if (not GetIsValidFieldConstructionMesh())
	{
		return;
	}

	if (not IsValid(PreviewMeshComponent))
	{
		RTSFunctionLibrary::ReportError("Preview mesh component is invalid in AFieldConstruction::InitialiseFromPreview");
		return;
	}

	M_ConstructionTime = FMath::Max(ConstructionDuration, 0.f);
	M_PreviewMaterial = PreviewMeshComponent->GetMaterial(0);

	M_OriginalMaterials.Reset();
	const int32 NumMaterials = FieldConstructionMesh->GetNumMaterials();
	for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
	{
		M_OriginalMaterials.Add(FieldConstructionMesh->GetMaterial(MaterialIndex));
	}

	ApplyPreviewMaterial();

	if (M_ConstructionTime <= 0.f)
	{
		RestoreOriginalMaterialSlots(1.f);
		return;
	}

	if (UWorld* World = GetWorld())
	{
		constexpr float MaterialUpdateFrequencySeconds = 0.1f;
		World->GetTimerManager().SetTimer(
			M_ConstructionMaterialTimer,
			this,
			&AFieldConstruction::RestoreMaterialsOverTime,
			MaterialUpdateFrequencySeconds,
			true);
	}
}

void AFieldConstruction::StopConstructionMaterialTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_ConstructionMaterialTimer);
	}
}

void AFieldConstruction::ApplyPreviewMaterial()
{
	if (not GetIsValidFieldConstructionMesh())
	{
		return;
	}

	if (not IsValid(M_PreviewMaterial))
	{
		RTSFunctionLibrary::ReportError("Preview material invalid in AFieldConstruction::ApplyPreviewMaterial");
		return;
	}

	const int32 NumMaterials = FieldConstructionMesh->GetNumMaterials();
	for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
	{
		FieldConstructionMesh->SetMaterial(MaterialIndex, M_PreviewMaterial);
	}
}

void AFieldConstruction::RestoreMaterialsOverTime()
{
	if (not GetIsValidFieldConstructionMesh())
	{
		StopConstructionMaterialTimer();
		return;
	}

	if (M_ConstructionTime <= 0.f)
	{
		StopConstructionMaterialTimer();
		return;
	}

	UWorld* World = GetWorld();
	if (not World)
	{
		RTSFunctionLibrary::ReportError("World invalid while restoring construction materials.");
		StopConstructionMaterialTimer();
		return;
	}

	M_ConstructionElapsed += World->GetDeltaSeconds();
	const float BuildProgress = FMath::Clamp(M_ConstructionElapsed / M_ConstructionTime, 0.f, 1.f);
	RestoreOriginalMaterialSlots(BuildProgress);

	if (BuildProgress >= 1.f)
	{
		StopConstructionMaterialTimer();
	}
}

void AFieldConstruction::RestoreOriginalMaterialSlots(const float BuildProgress)
{
	if (not GetIsValidFieldConstructionMesh())
	{
		return;
	}

	if (M_OriginalMaterials.Num() <= 0)
	{
		return;
	}

	const int32 TotalSlots = FieldConstructionMesh->GetNumMaterials();
	const int32 SlotsToRestore = FMath::Clamp(
		FMath::CeilToInt(static_cast<float>(TotalSlots) * BuildProgress),
		0,
		TotalSlots);

	for (int32 MaterialIndex = 0; MaterialIndex < SlotsToRestore; ++MaterialIndex)
	{
		if (M_OriginalMaterials.IsValidIndex(MaterialIndex))
		{
			FieldConstructionMesh->SetMaterial(MaterialIndex, M_OriginalMaterials[MaterialIndex]);
		}
	}
}
