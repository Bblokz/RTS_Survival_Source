// Copyright (C) Bas Blokzijl - All rights reserved.

// Editor-only diagnostics for add-on armour panel authoring (see AddOnArmorPanelAuthoring.cpp).
// RTS.AddOnArmor.SurveyPanels [Path=] Out=<file>: thin outer side plates of every hull mesh under a path (candidates).
// RTS.AddOnArmor.DumpMeshTriangles <mesh> <file>: LOD0 triangles with their island index, for plotting outside Unreal.
// RTS.AddOnArmor.DumpMaterialHlsl <material> <file>: the translated HLSL; run with -RenderOffscreen, not -nullrhi.

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "AddOnArmorPanelAuthoringShared.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "HAL/IConsoleManager.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Misc/FileHelper.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSAddOnArmorPanelDiagnostics, Log, All);

namespace AddOnArmorPanelDiagnostics
{
	using namespace AddOnArmorPanelAuthoring;

	void DumpMaterialHlsl(const TArray<FString>& Args)
	{
		UMaterialInterface* const Material = Args.Num() >= 2 ? LoadObject<UMaterialInterface>(nullptr, *Args[0]) : nullptr;
		FMaterialResource* const Resource = IsValid(Material) ? Material->GetMaterialResource(GMaxRHIFeatureLevel) : nullptr;
		if (const UMaterial* const BaseMaterial = IsValid(Material) ? Material->GetMaterial() : nullptr)
		{
			const UMaterialExpression* const MaskExpression = BaseMaterial->GetEditorOnlyData()->OpacityMask.Expression;
			UE_LOG(LogRTSAddOnArmorPanelDiagnostics, Display,
			       TEXT("RTS_ADDON_PANEL_AUTHORING hlsl state raw_blend=%d blend=%d assumed_opaque=%d mask_active=%d mask_input=%s mask_constant=%d"),
			       static_cast<int32>(BaseMaterial->BlendMode.GetValue()), static_cast<int32>(Material->GetBlendMode()),
			       BaseMaterial->bCanMaskedBeAssumedOpaque ? 1 : 0, Material->IsPropertyActive(MP_OpacityMask) ? 1 : 0,
			       *GetNameSafe(MaskExpression), BaseMaterial->GetEditorOnlyData()->OpacityMask.UseConstant ? 1 : 0);
		}
		FString Source;
		const bool bTranslated = Resource != nullptr && Resource->GetMaterialExpressionSource(Source);
		const bool bSaved = bTranslated && FFileHelper::SaveStringToFile(Source, *Args[1]);
		UE_LOG(LogRTSAddOnArmorPanelDiagnostics, Display, TEXT("RTS_ADDON_PANEL_AUTHORING hlsl %s translated=%d saved=%d"),
		       Args.Num() > 0 ? *Args[0] : TEXT("none"), bTranslated ? 1 : 0, bSaved ? 1 : 0);
	}

	namespace Survey
	{
		// Panel-like islands: thin across Y, of plate size, and near the mesh's outermost |Y|.
		constexpr double MaxPlateThickness = 6.0;
		constexpr double MinPlateLength = 20.0;
		constexpr double MinPlateHeight = 15.0;
		constexpr double OuterBand = 30.0;
		const TCHAR* const ExcludedNameParts[] = {
			TEXT("Destroyed"), TEXT("Collision"), TEXT("TrainingPreview"), TEXT("AddOnPanels"), TEXT("Wreck")};

		bool GetIsSurveyedMesh(const FString& AssetName)
		{
			if (not AssetName.Contains(TEXT("Hull")))
			{
				return false;
			}
			for (const TCHAR* const Excluded : ExcludedNameParts)
			{
				if (AssetName.Contains(Excluded))
				{
					return false;
				}
			}
			return true;
		}

		FString DescribeMesh(UStaticMesh& Mesh)
		{
			const FMeshDescription* const Description = Mesh.GetMeshDescription(0);
			if (Description == nullptr)
			{
				return FString::Printf(TEXT("MESH %s no-description\n"), *Mesh.GetPathName());
			}
			const TArray<FMeshIsland> Islands = FindIslands(*Description);
			double MaxAbsY = 0.0;
			for (const FMeshIsland& Island : Islands)
			{
				MaxAbsY = FMath::Max(MaxAbsY, FMath::Max(FMath::Abs(Island.Bounds.Min.Y), FMath::Abs(Island.Bounds.Max.Y)));
			}
			int32 DescribedLods = 0;
			for (int32 LodIndex = 0; LodIndex < Mesh.GetNumSourceModels(); ++LodIndex)
			{
				DescribedLods += Mesh.GetMeshDescription(LodIndex) != nullptr ? 1 : 0;
			}
			FString Lines;
			int32 LeftCount = 0;
			int32 RightCount = 0;
			for (const FMeshIsland& Island : Islands)
			{
				const FVector Size = Island.Bounds.GetSize();
				const double InnerAbsY = FMath::Min(FMath::Abs(Island.Bounds.Min.Y), FMath::Abs(Island.Bounds.Max.Y));
				const bool bOneSide = Island.Bounds.Min.Y > 0.0 || Island.Bounds.Max.Y < 0.0;
				if (not bOneSide || Size.Y > MaxPlateThickness || Size.X < MinPlateLength || Size.Z < MinPlateHeight
					|| InnerAbsY < MaxAbsY - OuterBand)
				{
					continue;
				}
				(Island.Bounds.Max.Y < 0.0 ? LeftCount : RightCount)++;
				Lines += FString::Printf(TEXT("  plate side=%s x=[%.1f,%.1f] z=[%.1f,%.1f] absy=[%.1f,%.1f] area=%.0f polys=%d\n"),
				                         Island.Bounds.Max.Y < 0.0 ? TEXT("L") : TEXT("R"), Island.Bounds.Min.X,
				                         Island.Bounds.Max.X, Island.Bounds.Min.Z, Island.Bounds.Max.Z, InnerAbsY,
				                         InnerAbsY + Size.Y, Island.Area, Island.Polygons.Num());
			}
			return FString::Printf(TEXT("MESH %s maxabsy=%.1f islands=%d lods=%d described_lods=%d slots=%d plates L=%d R=%d\n"),
			                       *Mesh.GetPathName(), MaxAbsY, Islands.Num(), Mesh.GetNumSourceModels(), DescribedLods,
			                       Mesh.GetStaticMaterials().Num(), LeftCount, RightCount) + Lines;
		}

