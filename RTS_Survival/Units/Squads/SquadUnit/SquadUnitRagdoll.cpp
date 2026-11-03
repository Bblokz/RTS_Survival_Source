// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SquadUnitRagdoll.h"

#include "AnimSquadUnit/SquadUnitAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/BodyInstance.h"
#include "RTS_Survival/Units/RTSDeathType/RTSDeathType.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "TimerManager.h"

namespace SquadUnitRagdollStatics
{
	const FName CorpseCollisionProfile(TEXT("RagdollCorpse"));
	// Most of the launch moves the whole body; the rest kicks one bone so the limbs trail behind it.
	constexpr float WholeBodyLaunchShare = 0.7f;
	constexpr float BoneKickLaunchShare = 0.8f;
	// A walking soldier's whole speed on top of the hit sends him sliding; he keeps only part of it.
	constexpr float InheritedVelocityShare = 0.5f;
	// Well above real ground, as a body with default friction glides for metres after it lands.
	constexpr float CorpseFriction = 3.f;
}

FSquadUnitRagdoll::FSquadUnitRagdoll()
	: bEnableRagdoll(true)
	  , bRagdollAtOnceOnKineticDeath(true)
	  , RagdollCollisionProfileName(SquadUnitRagdollStatics::CorpseCollisionProfile)
	  , MinHitSpeed(150.0f)
	  , MaxHitSpeed(450.0f)
	  , MinBlastSpeed(300.0f)
	  , MaxBlastSpeed(650.0f)
	  , BlastUpwardRatio(0.8f)
	  , ImpulseBoneName(TEXT("spine_02"))
	  , TimeTillDeath(3.0f)
	  , CorpseSinkSeconds(1.5f)
	  , CorpseSinkDepthCm(60.0f)
	  , LinearDamping(0.5f)
	  , AngularDamping(0.5f)
	  , MassScale(1.0f)
	  , bIsRagdollActive(false)
{
}

UPhysicalMaterial* FSquadUnitRagdoll::CreateCorpseMaterial(UObject* Outer)
{
	UPhysicalMaterial* CorpseMaterial = NewObject<UPhysicalMaterial>(Outer);
	CorpseMaterial->Friction = SquadUnitRagdollStatics::CorpseFriction;
	CorpseMaterial->bOverrideFrictionCombineMode = true;
	CorpseMaterial->FrictionCombineMode = EFrictionCombineMode::Max;
	CorpseMaterial->Restitution = 0.0f;
	return CorpseMaterial;
}

bool FSquadUnitRagdoll::GetCanRagdoll(const USkeletalMeshComponent* MeshComponent) const
{
	return bEnableRagdoll && IsValid(MeshComponent) && MeshComponent->GetPhysicsAsset() != nullptr;
}

bool FSquadUnitRagdoll::GetShouldSkipDeathMontage(const ERTSDeathType DeathType,
                                                  const USkeletalMeshComponent* MeshComponent) const
{
	return bRagdollAtOnceOnKineticDeath && DeathType == ERTSDeathType::Kinetic && GetCanRagdoll(MeshComponent);
}

void FSquadUnitRagdoll::StartRagdoll(
	AActor* OwningActor,
	USkeletalMeshComponent* MeshComponent,
	USquadUnitAnimInstance* AnimInstance,
	const FSquadUnitDeathHit& DeathHit,
	const FVector& InheritedVelocity)
{
	if (not bEnableRagdoll)
	{
		return;
	}

	if (not IsValid(MeshComponent))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(
			OwningActor,
			"MeshComponent",
			"FSquadUnitRagdoll::StartRagdoll");
		return;
	}

	StopAnimations(AnimInstance, MeshComponent);
	SetupPhysicsAndCollision(MeshComponent);
	ScheduleLaunch(OwningActor, MeshComponent, GetLaunchVelocity(DeathHit), InheritedVelocity);
	StartRagdoll_CreateEffect(OwningActor);

	bIsRagdollActive = true;
}

