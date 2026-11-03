// Copyright (C) Bas Blokzijl - All rights reserved.

#include "CorpsePoseBaker.h"

#if WITH_EDITOR

#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "Misc/PackageName.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshCompiler.h"
#include "StaticMeshResources.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace CorpsePoseBakerPrivate
{
	constexpr int32 BakedLodIndex = 0;
	// Corpses are instanced in hundreds; denser soldier meshes are reduced to about this many triangles.
	constexpr int32 MaxCorpseTriangles = 8000;

	/** Per vertex of the imported LOD, in pivot space. */
	struct FSkinnedVertex
	{
		FVector3f Position = FVector3f::ZeroVector;
		FVector3f Normal = FVector3f::UpVector;
		FVector3f Tangent = FVector3f::ForwardVector;
		FVector2f UV = FVector2f::ZeroVector;
		float BinormalSign = 1.f;
		int32 MainBone = INDEX_NONE;
		// Whether a physics body moves this vertex; others (root, IK helper chains) stay where the body woke.
		bool bIsCarried = true;
	};

	/** @return Per bone, whether it has a physics body or hangs below one; root and IK chains do not. */
	TArray<bool> GetCarriedBones(const USkeletalMesh* SkeletalMesh)
	{
		const FReferenceSkeleton& RefSkeleton = SkeletalMesh->GetRefSkeleton();
		TArray<bool> CarriedBones;
		CarriedBones.Init(false, RefSkeleton.GetNum());
		const UPhysicsAsset* PhysicsAsset = SkeletalMesh->GetPhysicsAsset();
		if (PhysicsAsset == nullptr)
		{
			CarriedBones.Init(true, RefSkeleton.GetNum());
			return CarriedBones;
		}
		for (const USkeletalBodySetup* BodySetup : PhysicsAsset->SkeletalBodySetups)
		{
			const int32 BoneIndex = BodySetup ? RefSkeleton.FindBoneIndex(BodySetup->BoneName) : INDEX_NONE;
			if (CarriedBones.IsValidIndex(BoneIndex))
			{
				CarriedBones[BoneIndex] = true;
			}
		}
		// Parents come before children in the reference skeleton.
		for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); ++BoneIndex)
		{
			const int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
			CarriedBones[BoneIndex] |= CarriedBones.IsValidIndex(ParentIndex) && CarriedBones[ParentIndex];
		}
		return CarriedBones;
	}

	/** Reference pose to pivot space per bone: undo the bind pose, then apply the bone's current world pose. */
	TArray<FMatrix44f> GetRefToPivot(const USkeletalMeshComponent* CrewMesh, const FVector& Pivot)
	{
		const TArray<FMatrix44f>& InverseRefBases = CrewMesh->GetSkeletalMeshAsset()->GetRefBasesInvMatrix();
		const TArray<FTransform>& ComponentSpace = CrewMesh->GetComponentSpaceTransforms();
		FTransform ComponentToPivot = CrewMesh->GetComponentTransform();
		ComponentToPivot.AddToTranslation(-Pivot);
		TArray<FMatrix44f> RefToPivot;
		RefToPivot.SetNum(InverseRefBases.Num());
		for (int32 BoneIndex = 0; BoneIndex < InverseRefBases.Num(); ++BoneIndex)
		{
			const FTransform BoneToPivot = ComponentSpace.IsValidIndex(BoneIndex)
				                               ? ComponentSpace[BoneIndex] * ComponentToPivot
				                               : ComponentToPivot;
			RefToPivot[BoneIndex] = InverseRefBases[BoneIndex] * FMatrix44f(BoneToPivot.ToMatrixWithScale());
		}
		return RefToPivot;
	}

	FSkinnedVertex SkinVertex(const FSoftSkinVertex& SoftVertex, const FSkelMeshSection& Section,
	                          const TArray<FMatrix44f>& RefToPivot)
	{
		FSkinnedVertex Skinned;
		FVector3f Position = FVector3f::ZeroVector;
		FVector3f Normal = FVector3f::ZeroVector;
		FVector3f Tangent = FVector3f::ZeroVector;
		float TotalWeight = 0.f;
		float MainWeight = 0.f;
		for (int32 Influence = 0; Influence < MAX_TOTAL_INFLUENCES; ++Influence)
		{
			const float Weight = SoftVertex.InfluenceWeights[Influence];
			if (Weight <= 0.f || not Section.BoneMap.IsValidIndex(SoftVertex.InfluenceBones[Influence]))
			{
				continue;
			}
			const int32 BoneIndex = Section.BoneMap[SoftVertex.InfluenceBones[Influence]];
			if (not RefToPivot.IsValidIndex(BoneIndex))
			{
				continue;
			}
			const FMatrix44f& Matrix = RefToPivot[BoneIndex];
			Position += Matrix.TransformPosition(SoftVertex.Position) * Weight;
			Normal += Matrix.TransformVector(FVector3f(SoftVertex.TangentZ)) * Weight;
			Tangent += Matrix.TransformVector(SoftVertex.TangentX) * Weight;
			TotalWeight += Weight;
			if (Weight > MainWeight)
			{
				MainWeight = Weight;
				Skinned.MainBone = BoneIndex;
			}
		}
		if (TotalWeight <= 0.f)
		{
			return Skinned;
		}
		Skinned.Position = Position / TotalWeight;
		Skinned.Normal = Normal.GetSafeNormal(UE_SMALL_NUMBER, FVector3f::UpVector);
		Skinned.Tangent = Tangent.GetSafeNormal(UE_SMALL_NUMBER, FVector3f::ForwardVector);
		Skinned.UV = SoftVertex.UVs[0];
		Skinned.BinormalSign = SoftVertex.TangentZ.W < 0.f ? -1.f : 1.f;
		return Skinned;
	}

	TArray<FSkinnedVertex> SkinLod(const FSkeletalMeshLODModel& Lod, const TArray<FMatrix44f>& RefToPivot)
	{
		TArray<FSkinnedVertex> Vertices;
		Vertices.SetNum(Lod.NumVertices);
		for (const FSkelMeshSection& Section : Lod.Sections)
		{
			for (int32 LocalIndex = 0; LocalIndex < Section.SoftVertices.Num(); ++LocalIndex)
			{
				const int32 VertexIndex = Section.BaseVertexIndex + LocalIndex;
				if (Vertices.IsValidIndex(VertexIndex))
				{
					Vertices[VertexIndex] = SkinVertex(Section.SoftVertices[LocalIndex], Section, RefToPivot);
				}
			}
		}
		return Vertices;
	}

	/**
	 * A triangle with a corner on a bone no physics body carries (the root, IK helper chains) stretches between the
	 * body and where that bone stayed when the body fell (a thin line on the Mosin soldier); a corpse leaves it out.
	 */
	bool GetHasDanglingCorner(const FSkeletalMeshLODModel& Lod, const TArray<FSkinnedVertex>& Vertices,
	                          const uint32 FirstIndex)
	{
		for (uint32 Corner = 0; Corner < 3; ++Corner)
		{
			if (not Vertices[Lod.IndexBuffer[FirstIndex + Corner]].bIsCarried)
			{
				return true;
			}
		}
		return false;
	}

	FName GetSlotName(const USkeletalMesh* SkeletalMesh, const int32 MaterialIndex)
	{
		const TArray<FSkeletalMaterial>& Materials = SkeletalMesh->GetMaterials();
		if (Materials.IsValidIndex(MaterialIndex) && not Materials[MaterialIndex].MaterialSlotName.IsNone())
		{
			return Materials[MaterialIndex].MaterialSlotName;
		}
		return FName(*FString::Printf(TEXT("Slot_%d"), MaterialIndex));
	}

	/**
	 * Builds one polygon group per used material, in material order, and returns those material indices so the
	 * static mesh sections can be mapped to them.
	 */
	TArray<int32> BuildMeshDescription(const USkeletalMesh* SkeletalMesh, const FSkeletalMeshLODModel& Lod,
	                                   const TArray<FSkinnedVertex>& Vertices, FMeshDescription& OutDescription,
	                                   int32& OutTriangles)
	{
		FStaticMeshAttributes Attributes(OutDescription);
		Attributes.Register();
		TVertexAttributesRef<FVector3f> Positions = Attributes.GetVertexPositions();
		TVertexInstanceAttributesRef<FVector3f> Normals = Attributes.GetVertexInstanceNormals();
		TVertexInstanceAttributesRef<FVector3f> Tangents = Attributes.GetVertexInstanceTangents();
		TVertexInstanceAttributesRef<float> BinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
		TVertexInstanceAttributesRef<FVector2f> UVs = Attributes.GetVertexInstanceUVs();
		UVs.SetNumChannels(1);
		TPolygonGroupAttributesRef<FName> SlotNames = Attributes.GetPolygonGroupMaterialSlotNames();

		TArray<FVertexID> VertexIds;
		VertexIds.Reserve(Vertices.Num());
		for (const FSkinnedVertex& Vertex : Vertices)
		{
			const FVertexID VertexId = OutDescription.CreateVertex();
			Positions[VertexId] = Vertex.Position;
			VertexIds.Add(VertexId);
		}

		TArray<int32> UsedMaterials;
		for (const FSkelMeshSection& Section : Lod.Sections)
		{
			if (not Section.bDisabled)
			{
				UsedMaterials.AddUnique(Section.MaterialIndex);
			}
		}
		UsedMaterials.Sort();
		OutTriangles = 0;
		for (const int32 MaterialIndex : UsedMaterials)
		{
			const FPolygonGroupID GroupId = OutDescription.CreatePolygonGroup();
			SlotNames[GroupId] = GetSlotName(SkeletalMesh, MaterialIndex);
			for (const FSkelMeshSection& Section : Lod.Sections)
			{
				if (Section.bDisabled || Section.MaterialIndex != MaterialIndex)
				{
					continue;
				}
				for (uint32 Triangle = 0; Triangle < Section.NumTriangles; ++Triangle)
				{
					if (GetHasDanglingCorner(Lod, Vertices, Section.BaseIndex + Triangle * 3))
					{
						continue;
					}
					TArray<FVertexInstanceID, TFixedAllocator<3>> Corners;
					for (int32 Corner = 0; Corner < 3; ++Corner)
					{
						const uint32 VertexIndex = Lod.IndexBuffer[Section.BaseIndex + Triangle * 3 + Corner];
						const FSkinnedVertex& Vertex = Vertices[VertexIndex];
						const FVertexInstanceID InstanceId = OutDescription.CreateVertexInstance(VertexIds[VertexIndex]);
						Normals[InstanceId] = Vertex.Normal;
						Tangents[InstanceId] = Vertex.Tangent;
						BinormalSigns[InstanceId] = Vertex.BinormalSign;
						UVs.Set(InstanceId, 0, Vertex.UV);
						Corners.Add(InstanceId);
					}
					OutDescription.CreateTriangle(GroupId, Corners);
					++OutTriangles;
				}
			}
		}
		return UsedMaterials;
	}

	void SetMaterials(UStaticMesh* StaticMesh, const USkeletalMeshComponent* CrewMesh,
	                  const TArray<int32>& UsedMaterials)
	{
		// Every slot of the soldier mesh, in its order, so a soldier's materials apply to the corpse slot for slot.
		const USkeletalMesh* SkeletalMesh = CrewMesh->GetSkeletalMeshAsset();
		TArray<FStaticMaterial> StaticMaterials;
		for (int32 MaterialIndex = 0; MaterialIndex < SkeletalMesh->GetMaterials().Num(); ++MaterialIndex)
		{
			const FName SlotName = GetSlotName(SkeletalMesh, MaterialIndex);
			StaticMaterials.Add(FStaticMaterial(CrewMesh->GetMaterial(MaterialIndex), SlotName, SlotName));
		}
		for (int32 SectionIndex = 0; SectionIndex < UsedMaterials.Num(); ++SectionIndex)
		{
			StaticMesh->GetSectionInfoMap().Set(BakedLodIndex, SectionIndex, FMeshSectionInfo(UsedMaterials[SectionIndex]));
		}
		StaticMesh->SetStaticMaterials(StaticMaterials);
	}

}

