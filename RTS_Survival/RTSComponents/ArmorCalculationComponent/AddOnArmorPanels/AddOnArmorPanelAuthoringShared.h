// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

// Helpers shared by the editor-only add-on armour panel authoring and its diagnostics.

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "Misc/PackageName.h"
#include "StaticMeshAttributes.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace AddOnArmorPanelAuthoring
{
	struct FMeshIsland
	{
		TArray<FPolygonID> Polygons;
		FBox Bounds = FBox(ForceInit);
		double Area = 0.0;
	};

	inline int32 FindRoot(TArray<int32>& Parents, int32 Element)
	{
		while (Parents[Element] != Element)
		{
			Parents[Element] = Parents[Parents[Element]];
			Element = Parents[Element];
		}
		return Element;
	}

	inline double GetPolygonArea(const FMeshDescription& Description, const FPolygonID PolygonID,
	                             const TVertexAttributesConstRef<FVector3f>& Positions)
	{
		double Area = 0.0;
		for (const FTriangleID TriangleID : Description.GetPolygonTriangles(PolygonID))
		{
			const TArrayView<const FVertexID> Corners = Description.GetTriangleVertices(TriangleID);
			const FVector A(Positions[Corners[0]]);
			Area += 0.5 * FVector::CrossProduct(FVector(Positions[Corners[1]]) - A, FVector(Positions[Corners[2]]) - A).Size();
		}
		return Area;
	}

	/** @return The polygons of the mesh grouped by shared vertices. */
	inline TArray<FMeshIsland> FindIslands(const FMeshDescription& Description)
	{
		TArray<int32> Parents;
		Parents.SetNumUninitialized(Description.Vertices().GetArraySize());
		for (int32 VertexIndex = 0; VertexIndex < Parents.Num(); ++VertexIndex)
		{
			Parents[VertexIndex] = VertexIndex;
		}
		for (const FPolygonID PolygonID : Description.Polygons().GetElementIDs())
		{
			const TArray<FVertexID> Corners = Description.GetPolygonVertices(PolygonID);
			for (int32 CornerIndex = 1; CornerIndex < Corners.Num(); ++CornerIndex)
			{
				Parents[FindRoot(Parents, Corners[CornerIndex].GetValue())] = FindRoot(Parents, Corners[0].GetValue());
			}
		}

		const FStaticMeshConstAttributes Attributes(Description);
		const TVertexAttributesConstRef<FVector3f> Positions = Attributes.GetVertexPositions();
		TMap<int32, int32> IslandByRoot;
		TArray<FMeshIsland> Islands;
		for (const FPolygonID PolygonID : Description.Polygons().GetElementIDs())
		{
			const TArray<FVertexID> Corners = Description.GetPolygonVertices(PolygonID);
			const int32 Root = FindRoot(Parents, Corners[0].GetValue());
			const int32* const ExistingIsland = IslandByRoot.Find(Root);
			const int32 IslandIndex = ExistingIsland != nullptr ? *ExistingIsland : Islands.AddDefaulted();
			IslandByRoot.Add(Root, IslandIndex);
			FMeshIsland& Island = Islands[IslandIndex];
			Island.Polygons.Add(PolygonID);
			Island.Area += GetPolygonArea(Description, PolygonID, Positions);
			for (const FVertexID Corner : Corners)
			{
				Island.Bounds += FVector(Positions[Corner]);
			}
		}
		return Islands;
	}

	/** @return A package for the asset; an existing asset of that name is moved out to the transient package. */
	inline UPackage* PrepareFreshPackage(const FString& PackageName)
	{
		const FString AssetName = FPackageName::GetShortName(PackageName);
		const bool bExists = FPackageName::DoesPackageExist(PackageName);
		UPackage* const Package = bExists ? LoadPackage(nullptr, *PackageName, LOAD_None) : CreatePackage(*PackageName);
		if (not IsValid(Package))
		{
			return nullptr;
		}
		Package->FullyLoad();
		if (UObject* const Existing = StaticFindObject(UObject::StaticClass(), Package, *AssetName))
		{
			Existing->ClearFlags(RF_Public | RF_Standalone);
			Existing->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
			Existing->MarkAsGarbage();
		}
		return Package;
	}

	inline bool SaveAsset(UObject& Asset)
	{
		UPackage* const Package = Asset.GetOutermost();
		Package->MarkPackageDirty();
		const FString FileName = FPackageName::LongPackageNameToFilename(Package->GetName(),
		                                                                 FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Package, &Asset, *FileName, SaveArgs);
	}

	template <typename TAsset>
	TAsset* DuplicateIntoFreshPackage(TAsset& Source, const FString& PackageName)
	{
		UPackage* const Package = PrepareFreshPackage(PackageName);
		if (Package == nullptr)
		{
			return nullptr;
		}
		TAsset* const Duplicate = DuplicateObject<TAsset>(&Source, Package, *FPackageName::GetShortName(PackageName));
		Duplicate->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
		return Duplicate;
	}
}

#endif // WITH_EDITOR
