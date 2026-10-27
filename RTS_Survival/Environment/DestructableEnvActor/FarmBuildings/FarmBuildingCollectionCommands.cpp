// Copyright (C) Bas Blokzijl - All rights reserved.

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/GeometryCollectionClusteringUtility.h"
#include "GeometryCollection/GeometryCollectionConvexPropertiesInterface.h"
#include "GeometryCollection/GeometryCollectionEngineConversion.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "HAL/IConsoleManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSFarmBuildingCollection, Log, All);

/**
 * Builds the geometry collection a destructible farm building collapses into. The source is a pre-fractured static
 * mesh made offline (FarmBuildings pipeline): every piece is a separate mesh island and its vertex colour red channel
 * holds the section (wall, gable, roof side, chimney) the piece belongs to, encoded as section * 16 + 8.
 * The collection becomes root -> one cluster per section -> pieces, so the collapse strain first breaks the building
 * into falling walls and roof halves, which then shatter into their pieces when they hit the ground.
 */
namespace RTSFarmBuildingCollection
{
	constexpr int32 RequiredArgumentCount = 2;
	const TCHAR* const OverwriteToken = TEXT("Overwrite");
	constexpr float SectionColourStep = 16.f;
	constexpr int32 MaximumSections = 16;
	// Pieces are separate islands that touch; neighbours are linked when their bounds, grown by this, overlap.
	constexpr float ConnectionBoundsMargin = 6.f;
	// Hull corners closer together than this are merged; the thinnest pieces (tiles, boards) are ~4 cm thick.
	constexpr float SimplifiedHullCornerDistance = 2.f;
	constexpr float CollisionSampleFraction = 0.05f;
	// Pieces touch or overlap (infill against posts, chimney through the roof); shrunken hulls start apart, so the
	// solver does not fling the debris across the map while it pushes them out of each other.
	constexpr float CollisionShrinkPercentage = 12.f;
	// Root: only the collapse strain breaks it. Sections: break apart when they land. Pieces: stay whole.
	const TArray<float> DamageThresholdPerLevel = {1000000.f, 120.f, 1000000.f};
	// Resting pieces shrink away between these times so a collapsed farm does not keep hundreds of bodies awake.
	const FVector2D SleepSecondsBeforeRemoval(6.f, 12.f);
	const FVector2D RemovalSeconds(2.f, 4.f);

	float SrgbToLinear(const float Value)
	{
		return Value <= 0.04045f ? Value / 12.92f : FMath::Pow((Value + 0.055f) / 1.055f, 2.4f);
	}

	/**
	 * @brief The importer may or may not have converted the vertex colours from sRGB; decode with both conventions
	 * and keep the one whose values sit closest to the encoded section steps.
	 * @param MeanRedPerPiece Average red value of every piece's vertices.
	 * @param OutSectionPerPiece Receives the decoded section per piece.
	 */
	void DecodeSections(const TArray<float>& MeanRedPerPiece, TArray<int32>& OutSectionPerPiece)
	{
		double RawError = 0.0;
		double LinearError = 0.0;
		TArray<int32> RawSections;
		TArray<int32> LinearSections;
		for (const float Red : MeanRedPerPiece)
		{
			const float RawStep = (Red * 255.f - 8.f) / SectionColourStep;
			RawSections.Add(FMath::Clamp(FMath::RoundToInt(RawStep), 0, MaximumSections - 1));
			RawError += FMath::Abs(RawStep - FMath::RoundToFloat(RawStep));
			int32 BestSection = 0;
			float BestDistance = TNumericLimits<float>::Max();
			for (int32 Section = 0; Section < MaximumSections; ++Section)
			{
				const float Encoded = SrgbToLinear((Section * SectionColourStep + 8.f) / 255.f);
				const float Distance = FMath::Abs(Encoded - Red);
				if (Distance < BestDistance)
				{
					BestDistance = Distance;
					BestSection = Section;
				}
			}
			LinearSections.Add(BestSection);
			LinearError += BestDistance * 255.f / SectionColourStep;
		}
		const bool bUseRaw = RawError <= LinearError;
		OutSectionPerPiece = bUseRaw ? RawSections : LinearSections;
		UE_LOG(LogRTSFarmBuildingCollection, Display, TEXT("Section colours decoded as %s (error raw %.2f, linear %.2f)."),
		       bUseRaw ? TEXT("raw") : TEXT("sRGB->linear"), RawError, LinearError);
	}

