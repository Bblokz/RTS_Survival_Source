// Copyright (C) Bas Blokzijl - All rights reserved.


#include "StaticPreviewMesh.h"

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