UPackage* CorpsePoseBaker::CreateAssetPackage(const FString& PackagePath)
{
	UPackage* Package = CreatePackage(*PackagePath);
	// The game may have loaded the previous bake (the corpse library references its meshes); move it aside so the
	// new asset can take its name.
	const FString AssetName = FPackageName::GetLongPackageAssetName(PackagePath);
	if (UObject* Previous = StaticFindObjectFast(UObject::StaticClass(), Package, FName(*AssetName)))
	{
		Previous->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
	}
	// A rebake replaces the previous asset; a new package over an existing file would count as partly loaded.
	Package->MarkAsFullyLoaded();
	return Package;
}

bool CorpsePoseBaker::SaveAsset(UPackage* Package, UObject* Asset)
{
	const FString Filename = FPackageName::LongPackageNameToFilename(
		Package->GetName(), FPackageName::GetAssetPackageExtension());
	// Outside the editor the package metadata is otherwise created mid-save, which asserts.
	Package->GetMetaData();
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	// An open editor rereads a changed asset file for a moment; a save that lands then is refused, so retry.
	SaveArgs.SaveFlags = SAVE_NoError;
	constexpr int32 SaveAttempts = 10;
	constexpr float SecondsBetweenAttempts = 1.f;
	for (int32 Attempt = 0; Attempt < SaveAttempts; ++Attempt)
	{
		if (UPackage::SavePackage(Package, Asset, *Filename, SaveArgs))
		{
			return true;
		}
		FPlatformProcess::Sleep(SecondsBetweenAttempts);
	}
	return false;
}

