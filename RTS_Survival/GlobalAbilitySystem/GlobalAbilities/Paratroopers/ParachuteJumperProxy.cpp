// Copyright (C) Bas Blokzijl - All rights reserved.

#include "ParachuteJumperProxy.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"

namespace ParachuteJumperProxyPrivate
{
	bool GetIsDrawnPart(const UMeshComponent& Part)
	{
		if (Part.IsA<UInstancedStaticMeshComponent>())
		{
			return false;
		}
		const bool bIsCopyableMesh = Part.IsA<UStaticMeshComponent>() || Part.IsA<USkeletalMeshComponent>();
		return bIsCopyableMesh && Part.IsRegistered() && Part.GetVisibleFlag() && not Part.bHiddenInGame;
	}

	/** @return The ancestor of Part that hangs directly on SourceMesh, or null when Part is not under it. */
	const USceneComponent* FindAncestorOnSourceMesh(const USceneComponent& Part, const USkeletalMeshComponent& SourceMesh)
	{
		const USceneComponent* Cursor = &Part;
		while (IsValid(Cursor) && Cursor->GetAttachParent() != &SourceMesh)
		{
			Cursor = Cursor->GetAttachParent();
		}
		return Cursor;
	}
}

AParachuteJumperProxy::AParachuteJumperProxy()
{
	PrimaryActorTick.bCanEverTick = false;
	M_Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(M_Root);
	M_BodyMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("BodyMesh"));
	M_BodyMesh->SetupAttachment(M_Root);
	M_BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	M_BodyMesh->SetCanEverAffectNavigation(false);
	M_BodyMesh->SetGenerateOverlapEvents(false);
	// The proxy is only seen while it falls; keep the pose running even before it was first rendered.
	M_BodyMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
}

