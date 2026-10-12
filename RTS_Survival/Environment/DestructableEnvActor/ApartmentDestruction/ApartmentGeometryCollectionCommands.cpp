// Copyright (C) Bas Blokzijl - All rights reserved.

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "Chaos/ChaosSolverActor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ComponentReregisterContext.h"
#include "Engine/StaticMesh.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/GeometryCollectionEngineConversion.h"
#include "Materials/MaterialInterface.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionClusteringUtility.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSApartmentGeometryCollection, Log, All);

namespace RTSApartmentGeometryCollection
{
	constexpr int32 RequiredArgumentCount = 2;
	constexpr int32 RequiredLayoutLineCount = 2;
	constexpr int32 BayPerLeafLineIndex = 0;
	constexpr int32 StoreyPerBayLineIndex = 1;
	// Kit pieces are separate meshes that touch or slightly overlap; neighbours are linked when their bounds, grown
	// by this margin, overlap. Without links every panel is its own island and one break releases the whole building.
	constexpr float ConnectionBoundsMargin = 15.f;
	const TCHAR* const NoStoreyLevelToken = TEXT("none");

	/** @brief Makes the collection hold together as a connected structure once pieces start breaking off. */
	void ApplyBuildingSimulationSettings(UGeometryCollection* GeometryCollectionAsset)
	{
		// The window glass of the kit is translucent, which the Nanite geometry collection proxy does not accept.
		GeometryCollectionAsset->EnableNanite = false;
		GeometryCollectionAsset->ClusterConnectionType = EClusterConnectionTypeEnum::Chaos_BoundsOverlapFilteredDelaunayTriangulation;
		GeometryCollectionAsset->ConnectionGraphBoundsFilteringMargin = ConnectionBoundsMargin;
	}

	/** @return Whether every comma separated token of the line was parsed into OutValues. */
	bool ParseIntegerLine(const FString& Line, TArray<int32>& OutValues)
	{
		TArray<FString> Tokens;
		Line.ParseIntoArray(Tokens, TEXT(","), true);
		OutValues.Reserve(Tokens.Num());
		for (const FString& Token : Tokens)
		{
			if (not Token.TrimStartAndEnd().IsNumeric())
			{
				return false;
			}
			OutValues.Add(FCString::Atoi(*Token));
		}
		return OutValues.Num() > 0;
	}

	/**
	 * @brief Groups the flat leaf bones into clusters under the root (optionally with a storey level in between), so a
	 * hit frees a wall slot with its facings and trims as one body instead of interpenetrating loose parts.
	 * @param Collection The flat collection as produced by the Geometry Collection factory.
	 * @param BayPerLeaf Bay id for every leaf bone, indexed by leaf transform index.
	 * @param StoreyPerBay Storey id for every bay id.
	 * @return Whether the hierarchy was built.
	 */
	bool BuildStoreyBayHierarchy(FGeometryCollection* Collection, const TArray<int32>& BayPerLeaf,
	                             const TArray<int32>& StoreyPerBay)
	{
		// Leaves are the transforms the factory appended first; a root added afterwards sits behind them.
		const int32 LeafCount = BayPerLeaf.Num();
		if (LeafCount > Collection->NumElements(FGeometryCollection::TransformGroup))
		{
			UE_LOG(LogRTSApartmentGeometryCollection, Error, TEXT("Leaf count mismatch: collection has %d, layout has %d."),
			       LeafCount, BayPerLeaf.Num());
			return false;
		}

		TMap<int32, TArray<int32>> LeavesPerBay;
		for (int32 LeafIndex = 0; LeafIndex < LeafCount; ++LeafIndex)
		{
			const int32 BayId = BayPerLeaf[LeafIndex];
			if (not StoreyPerBay.IsEmpty() && not StoreyPerBay.IsValidIndex(BayId))
			{
				UE_LOG(LogRTSApartmentGeometryCollection, Error, TEXT("Leaf %d references unknown bay %d."), LeafIndex, BayId);
				return false;
			}
			LeavesPerBay.FindOrAdd(BayId).Add(LeafIndex);
		}

		if (FGeometryCollectionClusteringUtility::ContainsMultipleRootBones(Collection))
		{
			FGeometryCollectionClusteringUtility::ClusterAllBonesUnderNewRoot(Collection);
		}
		TArray<int32> RootBones;
		FGeometryCollectionClusteringUtility::GetRootBones(Collection, RootBones);
		if (RootBones.Num() != 1)
		{
			UE_LOG(LogRTSApartmentGeometryCollection, Error, TEXT("Expected a single root after clustering, found %d."),
			       RootBones.Num());
			return false;
		}
		const int32 RootBone = RootBones[0];

		TMap<int32, TArray<int32>> BayNodesPerStorey;
		for (const TPair<int32, TArray<int32>>& BayLeaves : LeavesPerBay)
		{
			// A group of one stays a plain leaf under the root.
			if (BayLeaves.Value.Num() < 2)
			{
				continue;
			}
			const int32 BayNode = FGeometryCollectionClusteringUtility::ClusterBonesUnderNewNodeWithParent(
				Collection, RootBone, BayLeaves.Value, false, false);
			if (not StoreyPerBay.IsEmpty())
			{
				BayNodesPerStorey.FindOrAdd(StoreyPerBay[BayLeaves.Key]).Add(BayNode);
			}
		}
		for (const TPair<int32, TArray<int32>>& StoreyBays : BayNodesPerStorey)
		{
			FGeometryCollectionClusteringUtility::ClusterBonesUnderNewNodeWithParent(
				Collection, RootBone, StoreyBays.Value, false, false);
		}

		FGeometryCollectionClusteringUtility::UpdateHierarchyLevelOfChildren(Collection, RootBone);
		FGeometryCollectionClusteringUtility::ValidateResults(Collection);
		return true;
	}

