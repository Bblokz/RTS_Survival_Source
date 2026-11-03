// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTSPooledRagdoll.h"

#include "Components/SkeletalMeshComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace RTSPooledRagdollStatics
{
	const FName CorpseCollisionProfile(TEXT("RagdollCorpse"));
	const FVector ParkingLocation(0.f, 0.f, -30000.f);
	// The launch moves the whole body; the kick on the upper spine makes the limbs trail behind it.
	const FName KickBone(TEXT("spine_02"));
	constexpr float BoneKickShare = 0.3f;
	const FName PelvisBone(TEXT("pelvis"));
}

ARTSPooledRagdoll::ARTSPooledRagdoll()
{
	PrimaryActorTick.bCanEverTick = false;
	M_CrewMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CrewMesh"));
	RootComponent = M_CrewMesh;
	M_CrewMesh->SetCanEverAffectNavigation(false);
	M_CrewMesh->SetGenerateOverlapEvents(false);
	M_CrewMesh->bPauseAnims = true;
	M_CrewMesh->bBlendPhysics = true;
	M_CrewMesh->SetCollisionProfileName(RTSPooledRagdollStatics::CorpseCollisionProfile);
}

void ARTSPooledRagdoll::InitializeForPool(USkeletalMesh* CrewMesh, UPhysicalMaterial* CorpseMaterial)
{
	if (not GetIsValidCrewMesh())
	{
		return;
	}
	M_CrewMesh->SetSkeletalMeshAsset(CrewMesh);
	M_CrewMesh->SetPhysMaterialOverride(CorpseMaterial);
	Park();
}

void ARTSPooledRagdoll::Wake(const FVector& Location, const FRotator& Rotation, UMaterialInterface* Material)
{
	if (not GetIsValidCrewMesh())
	{
		return;
	}
	M_CrewMesh->bNoSkeletonUpdate = false;
	SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::ResetPhysics);
	// A frozen mesh does not update its bones, so its bodies would stay parked and simulate far below the map.
	M_CrewMesh->UpdateKinematicBonesToAnim(M_CrewMesh->GetComponentSpaceTransforms(), ETeleportType::TeleportPhysics,
	                                       false, EAllowKinematicDeferral::DisallowDeferral);
	if (IsValid(Material))
	{
		M_CrewMesh->SetMaterial(0, Material);
	}
	SetActorHiddenInGame(false);
	M_CrewMesh->SetComponentTickEnabled(true);
	M_CrewMesh->SetSimulatePhysics(true);
	M_CrewMesh->WakeAllRigidBodies();
}

void ARTSPooledRagdoll::Launch(const FVector& Velocity, const FVector& SpinDegrees)
{
	if (not GetIsValidCrewMesh())
	{
		return;
	}
	using namespace RTSPooledRagdollStatics;
	M_CrewMesh->WakeAllRigidBodies();
	M_CrewMesh->SetAllPhysicsLinearVelocity(Velocity, false);
	M_CrewMesh->SetAllPhysicsAngularVelocityInDegrees(SpinDegrees, false);
	if (M_CrewMesh->GetBodyInstance(KickBone) != nullptr)
	{
		M_CrewMesh->AddImpulse(Velocity * BoneKickShare, KickBone, true);
	}
}

void ARTSPooledRagdoll::Freeze()
{
	if (not GetIsValidCrewMesh())
	{
		return;
	}
	// Without bone updates the mesh keeps the pose the simulation left it in.
	M_CrewMesh->bNoSkeletonUpdate = true;
	M_CrewMesh->SetSimulatePhysics(false);
}

void ARTSPooledRagdoll::Sink(const float DepthCm)
{
	if (not GetIsValidCrewMesh())
	{
		return;
	}
	AddActorWorldOffset(FVector(0.f, 0.f, -DepthCm));
}

void ARTSPooledRagdoll::Park()
{
	if (not GetIsValidCrewMesh())
	{
		return;
	}
	Freeze();
	M_CrewMesh->SetComponentTickEnabled(false);
	SetActorHiddenInGame(true);
	SetActorLocation(RTSPooledRagdollStatics::ParkingLocation, false, nullptr, ETeleportType::ResetPhysics);
}

bool ARTSPooledRagdoll::GetIsAsleep() const
{
	return IsValid(M_CrewMesh) && not M_CrewMesh->RigidBodyIsAwake();
}

FTransform ARTSPooledRagdoll::GetPelvisTransform() const
{
	if (not GetIsValidCrewMesh())
	{
		return GetActorTransform();
	}
	// Bones only refresh while the mesh is rendered, so a body that fell out of view would report where it woke;
	// the physics body always knows where it lies.
	if (const FBodyInstance* PelvisBody = M_CrewMesh->GetBodyInstance(RTSPooledRagdollStatics::PelvisBone))
	{
		if (PelvisBody->IsValidBodyInstance())
		{
			return PelvisBody->GetUnrealWorldTransform();
		}
	}
	return M_CrewMesh->GetBoneIndex(RTSPooledRagdollStatics::PelvisBone) != INDEX_NONE
		       ? M_CrewMesh->GetBoneTransform(RTSPooledRagdollStatics::PelvisBone)
		       : M_CrewMesh->GetComponentTransform();
}

bool ARTSPooledRagdoll::GetIsValidCrewMesh() const
{
	if (IsValid(M_CrewMesh))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_CrewMesh", "ARTSPooledRagdoll::GetIsValidCrewMesh");
	return false;
}