AParachuteJumperProxy* AParachuteJumperProxy::SpawnForSquadUnit(
	ASquadUnit* SquadUnit,
	UStaticMesh* ParachuteMesh,
	UAnimMontage* ParachuteMontage,
	const FVector& SpawnLocation,
	const float Yaw)
{
	if (not IsValid(SquadUnit) || not IsValid(SquadUnit->GetMesh()) || not IsValid(SquadUnit->GetWorld()))
	{
		return nullptr;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AParachuteJumperProxy* Proxy = SquadUnit->GetWorld()->SpawnActor<AParachuteJumperProxy>(
		SpawnLocation, FRotator(0.0f, Yaw, 0.0f), SpawnParameters);
	if (not IsValid(Proxy))
	{
		return nullptr;
	}
	const USkeletalMeshComponent& SourceMesh = *SquadUnit->GetMesh();
	Proxy->CopyBody(SourceMesh);
	Proxy->CopyAttachedMeshes(*SquadUnit, SourceMesh);
	Proxy->AttachParachute(ParachuteMesh);
	Proxy->PlayHangAnimation(ParachuteMontage);
	return Proxy;
}

void AParachuteJumperProxy::CopyBody(const USkeletalMeshComponent& SourceMesh)
{
	M_BodyMesh->SetSkeletalMeshAsset(SourceMesh.GetSkeletalMeshAsset());
	M_BodyMesh->SetRelativeScale3D(SourceMesh.GetComponentScale());
	for (int32 MaterialIndex = 0; MaterialIndex < SourceMesh.GetNumMaterials(); ++MaterialIndex)
	{
		M_BodyMesh->SetMaterial(MaterialIndex, SourceMesh.GetMaterial(MaterialIndex));
	}
}

void AParachuteJumperProxy::CopyAttachedMeshes(const ASquadUnit& SquadUnit, const USkeletalMeshComponent& SourceMesh)
{
	// The weapon is a child actor on a hand socket: walk the attached actors as well as the soldier's own parts.
	TArray<AActor*> SourceActors;
	SquadUnit.GetAttachedActors(SourceActors, true, true);
	SourceActors.Add(const_cast<ASquadUnit*>(&SquadUnit));
	for (const AActor* SourceActor : SourceActors)
	{
		TArray<UMeshComponent*> Parts;
		SourceActor->GetComponents<UMeshComponent>(Parts);
		for (const UMeshComponent* Part : Parts)
		{
			if (Part == &SourceMesh || not ParachuteJumperProxyPrivate::GetIsDrawnPart(*Part))
			{
				continue;
			}
			CopyAttachedMesh(*Part, SourceMesh);
		}
	}
}

void AParachuteJumperProxy::CopyAttachedMesh(const UMeshComponent& SourcePart, const USkeletalMeshComponent& SourceMesh)
{
	const USceneComponent* AncestorOnMesh = ParachuteJumperProxyPrivate::FindAncestorOnSourceMesh(
		SourcePart, SourceMesh);
	if (not IsValid(AncestorOnMesh))
	{
		return;
	}
	const FName SocketName = AncestorOnMesh->GetAttachSocketName();
	// Relative to the socket the part hangs from, so it follows the proxy's own pose of that bone.
	const FTransform SocketTransform = SourceMesh.GetSocketTransform(SocketName);
	const FTransform PartToSocket = SourcePart.GetComponentTransform().GetRelativeTransform(SocketTransform);

	UMeshComponent* Copy = nullptr;
	if (const UStaticMeshComponent* StaticPart = Cast<UStaticMeshComponent>(&SourcePart))
	{
		UStaticMeshComponent* StaticCopy = NewObject<UStaticMeshComponent>(this);
		StaticCopy->SetStaticMesh(StaticPart->GetStaticMesh());
		Copy = StaticCopy;
	}
	else if (const USkeletalMeshComponent* SkeletalPart = Cast<USkeletalMeshComponent>(&SourcePart))
	{
		USkeletalMeshComponent* SkeletalCopy = NewObject<USkeletalMeshComponent>(this);
		SkeletalCopy->SetSkeletalMeshAsset(SkeletalPart->GetSkeletalMeshAsset());
		if (SkeletalPart->LeaderPoseComponent.Get() == &SourceMesh)
		{
			SkeletalCopy->SetLeaderPoseComponent(M_BodyMesh);
		}
		Copy = SkeletalCopy;
	}
	if (not IsValid(Copy))
	{
		return;
	}
	Copy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Copy->SetCanEverAffectNavigation(false);
	Copy->SetGenerateOverlapEvents(false);
	for (int32 MaterialIndex = 0; MaterialIndex < SourcePart.GetNumMaterials(); ++MaterialIndex)
	{
		Copy->SetMaterial(MaterialIndex, SourcePart.GetMaterial(MaterialIndex));
	}
	Copy->SetupAttachment(M_BodyMesh, SocketName);
	Copy->SetUsingAbsoluteScale(true);
	Copy->SetRelativeTransform(FTransform(PartToSocket.GetRotation(), PartToSocket.GetLocation()));
	Copy->SetRelativeScale3D(SourcePart.GetComponentScale());
	Copy->RegisterComponent();
}

void AParachuteJumperProxy::AttachParachute(UStaticMesh* ParachuteMesh)
{
	if (not IsValid(ParachuteMesh))
	{
		return;
	}
	// Same as a jumping soldier: the parachute is authored in the soldier mesh's space.
	UStaticMeshComponent* Parachute = NewObject<UStaticMeshComponent>(this);
	Parachute->SetStaticMesh(ParachuteMesh);
	Parachute->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Parachute->SetCanEverAffectNavigation(false);
	Parachute->SetupAttachment(M_BodyMesh);
	Parachute->RegisterComponent();
}

void AParachuteJumperProxy::PlayHangAnimation(const UAnimMontage* ParachuteMontage)
{
	if (not IsValid(ParachuteMontage) || ParachuteMontage->SlotAnimTracks.IsEmpty())
	{
		return;
	}
	const TArray<FAnimSegment>& Segments = ParachuteMontage->SlotAnimTracks[0].AnimTrack.AnimSegments;
	if (Segments.IsEmpty())
	{
		return;
	}
	UAnimSequenceBase* HangAnimation = Segments[0].GetAnimReference();
	if (not IsValid(HangAnimation))
	{
		return;
	}
	M_BodyMesh->PlayAnimation(HangAnimation, true);
}