void FSquadUnitRagdoll::FreezeCorpse(USkeletalMeshComponent* MeshComponent) const
{
	if (not IsValid(MeshComponent))
	{
		return;
	}
	// Without bone updates the mesh keeps the pose the simulation left it in.
	MeshComponent->bNoSkeletonUpdate = true;
	MeshComponent->SetSimulatePhysics(false);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void FSquadUnitRagdoll::StopAnimations(
	USquadUnitAnimInstance* AnimInstance,
	USkeletalMeshComponent* MeshComponent) const
{
	if (IsValid(AnimInstance))
	{
		AnimInstance->StopAllMontages();
	}

	if (not IsValid(MeshComponent))
	{
		return;
	}

	MeshComponent->bPauseAnims = true;
}

void FSquadUnitRagdoll::SetupPhysicsAndCollision(USkeletalMeshComponent* MeshComponent) const
{
	if (not IsValid(MeshComponent))
	{
		return;
	}

	// The capsule stays where the soldier died; the body has to be free to leave it.
	MeshComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

	MeshComponent->SetCollisionProfileName(RagdollCollisionProfileName != NAME_None
		                                       ? RagdollCollisionProfileName
		                                       : SquadUnitRagdollStatics::CorpseCollisionProfile);

	MeshComponent->SetSimulatePhysics(true);
	MeshComponent->SetAllBodiesSimulatePhysics(true);
	MeshComponent->WakeAllRigidBodies();
	MeshComponent->bBlendPhysics = true;

	MeshComponent->SetPhysMaterialOverride(CreateCorpseMaterial(MeshComponent));

	const float ClampedLinearDamping = FMath::Max(0.0f, LinearDamping);
	const float ClampedAngularDamping = FMath::Max(0.0f, AngularDamping);
	const float ClampedMassScale = FMath::Max(0.01f, MassScale);

	MeshComponent->SetLinearDamping(ClampedLinearDamping);
	MeshComponent->SetAngularDamping(ClampedAngularDamping);
	MeshComponent->SetAllMassScale(ClampedMassScale);
}

FVector FSquadUnitRagdoll::GetLaunchVelocity(const FSquadUnitDeathHit& DeathHit) const
{
	if (DeathHit.PushDirection.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}
	if (not DeathHit.bIsBlast)
	{
		return DeathHit.PushDirection * FMath::Lerp(MinHitSpeed, MaxHitSpeed, DeathHit.DamageFraction);
	}
	const float BlastSpeed = FMath::Lerp(MinBlastSpeed, MaxBlastSpeed, DeathHit.DamageFraction);
	return DeathHit.PushDirection * BlastSpeed + FVector::UpVector * BlastSpeed * BlastUpwardRatio;
}

void FSquadUnitRagdoll::ScheduleLaunch(AActor* OwningActor, USkeletalMeshComponent* MeshComponent,
                                       const FVector& LaunchVelocity, const FVector& InheritedVelocity) const
{
	UWorld* World = IsValid(OwningActor) ? OwningActor->GetWorld() : nullptr;
	if (not IsValid(World))
	{
		return;
	}
	const TWeakObjectPtr<USkeletalMeshComponent> WeakMesh = MeshComponent;
	const FName BoneToKick = ImpulseBoneName;
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda(
		[WeakMesh, BoneToKick, LaunchVelocity, InheritedVelocity]()
		{
			USkeletalMeshComponent* Mesh = WeakMesh.Get();
			if (not IsValid(Mesh))
			{
				return;
			}
			using namespace SquadUnitRagdollStatics;
			Mesh->WakeAllRigidBodies();
			Mesh->SetAllPhysicsLinearVelocity(
				InheritedVelocity * InheritedVelocityShare + LaunchVelocity * WholeBodyLaunchShare, false);
			if (Mesh->GetBodyInstance(BoneToKick) != nullptr)
			{
				Mesh->AddImpulse(LaunchVelocity * BoneKickLaunchShare, BoneToKick, true);
			}
		}));
}

void FSquadUnitRagdoll::StartRagdoll_CreateEffect(AActor* OwningActor)
{
	if (not IsValid(OwningActor))
	{
		return;
	}
	DeathEffects.CreateFX(OwningActor);
}
