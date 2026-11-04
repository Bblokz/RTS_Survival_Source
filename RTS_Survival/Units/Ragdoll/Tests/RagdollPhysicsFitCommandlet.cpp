// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RagdollPhysicsFitCommandlet.h"

#if WITH_EDITOR

#include "Engine/SkeletalMesh.h"
#include "MeshUtilitiesCommon.h"
#include "Misc/PackageName.h"
#include "PhysicsAssetUtils.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogRagdollPhysicsFit, Log, All);

namespace RagdollPhysicsFitPrivate
{
	constexpr int32 RootBoneIndex = 0;
	constexpr int32 FittedLodIndex = 0;

	TArray<FTransform> GetRefComponentSpace(const FReferenceSkeleton& RefSkeleton)
	{
		const TArray<FTransform>& LocalPose = RefSkeleton.GetRefBonePose();
		TArray<FTransform> ComponentSpace;
		ComponentSpace.SetNum(LocalPose.Num());
		for (int32 BoneIndex = 0; BoneIndex < LocalPose.Num(); ++BoneIndex)
		{
			const int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
			ComponentSpace[BoneIndex] = ParentIndex == INDEX_NONE
				                            ? LocalPose[BoneIndex]
				                            : LocalPose[BoneIndex] * ComponentSpace[ParentIndex];
		}
		return ComponentSpace;
	}

	/** The bone itself when it has a body, else the closest parent that has one; INDEX_NONE up to the root. */
	int32 GetCarryingBone(const FReferenceSkeleton& RefSkeleton, const TSet<int32>& BodyBones, int32 BoneIndex)
	{
		while (BoneIndex != INDEX_NONE && BoneIndex != RootBoneIndex)
		{
			if (BodyBones.Contains(BoneIndex))
			{
				return BoneIndex;
			}
			BoneIndex = RefSkeleton.GetParentIndex(BoneIndex);
		}
		return INDEX_NONE;
	}

	int32 GetDominantBone(const FSoftSkinVertex& Vertex, const FSkelMeshSection& Section)
	{
		int32 MainInfluence = 0;
		for (int32 Influence = 1; Influence < MAX_TOTAL_INFLUENCES; ++Influence)
		{
			if (Vertex.InfluenceWeights[Influence] > Vertex.InfluenceWeights[MainInfluence])
			{
				MainInfluence = Influence;
			}
		}
		return Section.BoneMap.IsValidIndex(Vertex.InfluenceBones[MainInfluence])
			       ? Section.BoneMap[Vertex.InfluenceBones[MainInfluence]]
			       : INDEX_NONE;
	}

	/** Vertices per body bone in that bone's reference space, as the fitter expects them. */
	TArray<FBoneVertInfo> GatherBodyVertices(const USkeletalMesh* Mesh, const TSet<int32>& BodyBones,
	                                         int32& OutSkippedVertices)
	{
		const FReferenceSkeleton& RefSkeleton = Mesh->GetRefSkeleton();
		const TArray<FTransform> RefComponentSpace = GetRefComponentSpace(RefSkeleton);
		TArray<FBoneVertInfo> Infos;
		Infos.SetNum(RefSkeleton.GetNum());
		OutSkippedVertices = 0;
		const FSkeletalMeshLODModel& Lod = Mesh->GetImportedModel()->LODModels[FittedLodIndex];
		for (const FSkelMeshSection& Section : Lod.Sections)
		{
			for (const FSoftSkinVertex& Vertex : Section.SoftVertices)
			{
				const int32 CarryingBone = GetCarryingBone(RefSkeleton, BodyBones, GetDominantBone(Vertex, Section));
				if (CarryingBone == INDEX_NONE)
				{
					++OutSkippedVertices;
					continue;
				}
				const FTransform& BoneSpace = RefComponentSpace[CarryingBone];
				Infos[CarryingBone].Positions.Add(
					FVector3f(BoneSpace.InverseTransformPosition(FVector(Vertex.Position))));
				Infos[CarryingBone].Normals.Add(
					FVector3f(BoneSpace.InverseTransformVectorNoScale(FVector(Vertex.TangentZ))));
			}
		}
		return Infos;
	}

	struct FCapsuleSegment
	{
		FVector Start = FVector::ZeroVector;
		FVector End = FVector::ZeroVector;
		float Radius = 0.f;
	};

	TArray<FCapsuleSegment> GetRefPoseCapsules(const UPhysicsAsset* PhysicsAsset, const USkeletalMesh* Mesh,
	                                           const TArray<FTransform>& RefComponentSpace, const int32 BodyIndex)
	{
		TArray<FCapsuleSegment> Segments;
		const USkeletalBodySetup* BodySetup = PhysicsAsset->SkeletalBodySetups[BodyIndex];
		const int32 BoneIndex = Mesh->GetRefSkeleton().FindBoneIndex(BodySetup->BoneName);
		if (not RefComponentSpace.IsValidIndex(BoneIndex))
		{
			return Segments;
		}
		for (const FKSphylElem& Capsule : BodySetup->AggGeom.SphylElems)
		{
			const FTransform CapsuleToComponent = Capsule.GetTransform() * RefComponentSpace[BoneIndex];
			const FVector HalfAxis = CapsuleToComponent.GetUnitAxis(EAxis::Z) * Capsule.Length * 0.5f;
			Segments.Add({CapsuleToComponent.GetLocation() - HalfAxis, CapsuleToComponent.GetLocation() + HalfAxis,
			              Capsule.Radius});
		}
		return Segments;
	}

