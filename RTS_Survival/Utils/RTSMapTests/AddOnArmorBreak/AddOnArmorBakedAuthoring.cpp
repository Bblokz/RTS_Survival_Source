// Copyright (C) Bas Blokzijl - All rights reserved.

// Editor-only authoring for the baked-skirt technique of the add-on armour break test (test content, never shipped).
// RTS.AddOnArmorTest.BuildBakedHull: SM_TestPzIVHull_Skirted, the Panzer IV hull with both skirt sides (80 shards)
//   and both rails merged into one static mesh. The skirt geometry uses its own material slot "Skirt" and carries its
//   shard id (= the test vehicle's piece index) in UV channel 2; the rails carry RailShardId and never hide.
// RTS.AddOnArmorTest.BuildBakedWpoMaterial: M_TestSkirtSteelBakedWpo, a copy of M_TestSkirtSteelBakedMask (made by
//   Tools/AddOnArmorBreak/ue_make_baked_materials.py) that is opaque and folds broken shards onto the object origin
//   through world position offset instead of clipping them. Python cannot wire that pin.

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "AddOnArmorBreakAuthoringShared.h"
#include "AddOnArmorBreakTestTypes.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "HAL/IConsoleManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionObjectPositionWS.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSAddOnArmorBakedAuthoring, Log, All);

namespace AddOnArmorBakedAuthoring
{
	using namespace AddOnArmorBreakTest;
	using namespace AddOnArmorBreakAuthoringShared;

	/**
	 * @brief Copies a skirt mesh's description and writes the shard id into UV channel ShardIdUvChannel.
	 * @param Mesh Shard or rail mesh.
	 * @param ShardId Id the skirt material reads to decide whether the shard is broken.
	 * @param OutDescription Receives the tagged copy.
	 * @return False when the mesh has no source description.
	 */
	bool MakeTaggedDescription(const UStaticMesh& Mesh, const float ShardId, FMeshDescription& OutDescription)
	{
		const FMeshDescription* const Source = Mesh.GetMeshDescription(0);
		if (Source == nullptr)
		{
			return false;
		}
		OutDescription = *Source;
		FStaticMeshAttributes Attributes(OutDescription);
		TVertexInstanceAttributesRef<FVector2f> Uvs = Attributes.GetVertexInstanceUVs();
		Uvs.SetNumChannels(ShardIdUvChannel + 1);
		for (const FVertexInstanceID VertexInstance : OutDescription.VertexInstances().GetElementIDs())
		{
			Uvs.Set(VertexInstance, ShardIdUvChannel, FVector2f(ShardId, 0.f));
		}
		return true;
	}

	void AppendIntoSkirtSlot(const FMeshDescription& Source, const FTransform& Transform, const FPolygonGroupID SkirtGroup,
	                         FMeshDescription& Target)
	{
		FStaticMeshOperations::FAppendSettings Settings;
		// Only channel 0 merges by default; the shard id lives in channel 2.
		for (int32 Channel = 0; Channel < FStaticMeshOperations::FAppendSettings::MAX_NUM_UV_CHANNELS; ++Channel)
		{
			Settings.bMergeUVChannels[Channel] = true;
		}
		Settings.MeshTransform = Transform;
		Settings.PolygonGroupsDelegate = FAppendPolygonGroupsDelegate::CreateLambda(
			[SkirtGroup](const FMeshDescription& SourceMesh, FMeshDescription&, PolygonGroupMap& RemapPolygonGroup)
			{
				for (const FPolygonGroupID SourceGroup : SourceMesh.PolygonGroups().GetElementIDs())
				{
					RemapPolygonGroup.Add(SourceGroup, SkirtGroup);
				}
			});
		FStaticMeshOperations::AppendMeshDescription(Source, Target, Settings);
	}

	/** @return The number of shards and rails merged in; the vehicle's piece order (side, panel, shard) is the id. */
	int32 AppendSkirts(const TArray<UStaticMesh*>& ShardMeshes, const TArray<FVector>& Centroids, const UStaticMesh& RailMesh,
	                   const FPolygonGroupID SkirtGroup, FMeshDescription& Target)
	{
		int32 AppendedCount = 0;
		for (int32 SideIndex = 0; SideIndex < SideCount; ++SideIndex)
		{
			const FTransform SideTransform = GetSideTransform(SideIndex);
			for (int32 PanelIndex = 0; PanelIndex < PanelsPerSide; ++PanelIndex)
			{
				for (int32 ShardIndex = 0; ShardIndex < ShardCount; ++ShardIndex)
				{
					const int32 ShardId = (SideIndex * PanelsPerSide + PanelIndex) * ShardCount + ShardIndex;
					FMeshDescription Shard;
					if (not MakeTaggedDescription(*ShardMeshes[ShardIndex], static_cast<float>(ShardId), Shard))
					{
						continue;
					}
					const FTransform PieceTransform = FTransform(GetPanelTopCentre(PanelIndex) + Centroids[ShardIndex]) * SideTransform;
					AppendIntoSkirtSlot(Shard, PieceTransform, SkirtGroup, Target);
					++AppendedCount;
				}
			}
			FMeshDescription Rail;
			if (MakeTaggedDescription(RailMesh, RailShardId, Rail))
			{
				AppendIntoSkirtSlot(Rail, FTransform(FVector(0.f, -RailInsetY, 0.f)) * SideTransform, SkirtGroup, Target);
				++AppendedCount;
			}
		}
		return AppendedCount;
	}