	/** @return Average vertex red of every geometry, indexed by transform; -1 for transforms without geometry. */
	TArray<float> GetMeanRedPerTransform(const FGeometryCollection& Collection)
	{
		TArray<float> MeanRed;
		MeanRed.Init(-1.f, Collection.NumElements(FGeometryCollection::TransformGroup));
		for (int32 GeometryIndex = 0; GeometryIndex < Collection.NumElements(FGeometryCollection::GeometryGroup);
		     ++GeometryIndex)
		{
			const int32 Start = Collection.VertexStart[GeometryIndex];
			const int32 Count = Collection.VertexCount[GeometryIndex];
			if (Count <= 0)
			{
				continue;
			}
			double Sum = 0.0;
			for (int32 VertexIndex = Start; VertexIndex < Start + Count; ++VertexIndex)
			{
				Sum += Collection.Color[VertexIndex].R;
			}
			MeanRed[Collection.TransformIndex[GeometryIndex]] = static_cast<float>(Sum / Count);
		}
		return MeanRed;
	}

	/**
	 * @brief Puts every piece in the cluster of its section under a single root.
	 * @param Collection Flat collection straight from the split static mesh.
	 * @return Whether a single root with section clusters was built.
	 */
	bool BuildSectionHierarchy(FGeometryCollection& Collection)
	{
		const TArray<float> MeanRed = GetMeanRedPerTransform(Collection);
		TArray<int32> PieceTransforms;
		TArray<float> PieceRed;
		for (int32 TransformIndex = 0; TransformIndex < MeanRed.Num(); ++TransformIndex)
		{
			if (MeanRed[TransformIndex] >= 0.f)
			{
				PieceTransforms.Add(TransformIndex);
				PieceRed.Add(MeanRed[TransformIndex]);
			}
		}
		TArray<int32> SectionPerPiece;
		DecodeSections(PieceRed, SectionPerPiece);

		if (FGeometryCollectionClusteringUtility::ContainsMultipleRootBones(&Collection))
		{
			FGeometryCollectionClusteringUtility::ClusterAllBonesUnderNewRoot(&Collection);
		}
		TArray<int32> RootBones;
		FGeometryCollectionClusteringUtility::GetRootBones(&Collection, RootBones);
		if (RootBones.Num() != 1)
		{
			UE_LOG(LogRTSFarmBuildingCollection, Error, TEXT("Expected a single root, found %d."), RootBones.Num());
			return false;
		}
		const int32 RootBone = RootBones[0];
		TMap<int32, TArray<int32>> PiecesPerSection;
		for (int32 PieceIndex = 0; PieceIndex < PieceTransforms.Num(); ++PieceIndex)
		{
			PiecesPerSection.FindOrAdd(SectionPerPiece[PieceIndex]).Add(PieceTransforms[PieceIndex]);
		}
		for (const TPair<int32, TArray<int32>>& SectionPieces : PiecesPerSection)
		{
			UE_LOG(LogRTSFarmBuildingCollection, Display, TEXT("  section %d: %d pieces"), SectionPieces.Key,
			       SectionPieces.Value.Num());
			if (SectionPieces.Value.Num() < 2)
			{
				continue;
			}
			FGeometryCollectionClusteringUtility::ClusterBonesUnderNewNodeWithParent(
				&Collection, RootBone, SectionPieces.Value, false, false);
		}
		FGeometryCollectionClusteringUtility::UpdateHierarchyLevelOfChildren(&Collection, RootBone);
		FGeometryCollectionClusteringUtility::ValidateResults(&Collection);
		return true;
	}