	void ClusterApartmentGeometryCollection(const TArray<FString>& Arguments)
	{
		if (Arguments.Num() < RequiredArgumentCount)
		{
			UE_LOG(LogRTSApartmentGeometryCollection, Error,
			       TEXT("Usage: RTS.Apartment.ClusterGeometryCollection <GeometryCollectionAssetPath> <LayoutFilePath>"));
			return;
		}

		UGeometryCollection* GeometryCollectionAsset = LoadObject<UGeometryCollection>(nullptr, *Arguments[0]);
		if (not IsValid(GeometryCollectionAsset) || not GeometryCollectionAsset->GetGeometryCollection().IsValid())
		{
			UE_LOG(LogRTSApartmentGeometryCollection, Error, TEXT("Could not load geometry collection '%s'."), *Arguments[0]);
			return;
		}

		TArray<FString> LayoutLines;
		if (not FFileHelper::LoadFileToStringArray(LayoutLines, *Arguments[1]) || LayoutLines.Num() < RequiredLayoutLineCount)
		{
			UE_LOG(LogRTSApartmentGeometryCollection, Error, TEXT("Could not read layout file '%s'."), *Arguments[1]);
			return;
		}

		TArray<int32> BayPerLeaf;
		TArray<int32> StoreyPerBay;
		const bool bHasStoreyLevel = not LayoutLines[StoreyPerBayLineIndex].TrimStartAndEnd().Equals(NoStoreyLevelToken, ESearchCase::IgnoreCase);
		if (not ParseIntegerLine(LayoutLines[BayPerLeafLineIndex], BayPerLeaf)
			|| (bHasStoreyLevel && not ParseIntegerLine(LayoutLines[StoreyPerBayLineIndex], StoreyPerBay)))
		{
			UE_LOG(LogRTSApartmentGeometryCollection, Error, TEXT("Layout file '%s' is malformed."), *Arguments[1]);
			return;
		}

		// Live components cache per-transform render data; they must be torn down while the transform count changes,
		// otherwise their scene proxies assert on the stale counts.
		TArray<TUniquePtr<FComponentReregisterContext>> ReregisterContexts;
		for (TObjectIterator<UGeometryCollectionComponent> ComponentIterator; ComponentIterator; ++ComponentIterator)
		{
			if (ComponentIterator->GetRestCollection() == GeometryCollectionAsset)
			{
				ReregisterContexts.Add(MakeUnique<FComponentReregisterContext>(*ComponentIterator));
			}
		}

		GeometryCollectionAsset->Modify();
		if (not BuildStoreyBayHierarchy(GeometryCollectionAsset->GetGeometryCollection().Get(), BayPerLeaf, StoreyPerBay))
		{
			return;
		}

		ApplyBuildingSimulationSettings(GeometryCollectionAsset);
		GeometryCollectionAsset->UpdateGeometryDependentProperties();
		GeometryCollectionAsset->InvalidateCollection();
		GeometryCollectionAsset->RebuildRenderData();
		GeometryCollectionAsset->MarkPackageDirty();
		UE_LOG(LogRTSApartmentGeometryCollection, Display, TEXT("Clustered '%s': %d leaves, %d bays, %d transforms total."),
		       *Arguments[0], BayPerLeaf.Num(), StoreyPerBay.Num(),
		       GeometryCollectionAsset->GetGeometryCollection()->NumElements(FGeometryCollection::TransformGroup));
	}