		/** @brief Writes "island x y z x y z x y z" per triangle of LOD0, for plotting the mesh outside the editor. */
		void DumpMeshTriangles(const TArray<FString>& Args)
		{
			if (Args.Num() < 2)
			{
				return;
			}
			const UStaticMesh* const Mesh = LoadObject<UStaticMesh>(nullptr, *Args[0]);
			const FMeshDescription* const Description = IsValid(Mesh) ? Mesh->GetMeshDescription(0) : nullptr;
			if (Description == nullptr)
			{
				UE_LOG(LogRTSAddOnArmorPanelDiagnostics, Error, TEXT("RTS_ADDON_PANEL_AUTHORING triangles %s FAIL"), *Args[0]);
				return;
			}
			const FStaticMeshConstAttributes Attributes(*Description);
			const TVertexAttributesConstRef<FVector3f> Positions = Attributes.GetVertexPositions();
			const TArray<FMeshIsland> Islands = FindIslands(*Description);
			TArray<FString> Lines;
			for (int32 IslandIndex = 0; IslandIndex < Islands.Num(); ++IslandIndex)
			{
				for (const FPolygonID PolygonID : Islands[IslandIndex].Polygons)
				{
					for (const FTriangleID TriangleID : Description->GetPolygonTriangles(PolygonID))
					{
						FString Line = FString::FromInt(IslandIndex);
						for (const FVertexID Corner : Description->GetTriangleVertices(TriangleID))
						{
							Line += FString::Printf(TEXT(" %.1f %.1f %.1f"), Positions[Corner].X, Positions[Corner].Y,
							                        Positions[Corner].Z);
						}
						Lines.Add(Line);
					}
				}
			}
			const bool bSaved = FFileHelper::SaveStringArrayToFile(Lines, *Args[1]);
			UE_LOG(LogRTSAddOnArmorPanelDiagnostics, Display, TEXT("RTS_ADDON_PANEL_AUTHORING triangles %s saved=%d"),
			       *Args[0], bSaved ? 1 : 0);
		}

		void SurveyPanels(const TArray<FString>& Args)
		{
			const FString Joined = FString::Join(Args, TEXT(" "));
			FString Path = TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles");
			FString OutFile;
			FParse::Value(*Joined, TEXT("Path="), Path);
			FParse::Value(*Joined, TEXT("Out="), OutFile);
			const IAssetRegistry& AssetRegistry = FAssetRegistryModule::GetRegistry();
			TArray<FAssetData> Assets;
			FARFilter Filter;
			Filter.PackagePaths.Add(FName(*Path));
			Filter.bRecursivePaths = true;
			Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
			AssetRegistry.GetAssets(Filter, Assets);
			FString Report;
			int32 Surveyed = 0;
			for (const FAssetData& Asset : Assets)
			{
				if (not GetIsSurveyedMesh(Asset.AssetName.ToString()))
				{
					continue;
				}
				if (UStaticMesh* const Mesh = Cast<UStaticMesh>(Asset.GetAsset()))
				{
					Report += DescribeMesh(*Mesh);
					++Surveyed;
				}
			}
			const bool bSaved = not OutFile.IsEmpty() && FFileHelper::SaveStringToFile(Report, *OutFile);
			UE_LOG(LogRTSAddOnArmorPanelDiagnostics, Display, TEXT("RTS_ADDON_PANEL_AUTHORING survey meshes=%d saved=%d"),
			       Surveyed, bSaved ? 1 : 0);
		}
	}

	FAutoConsoleCommand DumpMeshTrianglesCommand(
		TEXT("RTS.AddOnArmor.DumpMeshTriangles"),
		TEXT("Writes a mesh's LOD0 triangles with their island index, for plotting. Args: <mesh path> <output file>"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Survey::DumpMeshTriangles));

	FAutoConsoleCommand SurveyPanelsCommand(
		TEXT("RTS.AddOnArmor.SurveyPanels"),
		TEXT("Lists thin outer side plates (add-on armour candidates) of every hull mesh under a path. Args: [Path=] Out=<file>"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Survey::SurveyPanels));

	FAutoConsoleCommand DumpMaterialHlslCommand(
		TEXT("RTS.AddOnArmor.DumpMaterialHlsl"),
		TEXT("Writes the HLSL the material translator generates for a material. Args: <material path> <output file>"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&DumpMaterialHlsl));

}

#endif // WITH_EDITOR