	void CopySockets(const UStaticMesh& From, UStaticMesh& To)
	{
		for (const UStaticMeshSocket* const Socket : From.Sockets)
		{
			if (IsValid(Socket))
			{
				To.AddSocket(DuplicateObject<UStaticMeshSocket>(Socket, &To));
			}
		}
	}

	void BuildBakedHull()
	{
		const UStaticMesh* const Hull = LoadObject<UStaticMesh>(nullptr, SourceHullMeshPath);
		UStaticMesh* const RailMesh = LoadObject<UStaticMesh>(nullptr, RailMeshPath);
		UMaterialInterface* const SkirtMaterial = LoadObject<UMaterialInterface>(nullptr, OpaqueSkirtMaterialPath);
		TArray<FVector> Centroids;
		TArray<UStaticMesh*> ShardMeshes;
		const FMeshDescription* const HullDescription = IsValid(Hull) ? Hull->GetMeshDescription(0) : nullptr;
		if (HullDescription == nullptr || not IsValid(RailMesh) || not IsValid(SkirtMaterial)
			|| not LoadShardCentroids(Centroids) || not LoadShardMeshes(ShardMeshes))
		{
			UE_LOG(LogRTSAddOnArmorBakedAuthoring, Error, TEXT("RTS_ADDON_ARMOR_AUTHORING baked hull RESULT FAIL inputs"));
			return;
		}
		FMeshDescription Merged = *HullDescription;
		FStaticMeshAttributes Attributes(Merged);
		Attributes.GetVertexInstanceUVs().SetNumChannels(ShardIdUvChannel + 1);
		const FPolygonGroupID SkirtGroup = Merged.CreatePolygonGroup();
		Attributes.GetPolygonGroupMaterialSlotNames()[SkirtGroup] = SkirtMaterialSlot;
		const int32 AppendedCount = AppendSkirts(ShardMeshes, Centroids, *RailMesh, SkirtGroup, Merged);

		UPackage* const Package = PrepareFreshPackage(BakedHullMeshPackage);
		if (Package == nullptr)
		{
			UE_LOG(LogRTSAddOnArmorBakedAuthoring, Error, TEXT("RTS_ADDON_ARMOR_AUTHORING baked hull RESULT FAIL package"));
			return;
		}
		UStaticMesh* const Baked = NewObject<UStaticMesh>(Package, *FPackageName::GetShortName(BakedHullMeshPackage),
		                                                  RF_Public | RF_Standalone | RF_Transactional);
		TArray<FStaticMaterial> Materials = Hull->GetStaticMaterials();
		Materials.Add(FStaticMaterial(SkirtMaterial, SkirtMaterialSlot, SkirtMaterialSlot));
		Baked->SetStaticMaterials(Materials);
		Baked->SetNumSourceModels(1);
		// The hull's own build settings: its source description relies on them (it recomputes its normals; without
		// that, parts of the hull below the skirts rendered as bright triangles).
		const FMeshBuildSettings& HullBuildSettings = Hull->GetSourceModel(0).BuildSettings;
		Baked->GetSourceModel(0).BuildSettings = HullBuildSettings;
		UE_LOG(LogRTSAddOnArmorBakedAuthoring, Display, TEXT("RTS_ADDON_ARMOR_AUTHORING hull build recompute_normals=%d recompute_tangents=%d lightmap_uvs=%d"),
		       HullBuildSettings.bRecomputeNormals ? 1 : 0, HullBuildSettings.bRecomputeTangents ? 1 : 0,
		       HullBuildSettings.bGenerateLightmapUVs ? 1 : 0);
		Baked->NaniteSettings.bEnabled = false;
		Baked->CreateMeshDescription(0, MoveTemp(Merged));
		Baked->CommitMeshDescription(0);
		CopySockets(*Hull, *Baked);
		Baked->Build(false);
		Baked->PostEditChange();
		FAssetRegistryModule::AssetCreated(Baked);
		const bool bSaved = SaveAsset(*Baked);
		UE_LOG(LogRTSAddOnArmorBakedAuthoring, Display,
		       TEXT("RTS_ADDON_ARMOR_AUTHORING baked hull RESULT DONE appended=%d hull_tris=%d baked_tris=%d slots=%d sockets=%d saved=%d"),
		       AppendedCount, Hull->GetNumTriangles(0), Baked->GetNumTriangles(0), Baked->GetStaticMaterials().Num(),
		       Baked->Sockets.Num(), bSaved ? 1 : 0);
	}