	/**
	 * @brief Moves every leaf directly under the root; Chaos only applies point strain to an active cluster's direct
	 * children, so panels must sit one level below the root to be broken loose individually by a hit.
	 * @param Collection The collection whose intermediate clusters are removed.
	 * @return Whether a single root with only leaf children remains.
	 */
	bool FlattenToRootAndLeaves(FGeometryCollection* Collection)
	{
		if (FGeometryCollectionClusteringUtility::ContainsMultipleRootBones(Collection))
		{
			FGeometryCollectionClusteringUtility::ClusterAllBonesUnderNewRoot(Collection);
		}
		TArray<int32> RootBones;
		FGeometryCollectionClusteringUtility::GetRootBones(Collection, RootBones);
		if (RootBones.Num() != 1)
		{
			UE_LOG(LogRTSApartmentGeometryCollection, Error, TEXT("Expected a single root, found %d."), RootBones.Num());
			return false;
		}
		const int32 RootBone = RootBones[0];
		TArray<int32> LeafBones;
		FGeometryCollectionClusteringUtility::GetLeafBones(Collection, RootBone, true, LeafBones);
		// Intermediate clusters were created without their own transform, so leaves keep their local transform.
		for (const int32 LeafBone : LeafBones)
		{
			const int32 OldParent = Collection->Parent[LeafBone];
			if (OldParent == RootBone)
			{
				continue;
			}
			if (OldParent != FGeometryCollection::Invalid)
			{
				Collection->Children[OldParent].Remove(LeafBone);
			}
			Collection->Parent[LeafBone] = RootBone;
			Collection->Children[RootBone].Add(LeafBone);
		}
		// Each pass only removes clusters that are empty at that moment; emptied parents follow in the next pass.
		while (FGeometryCollectionClusteringUtility::RemoveDanglingClusters(Collection))
		{
		}

		// GetRootBones appends to its output.
		RootBones.Reset();
		FGeometryCollectionClusteringUtility::GetRootBones(Collection, RootBones);
		if (RootBones.Num() != 1)
		{
			UE_LOG(LogRTSApartmentGeometryCollection, Error, TEXT("Flattening left %d roots."), RootBones.Num());
			for (const int32 RemainingRoot : RootBones)
			{
				UE_LOG(LogRTSApartmentGeometryCollection, Error, TEXT("  root %d children=%d simulation_type=%d name=%s"),
				       RemainingRoot, Collection->Children[RemainingRoot].Num(), Collection->SimulationType[RemainingRoot],
				       *Collection->BoneName[RemainingRoot]);
			}
			return false;
		}
		FGeometryCollectionClusteringUtility::UpdateHierarchyLevelOfChildren(Collection, RootBones[0]);
		FGeometryCollectionClusteringUtility::ValidateResults(Collection);
		return true;
	}