FCorpsePoseBakeResult CorpsePoseBaker::BakeAndSave(const USkeletalMeshComponent* CrewMesh, const FVector& Pivot,
                                                   const FString& PackagePath)
{
	using namespace CorpsePoseBakerPrivate;
	FCorpsePoseBakeResult Result;
	const USkeletalMesh* SkeletalMesh = CrewMesh ? CrewMesh->GetSkeletalMeshAsset() : nullptr;
	const FSkeletalMeshModel* ImportedModel = SkeletalMesh ? SkeletalMesh->GetImportedModel() : nullptr;
	if (ImportedModel == nullptr || not ImportedModel->LODModels.IsValidIndex(BakedLodIndex))
	{
		return Result;
	}
	const FSkeletalMeshLODModel& Lod = ImportedModel->LODModels[BakedLodIndex];
	TArray<FSkinnedVertex> Vertices = SkinLod(Lod, GetRefToPivot(CrewMesh, Pivot));
	const TArray<bool> CarriedBones = GetCarriedBones(SkeletalMesh);
	for (FSkinnedVertex& Vertex : Vertices)
	{
		Vertex.bIsCarried = CarriedBones.IsValidIndex(Vertex.MainBone) && CarriedBones[Vertex.MainBone];
	}
	Result.LowestZ = TNumericLimits<float>::Max();
	for (const FSkinnedVertex& Vertex : Vertices)
	{
		// Dangling triangles are left out of the corpse, so their vertices do not count either.
		if (Vertex.bIsCarried && Vertex.Position.Z < Result.LowestZ)
		{
			Result.LowestZ = Vertex.Position.Z;
			Result.LowestBone = SkeletalMesh->GetRefSkeleton().IsValidIndex(Vertex.MainBone)
				                    ? SkeletalMesh->GetRefSkeleton().GetBoneName(Vertex.MainBone)
				                    : NAME_None;
		}
	}

	FMeshDescription MeshDescription;
	const TArray<int32> UsedMaterials = BuildMeshDescription(SkeletalMesh, Lod, Vertices, MeshDescription,
	                                                         Result.Triangles);
	UPackage* Package = CreateAssetPackage(PackagePath);
	const FName AssetName(*FPackageName::GetLongPackageAssetName(PackagePath));
	UStaticMesh* StaticMesh = NewObject<UStaticMesh>(Package, AssetName, RF_Public | RF_Standalone);
	FStaticMeshSourceModel& SourceModel = StaticMesh->AddSourceModel();
	SourceModel.BuildSettings.bRecomputeNormals = false;
	SourceModel.BuildSettings.bRecomputeTangents = false;
	SourceModel.BuildSettings.bGenerateLightmapUVs = false;
	if (Result.Triangles > MaxCorpseTriangles)
	{
		SourceModel.ReductionSettings.PercentTriangles = static_cast<float>(MaxCorpseTriangles) / Result.Triangles;
	}
	SetMaterials(StaticMesh, CrewMesh, UsedMaterials);
	StaticMesh->CreateMeshDescription(BakedLodIndex, MoveTemp(MeshDescription));
	StaticMesh->CommitMeshDescription(BakedLodIndex);
	StaticMesh->Build(true);
	FStaticMeshCompilingManager::Get().FinishCompilation({StaticMesh});
	StaticMesh->MarkPackageDirty();
	if (StaticMesh->GetRenderData() != nullptr && StaticMesh->GetRenderData()->LODResources.IsValidIndex(BakedLodIndex))
	{
		Result.Triangles = StaticMesh->GetRenderData()->LODResources[BakedLodIndex].GetNumTriangles();
	}
	Result.Mesh = StaticMesh;
	Result.bSaved = SaveAsset(Package, StaticMesh);
	return Result;
}

#endif
