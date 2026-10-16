// Copyright (C) Bas Blokzijl - All rights reserved.


#include "StaticPreviewMesh.h"

#include "Components/BoxComponent.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"

FName AStaticPreviewMesh::PreviewMeshComponentName(TEXT("PreviewMesh"));

// Sets default values
AStaticPreviewMesh::AStaticPreviewMesh(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	M_PreviewMesh = CreateDefaultSubobject<UStaticMeshComponent>(PreviewMeshComponentName);
	if (IsValid(M_PreviewMesh))
	{
		M_PreviewMesh->BodyInstance.bSimulatePhysics = false;
		M_PreviewMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		M_PreviewMesh->SetGenerateOverlapEvents(true);
		M_PreviewMesh->SetCanEverAffectNavigation(false);
		RootComponent = M_PreviewMesh;
	}
	else
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this,
		                                             "PreviewMesh",
		                                             "AStaticPreviewMesh::AStaticPreviewMesh");
	}
}

void AStaticPreviewMesh::SetPreviewMesh(
	UStaticMesh* NewPreviewMesh,
	UMaterialInstance* PreviewMaterial)
{
	if (!IsValid(M_PreviewMesh))
	{
		return;
	}
	M_PreviewMesh->SetStaticMesh(NewPreviewMesh);

	for (int32 i = 0; i < M_PreviewMesh->GetNumMaterials(); ++i)
	{
		M_PreviewMesh->SetMaterial(i, PreviewMaterial);
	}
}

void AStaticPreviewMesh::BeginPlay()
{
	Super::BeginPlay();
	FRTS_CollisionSetup::SetupStaticBuildingPreviewCollision(M_PreviewMesh, true);
}

void AStaticPreviewMesh::BlockPlacementFootprint(const FBox2D& FootprintArea, const float CenterHeight)
{
	if (not FootprintArea.bIsValid)
	{
		RTSFunctionLibrary::ReportError("Cannot block an invalid footprint area for static preview: " + GetName());
		return;
	}
	using namespace DeveloperSettings::GamePlay::Construction::FieldDefense;
	if (not IsValid(M_FootprintBlocker))
	{
		M_FootprintBlocker = NewObject<UBoxComponent>(this, TEXT("FootprintBlocker"));
		if (not IsValid(M_FootprintBlocker))
		{
			RTSFunctionLibrary::ReportError("Failed to create the footprint blocker for static preview: " + GetName());
			return;
		}
		M_FootprintBlocker->SetupAttachment(RootComponent);
		// The footprint is laid out on the world aligned building grid, so the box must not turn with the preview.
		M_FootprintBlocker->SetUsingAbsoluteRotation(true);
		M_FootprintBlocker->SetHiddenInGame(true);
		M_FootprintBlocker->SetCanEverAffectNavigation(false);
		M_FootprintBlocker->SetGenerateOverlapEvents(false);
		M_FootprintBlocker->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		M_FootprintBlocker->SetCollisionObjectType(COLLISION_OBJ_BUILDING_PLACEMENT);
		M_FootprintBlocker->SetCollisionResponseToAllChannels(ECR_Ignore);
		// The grid overlay counts every component that overlaps this channel as an occupied tile.
		M_FootprintBlocker->SetCollisionResponseToChannel(COLLISION_OBJ_BUILDING_PLACEMENT, ECR_Overlap);
		M_FootprintBlocker->RegisterComponent();
	}
	const FVector2D HalfSize = FootprintArea.GetExtent();
	const FVector BlockerExtent(
		FMath::Max(1.f, HalfSize.X - PlannedFootprintBlockerInset),
		FMath::Max(1.f, HalfSize.Y - PlannedFootprintBlockerInset),
		PlannedFootprintBlockerHalfHeight);
	const FVector2D Center = FootprintArea.GetCenter();
	M_FootprintBlocker->SetWorldLocationAndRotation(FVector(Center.X, Center.Y, CenterHeight), FRotator::ZeroRotator);
	M_FootprintBlocker->SetBoxExtent(BlockerExtent);
}