	void FlattenApartmentGeometryCollection(const TArray<FString>& Arguments)
	{
		if (Arguments.Num() < 1)
		{
			UE_LOG(LogRTSApartmentGeometryCollection, Error,
			       TEXT("Usage: RTS.Apartment.FlattenGeometryCollection <GeometryCollectionAssetPath>"));
			return;
		}
		UGeometryCollection* GeometryCollectionAsset = LoadObject<UGeometryCollection>(nullptr, *Arguments[0]);
		if (not IsValid(GeometryCollectionAsset) || not GeometryCollectionAsset->GetGeometryCollection().IsValid())
		{
			UE_LOG(LogRTSApartmentGeometryCollection, Error, TEXT("Could not load geometry collection '%s'."), *Arguments[0]);
			return;
		}

		// Live components cache per-transform render data; they must be torn down while the transform count changes.
		TArray<TUniquePtr<FComponentReregisterContext>> ReregisterContexts;
		for (TObjectIterator<UGeometryCollectionComponent> ComponentIterator; ComponentIterator; ++ComponentIterator)
		{
			if (ComponentIterator->GetRestCollection() == GeometryCollectionAsset)
			{
				ReregisterContexts.Add(MakeUnique<FComponentReregisterContext>(*ComponentIterator));
			}
		}

		GeometryCollectionAsset->Modify();
		if (not FlattenToRootAndLeaves(GeometryCollectionAsset->GetGeometryCollection().Get()))
		{
			return;
		}
		ApplyBuildingSimulationSettings(GeometryCollectionAsset);
		GeometryCollectionAsset->UpdateGeometryDependentProperties();
		GeometryCollectionAsset->InvalidateCollection();
		GeometryCollectionAsset->RebuildRenderData();
		GeometryCollectionAsset->MarkPackageDirty();
		UE_LOG(LogRTSApartmentGeometryCollection, Display, TEXT("Flattened '%s': %d transforms total."), *Arguments[0],
		       GeometryCollectionAsset->GetGeometryCollection()->NumElements(FGeometryCollection::TransformGroup));
	}

	FAutoConsoleCommand FlattenGeometryCollectionCommand(
		TEXT("RTS.Apartment.FlattenGeometryCollection"),
		TEXT("Moves every leaf of an apartment geometry collection directly under one root. Args: <GeometryCollectionAssetPath>"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&FlattenApartmentGeometryCollection));

	/** @brief One kit piece of a planned building, as written by the offline planner. */
	struct FApartmentPiece
	{
		FString MeshPath;
		TArray<FString> MaterialPaths;
		FTransform Transform = FTransform::Identity;
		// Pieces that share a slot (a wall panel with its facings, trims and window parts) break off as one body.
		int32 SlotId = 0;
	};

	/**
	 * @brief Reads "MeshPath|Material;Material|X,Y,Z|Yaw,Pitch,Roll|SlotId" lines.
	 * @param PiecesFilePath File written by the planner export.
	 * @param OutPieces Receives one entry per valid line, in file order.
	 * @return Whether at least one piece was read and no line was malformed.
	 */
	bool LoadApartmentPieces(const FString& PiecesFilePath, TArray<FApartmentPiece>& OutPieces)
	{
		constexpr int32 FieldCount = 5;
		constexpr int32 VectorComponentCount = 3;
		TArray<FString> Lines;
		if (not FFileHelper::LoadFileToStringArray(Lines, *PiecesFilePath))
		{
			return false;
		}
		for (const FString& Line : Lines)
		{
			if (Line.TrimStartAndEnd().IsEmpty())
			{
				continue;
			}
			TArray<FString> Fields;
			Line.ParseIntoArray(Fields, TEXT("|"), false);
			TArray<FString> LocationTokens;
			TArray<FString> RotationTokens;
			if (Fields.Num() != FieldCount
				|| Fields[2].ParseIntoArray(LocationTokens, TEXT(","), false) != VectorComponentCount
				|| Fields[3].ParseIntoArray(RotationTokens, TEXT(","), false) != VectorComponentCount)
			{
				UE_LOG(LogRTSApartmentGeometryCollection, Error, TEXT("Malformed piece line: %s"), *Line);
				return false;
			}
			FApartmentPiece Piece;
			Piece.MeshPath = Fields[0];
			Fields[1].ParseIntoArray(Piece.MaterialPaths, TEXT(";"), false);
			const FVector Location(FCString::Atod(*LocationTokens[0]), FCString::Atod(*LocationTokens[1]), FCString::Atod(*LocationTokens[2]));
			const FRotator Rotation(FCString::Atod(*RotationTokens[1]), FCString::Atod(*RotationTokens[0]), FCString::Atod(*RotationTokens[2]));
			Piece.Transform = FTransform(Rotation, Location);
			Piece.SlotId = FCString::Atoi(*Fields[4]);
			OutPieces.Add(MoveTemp(Piece));
		}
		return OutPieces.Num() > 0;
	}