	template <typename TExpression>
	TExpression* AddExpression(UMaterial& Material, const int32 PositionX, const int32 PositionY)
	{
		TExpression* const Expression = NewObject<TExpression>(&Material, NAME_None, RF_Transactional);
		Expression->MaterialExpressionEditorX = PositionX;
		Expression->MaterialExpressionEditorY = PositionY;
		Material.GetExpressionCollection().AddExpression(Expression);
		return Expression;
	}

	UMaterialExpressionCustom* FindVisibilityNode(UMaterial& Material)
	{
		for (UMaterialExpression* const Expression : Material.GetExpressions())
		{
			if (UMaterialExpressionCustom* const Custom = Cast<UMaterialExpressionCustom>(Expression))
			{
				return Custom;
			}
		}
		return nullptr;
	}

	void BuildBakedWpoMaterial()
	{
		constexpr int32 FoldNodeX = -150;
		UMaterial* const MaskMaterial = LoadObject<UMaterial>(nullptr, MaskSkirtMaterialPath);
		UPackage* const Package = IsValid(MaskMaterial) ? PrepareFreshPackage(WpoSkirtMaterialPackage) : nullptr;
		if (Package == nullptr)
		{
			UE_LOG(LogRTSAddOnArmorBakedAuthoring, Error, TEXT("RTS_ADDON_ARMOR_AUTHORING wpo material RESULT FAIL inputs"));
			return;
		}
		UMaterial* const Material = DuplicateObject<UMaterial>(MaskMaterial, Package, *FPackageName::GetShortName(WpoSkirtMaterialPackage));
		Material->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
		UMaterialExpressionCustom* const Visible = FindVisibilityNode(*Material);
		UMaterialEditorOnlyData* const EditorData = Material->GetEditorOnlyData();
		if (Visible == nullptr || EditorData == nullptr)
		{
			UE_LOG(LogRTSAddOnArmorBakedAuthoring, Error, TEXT("RTS_ADDON_ARMOR_AUTHORING wpo material RESULT FAIL no visibility node"));
			return;
		}
		Material->BlendMode = BLEND_Opaque;
		EditorData->OpacityMask.Expression = nullptr;
		UMaterialExpressionObjectPositionWS* const ObjectPosition = AddExpression<UMaterialExpressionObjectPositionWS>(*Material, -500, 700);
		UMaterialExpressionWorldPosition* const WorldPosition = AddExpression<UMaterialExpressionWorldPosition>(*Material, -500, 800);
		UMaterialExpressionSubtract* const ToOrigin = AddExpression<UMaterialExpressionSubtract>(*Material, -300, 750);
		ToOrigin->A.Expression = ObjectPosition;
		ToOrigin->B.Expression = WorldPosition;
		UMaterialExpressionOneMinus* const Broken = AddExpression<UMaterialExpressionOneMinus>(*Material, -300, 450);
		Broken->Input.Expression = Visible;
		UMaterialExpressionMultiply* const Fold = AddExpression<UMaterialExpressionMultiply>(*Material, FoldNodeX, 600);
		Fold->A.Expression = ToOrigin;
		Fold->B.Expression = Broken;
		EditorData->WorldPositionOffset.Expression = Fold;
		Material->PreEditChange(nullptr);
		Material->PostEditChange();
		FAssetRegistryModule::AssetCreated(Material);
		const bool bSaved = SaveAsset(*Material);
		UE_LOG(LogRTSAddOnArmorBakedAuthoring, Display, TEXT("RTS_ADDON_ARMOR_AUTHORING wpo material RESULT DONE saved=%d"), bSaved ? 1 : 0);
	}

	FAutoConsoleCommand BuildBakedHullCommand(
		TEXT("RTS.AddOnArmorTest.BuildBakedHull"),
		TEXT("Builds the add-on armour break test's Panzer IV hull with the skirts merged in (test content) and saves it."),
		FConsoleCommandDelegate::CreateStatic(&BuildBakedHull));

	FAutoConsoleCommand BuildBakedWpoMaterialCommand(
		TEXT("RTS.AddOnArmorTest.BuildBakedWpoMaterial"),
		TEXT("Builds the world-position-offset skirt material of the add-on armour break test (test content) and saves it."),
		FConsoleCommandDelegate::CreateStatic(&BuildBakedWpoMaterial));
}

#endif // WITH_EDITOR