	void ApplyFarmBuildingSettings(UGeometryCollection& CollectionAsset)
	{
		CollectionAsset.EnableNanite = false;
		CollectionAsset.EnableClustering = true;
		CollectionAsset.DamageModel = EDamageModelTypeEnum::Chaos_Damage_Model_UserDefined_Damage_Threshold;
		CollectionAsset.bUseSizeSpecificDamageThreshold = false;
		CollectionAsset.DamageThreshold = DamageThresholdPerLevel;
		CollectionAsset.ClusterConnectionType =
			EClusterConnectionTypeEnum::Chaos_BoundsOverlapFilteredDelaunayTriangulation;
		CollectionAsset.ConnectionGraphBoundsFilteringMargin = ConnectionBoundsMargin;
		CollectionAsset.bRemoveOnMaxSleep = true;
		CollectionAsset.MaximumSleepTime = SleepSecondsBeforeRemoval;
		CollectionAsset.RemovalDuration = RemovalSeconds;
		CollectionAsset.bScaleOnRemoval = true;

		FGeometryCollectionConvexPropertiesInterface::FConvexCreationProperties ConvexProperties =
			CollectionAsset.GetGeometryCollection()->GetConvexProperties();
		// The pieces are convex already; cutting overlaps away would only add hull faces where pieces touch.
		ConvexProperties.RemoveOverlaps = EConvexOverlapRemoval::None;
		ConvexProperties.SimplificationThreshold = SimplifiedHullCornerDistance;
		CollectionAsset.GetGeometryCollection()->SetConvexProperties(ConvexProperties);
		if (CollectionAsset.SizeSpecificData.IsEmpty())
		{
			CollectionAsset.SizeSpecificData.AddDefaulted();
		}
		for (FGeometryCollectionSizeSpecificData& SizeData : CollectionAsset.SizeSpecificData)
		{
			for (FGeometryCollectionCollisionTypeData& CollisionShape : SizeData.CollisionShapes)
			{
				CollisionShape.CollisionParticles.CollisionParticlesFraction = CollisionSampleFraction;
				CollisionShape.CollisionObjectReductionPercentage = CollisionShrinkPercentage;
			}
		}
	}

	/** @brief Geometry collections draw only with materials flagged for them; a cooked game cannot add the flag. */
	void EnsureMaterialsSupportGeometryCollections(const UGeometryCollection& Collection)
	{
		for (UMaterialInterface* MaterialInterface : Collection.Materials)
		{
			UMaterial* Material = IsValid(MaterialInterface) ? MaterialInterface->GetMaterial() : nullptr;
			if (not IsValid(Material) || Material->bUsedWithGeometryCollections
				|| Material->GetPathName().StartsWith(TEXT("/Engine/")))
			{
				continue;
			}
			bool bNeedsRecompile = false;
			Material->SetMaterialUsage(bNeedsRecompile, MATUSAGE_GeometryCollections);
			Material->MarkPackageDirty();
			UPackage* Package = Material->GetOutermost();
			const FString FileName = FPackageName::LongPackageNameToFilename(Package->GetName(),
			                                                                 FPackageName::GetAssetPackageExtension());
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
			const bool bSaved = UPackage::SavePackage(Package, Material, *FileName, SaveArgs);
			UE_LOG(LogRTSFarmBuildingCollection, Display, TEXT("Material %s now supports geometry collections (saved=%d)."),
			       *Material->GetPathName(), bSaved);
		}
	}

	/** @return The new, empty collection in its package; an existing one is moved aside so references stay valid. */
	UGeometryCollection* CreateCollectionAsset(const FString& PackageName, const bool bOverwrite)
	{
		const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
		const bool bExists = FPackageName::DoesPackageExist(PackageName);
		if (bExists && not bOverwrite)
		{
			UE_LOG(LogRTSFarmBuildingCollection, Error, TEXT("'%s' already exists; pass Overwrite to rebuild it."),
			       *PackageName);
			return nullptr;
		}
		// An existing package must be loaded in full, else saving over it is refused.
		UPackage* Package = bExists ? LoadPackage(nullptr, *PackageName, LOAD_None) : CreatePackage(*PackageName);
		if (not IsValid(Package))
		{
			UE_LOG(LogRTSFarmBuildingCollection, Error, TEXT("Could not open package %s."), *PackageName);
			return nullptr;
		}
		Package->FullyLoad();
		if (UGeometryCollection* Existing = FindObject<UGeometryCollection>(Package, *AssetName))
		{
			Existing->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
		}
		return NewObject<UGeometryCollection>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
	}