	/** @return Whether every piece was appended as its own transform, in file order. */
	bool AppendApartmentPieces(UGeometryCollection* GeometryCollectionAsset, const TArray<FApartmentPiece>& Pieces)
	{
		for (const FApartmentPiece& Piece : Pieces)
		{
			const UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Piece.MeshPath);
			if (not IsValid(Mesh))
			{
				UE_LOG(LogRTSApartmentGeometryCollection, Error, TEXT("Could not load mesh '%s'."), *Piece.MeshPath);
				return false;
			}
			TArray<UMaterialInterface*> Materials;
			for (const FString& MaterialPath : Piece.MaterialPaths)
			{
				Materials.Add(LoadObject<UMaterialInterface>(nullptr, *MaterialPath));
			}
			if (not FGeometryCollectionEngineConversion::AppendStaticMesh(
				Mesh, Materials, Piece.Transform, GeometryCollectionAsset, false, false, false, false))
			{
				UE_LOG(LogRTSApartmentGeometryCollection, Error, TEXT("Could not append mesh '%s'."), *Piece.MeshPath);
				return false;
			}
		}
		return true;
	}

	void BuildApartmentGeometryCollection(const TArray<FString>& Arguments)
	{
		if (Arguments.Num() < RequiredArgumentCount)
		{
			UE_LOG(LogRTSApartmentGeometryCollection, Error,
			       TEXT("Usage: RTS.Apartment.BuildGeometryCollection <NewAssetPackagePath> <PiecesFilePath>"));
			return;
		}
		TArray<FApartmentPiece> Pieces;
		if (not LoadApartmentPieces(Arguments[1], Pieces))
		{
			UE_LOG(LogRTSApartmentGeometryCollection, Error, TEXT("Could not read pieces file '%s'."), *Arguments[1]);
			return;
		}

		const FString AssetName = FPackageName::GetLongPackageAssetName(Arguments[0]);
		UPackage* Package = CreatePackage(*Arguments[0]);
		if (not IsValid(Package) || FindObject<UGeometryCollection>(Package, *AssetName) != nullptr)
		{
			UE_LOG(LogRTSApartmentGeometryCollection, Error, TEXT("'%s' already exists; delete it first."), *Arguments[0]);
			return;
		}
		UGeometryCollection* GeometryCollectionAsset = NewObject<UGeometryCollection>(
			Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
		if (not AppendApartmentPieces(GeometryCollectionAsset, Pieces))
		{
			return;
		}
		GeometryCollectionAsset->InitializeMaterials(false);
		GeometryCollectionAlgo::PrepareForSimulation(GeometryCollectionAsset->GetGeometryCollection().Get());

		TArray<int32> SlotPerLeaf;
		SlotPerLeaf.Reserve(Pieces.Num());
		for (const FApartmentPiece& Piece : Pieces)
		{
			SlotPerLeaf.Add(Piece.SlotId);
		}
		if (not BuildStoreyBayHierarchy(GeometryCollectionAsset->GetGeometryCollection().Get(), SlotPerLeaf, TArray<int32>()))
		{
			return;
		}
		ApplyBuildingSimulationSettings(GeometryCollectionAsset);
		GeometryCollectionAsset->UpdateGeometryDependentProperties();
		GeometryCollectionAsset->InvalidateCollection();
		GeometryCollectionAsset->RebuildRenderData();
		GeometryCollectionAsset->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(GeometryCollectionAsset);
		UE_LOG(LogRTSApartmentGeometryCollection, Display, TEXT("Built '%s': %d pieces, %d transforms total."), *Arguments[0],
		       Pieces.Num(), GeometryCollectionAsset->GetGeometryCollection()->NumElements(FGeometryCollection::TransformGroup));
	}

	FAutoConsoleCommand BuildGeometryCollectionCommand(
		TEXT("RTS.Apartment.BuildGeometryCollection"),
		TEXT("Builds a destructible apartment geometry collection from a planner pieces file, one breakable unit per slot. ")
		TEXT("Args: <NewAssetPackagePath> <PiecesFilePath>"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&BuildApartmentGeometryCollection));

	FAutoConsoleCommand ClusterGeometryCollectionCommand(
		TEXT("RTS.Apartment.ClusterGeometryCollection"),
		TEXT("Builds a root -> storey -> bay cluster hierarchy on a flat apartment geometry collection. ")
		TEXT("Args: <GeometryCollectionAssetPath> <LayoutFilePath>"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&ClusterApartmentGeometryCollection));
}

#endif // WITH_EDITOR