	bool GetDoCapsulesOverlap(const TArray<FCapsuleSegment>& First, const TArray<FCapsuleSegment>& Second)
	{
		for (const FCapsuleSegment& A : First)
		{
			for (const FCapsuleSegment& B : Second)
			{
				FVector ClosestOnA;
				FVector ClosestOnB;
				FMath::SegmentDistToSegmentSafe(A.Start, A.End, B.Start, B.End, ClosestOnA, ClosestOnB);
				if (FVector::Dist(ClosestOnA, ClosestOnB) < A.Radius + B.Radius)
				{
					return true;
				}
			}
		}
		return false;
	}

	/**
	 * Bodies that already overlap in the rest pose push each other apart for as long as they simulate, so the
	 * ragdoll twitches on the ground; their collision with each other is switched off.
	 * @return How many pairs were switched off.
	 */
	int32 DisableOverlappingPairs(UPhysicsAsset* PhysicsAsset, const USkeletalMesh* Mesh)
	{
		const TArray<FTransform> RefComponentSpace = GetRefComponentSpace(Mesh->GetRefSkeleton());
		const int32 BodyCount = PhysicsAsset->SkeletalBodySetups.Num();
		TArray<TArray<FCapsuleSegment>> Capsules;
		for (int32 BodyIndex = 0; BodyIndex < BodyCount; ++BodyIndex)
		{
			Capsules.Add(GetRefPoseCapsules(PhysicsAsset, Mesh, RefComponentSpace, BodyIndex));
		}
		int32 Disabled = 0;
		for (int32 First = 0; First < BodyCount; ++First)
		{
			for (int32 Second = First + 1; Second < BodyCount; ++Second)
			{
				if (PhysicsAsset->IsCollisionEnabled(First, Second) && GetDoCapsulesOverlap(Capsules[First],
					Capsules[Second]))
				{
					PhysicsAsset->DisableCollision(First, Second);
					++Disabled;
				}
			}
		}
		return Disabled;
	}

	void LogBodies(const UPhysicsAsset* PhysicsAsset, const TCHAR* Label)
	{
		for (const USkeletalBodySetup* BodySetup : PhysicsAsset->SkeletalBodySetups)
		{
			FString Shapes;
			for (const FKSphylElem& Capsule : BodySetup->AggGeom.SphylElems)
			{
				Shapes += FString::Printf(TEXT(" r=%.1f l=%.1f"), Capsule.Radius, Capsule.Length);
			}
			UE_LOG(LogRagdollPhysicsFit, Display, TEXT("RTS_RPF %s bone=%s%s"), Label, *BodySetup->BoneName.ToString(),
			       *Shapes);
		}
	}

	bool SaveAsset(UObject* Asset)
	{
		UPackage* Package = Asset->GetOutermost();
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		return UPackage::SavePackage(Package, Asset, *Filename, SaveArgs);
	}
}

int32 URagdollPhysicsFitCommandlet::Main(const FString& Params)
{
	using namespace RagdollPhysicsFitPrivate;
	FString MeshPath;
	FString PhysicsAssetPath;
	FParse::Value(*Params, TEXT("Mesh="), MeshPath);
	FParse::Value(*Params, TEXT("PhysicsAsset="), PhysicsAssetPath);
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
	UPhysicsAsset* PhysicsAsset = LoadObject<UPhysicsAsset>(nullptr, *PhysicsAssetPath);
	if (not IsValid(Mesh) || not IsValid(PhysicsAsset) || Mesh->GetImportedModel() == nullptr
		|| not Mesh->GetImportedModel()->LODModels.IsValidIndex(FittedLodIndex))
	{
		UE_LOG(LogRagdollPhysicsFit, Error, TEXT("RTS_RPF missing mesh or physics asset: %s %s"), *MeshPath,
		       *PhysicsAssetPath);
		return 1;
	}
	LogBodies(PhysicsAsset, TEXT("before"));
	TSet<int32> BodyBones;
	TArray<int32> BodyIndices;
	for (int32 BodyIndex = 0; BodyIndex < PhysicsAsset->SkeletalBodySetups.Num(); ++BodyIndex)
	{
		const int32 BoneIndex = Mesh->GetRefSkeleton().FindBoneIndex(
			PhysicsAsset->SkeletalBodySetups[BodyIndex]->BoneName);
		if (BoneIndex != INDEX_NONE)
		{
			BodyBones.Add(BoneIndex);
			BodyIndices.Add(BodyIndex);
		}
	}
	int32 SkippedVertices = 0;
	const TArray<FBoneVertInfo> Infos = GatherBodyVertices(Mesh, BodyBones, SkippedVertices);
	FPhysAssetCreateParams FitParams;
	FitParams.GeomType = EFG_Sphyl;
	TArray<int32> FittedBodies;
	FPhysicsAssetUtils::CreateCollisionsFromBones(PhysicsAsset, Mesh, BodyIndices, FitParams, Infos, FittedBodies);
	const int32 DisabledPairs = DisableOverlappingPairs(PhysicsAsset, Mesh);
	UE_LOG(LogRagdollPhysicsFit, Display, TEXT("RTS_RPF overlapping_pairs_disabled=%d"), DisabledPairs);
	PhysicsAsset->UpdateBoundsBodiesArray();
	PhysicsAsset->UpdateBodySetupIndexMap();
	PhysicsAsset->MarkPackageDirty();
	LogBodies(PhysicsAsset, TEXT("after"));
	const bool bSaved = SaveAsset(PhysicsAsset);
	UE_LOG(LogRagdollPhysicsFit, Display, TEXT("RTS_RPF fitted=%d of %d skipped_root_vertices=%d saved=%d"),
	       FittedBodies.Num(), BodyIndices.Num(), SkippedVertices, bSaved ? 1 : 0);
	return bSaved ? 0 : 1;
}

#else

int32 URagdollPhysicsFitCommandlet::Main(const FString& Params)
{
	return 1;
}

#endif