	bool SaveCollection(UGeometryCollection& CollectionAsset)
	{
		UPackage* Package = CollectionAsset.GetOutermost();
		const FString FileName = FPackageName::LongPackageNameToFilename(Package->GetName(),
		                                                                 FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		return UPackage::SavePackage(Package, &CollectionAsset, *FileName, SaveArgs);
	}

	void BuildFarmBuildingCollection(const TArray<FString>& Arguments)
	{
		if (Arguments.Num() < RequiredArgumentCount)
		{
			UE_LOG(LogRTSFarmBuildingCollection, Error,
			       TEXT("Usage: RTS.FarmBuildings.BuildCollection <FracturedStaticMeshPath> <NewCollectionPackagePath> [Overwrite]"));
			return;
		}
		const UStaticMesh* FracturedMesh = LoadObject<UStaticMesh>(nullptr, *Arguments[0]);
		if (not IsValid(FracturedMesh))
		{
			UE_LOG(LogRTSFarmBuildingCollection, Error, TEXT("Could not load static mesh '%s'."), *Arguments[0]);
			return;
		}
		UGeometryCollection* CollectionAsset = CreateCollectionAsset(Arguments[1], Arguments.Contains(OverwriteToken));
		if (CollectionAsset == nullptr)
		{
			return;
		}
		TArray<UMaterialInterface*> Materials;
		for (const FStaticMaterial& StaticMaterial : FracturedMesh->GetStaticMaterials())
		{
			Materials.Add(StaticMaterial.MaterialInterface);
		}
		constexpr bool bReindexMaterials = false;
		// The torn faces are textured on the atlas already; an extra internal slot would shift the material indices.
		constexpr bool bAddInternalMaterials = false;
		constexpr bool bSplitComponents = true;
		if (not FGeometryCollectionEngineConversion::AppendStaticMesh(FracturedMesh, Materials, FTransform::Identity,
		                                                              CollectionAsset, bReindexMaterials,
		                                                              bAddInternalMaterials, bSplitComponents))
		{
			UE_LOG(LogRTSFarmBuildingCollection, Error, TEXT("Could not append '%s'."), *Arguments[0]);
			return;
		}
		FGeometryCollection& Collection = *CollectionAsset->GetGeometryCollection();
		CollectionAsset->InitializeMaterials(false);
		GeometryCollectionAlgo::PrepareForSimulation(&Collection);
		if (not BuildSectionHierarchy(Collection))
		{
			return;
		}
		ApplyFarmBuildingSettings(*CollectionAsset);
		CollectionAsset->UpdateGeometryDependentProperties();
		CollectionAsset->InvalidateCollection();
		CollectionAsset->RebuildRenderData();
		EnsureMaterialsSupportGeometryCollections(*CollectionAsset);
		CollectionAsset->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(CollectionAsset);
		const int32 PieceCount = Collection.NumElements(FGeometryCollection::GeometryGroup);
		const bool bSaved = SaveCollection(*CollectionAsset);
		UE_LOG(LogRTSFarmBuildingCollection, Display, TEXT("RTS_FARM_BUILDING_COLLECTION built=%s pieces=%d transforms=%d saved=%d"),
		       *Arguments[1], PieceCount, Collection.NumElements(FGeometryCollection::TransformGroup), bSaved);
	}

	FAutoConsoleCommand BuildFarmBuildingCollectionCommand(
		TEXT("RTS.FarmBuildings.BuildCollection"),
		TEXT("Builds a destructible farm building's geometry collection from its pre-fractured static mesh (one ")
		TEXT("island per piece, vertex red = section). Args: <FracturedStaticMeshPath> <NewCollectionPackagePath> [Overwrite]"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&BuildFarmBuildingCollection));
}

#endif // WITH_EDITOR
