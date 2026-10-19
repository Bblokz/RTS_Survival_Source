// Copyright (C) Bas Blokzijl - All rights reserved.

#include "PitMG42VisualComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

UPitMG42VisualComponent::UPitMG42VisualComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	M_MG42Mesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(
		TEXT("/Game/RTS_Survival/Meshes/Weapons/Ranged/StaticDefense/MG42/MG42.MG42")));
}

void UPitMG42VisualComponent::BeginPlay()
{
	Super::BeginPlay();

	USkeletalMeshComponent* ProxyTurretMesh = FindProxyTurretMesh();
	if (not IsValid(ProxyTurretMesh))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("Pit MG42 visual could not find its proxy Turret skeletal mesh."));
		return;
	}

	USkeletalMesh* MG42Mesh = M_MG42Mesh.LoadSynchronous();
	if (not IsValid(MG42Mesh))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("Pit MG42 visual could not load the configured MG42 skeletal mesh."));
		return;
	}

	CreateMG42Visual(*ProxyTurretMesh, *MG42Mesh);
}

USkeletalMeshComponent* UPitMG42VisualComponent::FindProxyTurretMesh() const
{
	AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return nullptr;
	}

	TArray<USkeletalMeshComponent*> SkeletalMeshComponents;
	Owner->GetComponents(SkeletalMeshComponents);
	for (USkeletalMeshComponent* SkeletalMeshComponent : SkeletalMeshComponents)
	{
		if (not IsValid(SkeletalMeshComponent))
		{
			continue;
		}
		if (SkeletalMeshComponent->GetFName() != M_ProxyComponentName)
		{
			continue;
		}
		return SkeletalMeshComponent;
	}
	return nullptr;
}

void UPitMG42VisualComponent::CreateMG42Visual(
	USkeletalMeshComponent& ProxyTurretMesh,
	USkeletalMesh& MG42Mesh) const
{
	AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return;
	}

	USkeletalMeshComponent* MG42Visual = NewObject<USkeletalMeshComponent>(Owner, TEXT("MG42Visual"));
	if (not IsValid(MG42Visual))
	{
		RTSFunctionLibrary::ReportError(TEXT("Pit MG42 visual component could not be created."));
		return;
	}

	Owner->AddInstanceComponent(MG42Visual);
	MG42Visual->SetupAttachment(&ProxyTurretMesh, M_AttachmentBoneName);
	MG42Visual->SetSkeletalMesh(&MG42Mesh);
	MG42Visual->SetRelativeTransform(M_MG42RelativeTransform);
	MG42Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MG42Visual->SetCollisionResponseToAllChannels(ECR_Ignore);
	MG42Visual->SetGenerateOverlapEvents(false);
	MG42Visual->SetCanEverAffectNavigation(false);
	MG42Visual->RegisterComponent();

	// Retain the animated DShK tripod and elevation rig, replacing only its gun silhouette.
	ProxyTurretMesh.HideBoneByName(M_AttachmentBoneName, EPhysBodyOp::PBO_None);
}
