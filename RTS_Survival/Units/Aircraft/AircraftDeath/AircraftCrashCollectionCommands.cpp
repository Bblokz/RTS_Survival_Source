// Copyright (C) Bas Blokzijl - All rights reserved.

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "AircraftDeathMath.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/GeometryCollectionEngineConversion.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "HAL/IConsoleManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "PlanarCut.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Voronoi/Voronoi.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSAircraftCrashCollection, Log, All);

namespace RTSAircraftCrashCollection
{
	const TCHAR* const AircraftBlueprintFolder = TEXT("/Game/RTS_Survival/Blueprints/Aircraft");
	const TCHAR* const OverwriteToken = TEXT("Overwrite");
	// A fighter breaks into a few dozen pieces, a bomber into more; one piece per this much of the aircraft's size.
	constexpr float CentimetresPerPiece = 45.f;
	constexpr int32 MinimumPieces = 26;
	constexpr int32 MaximumPieces = 64;
	constexpr int32 FractureSeed = 1941;
	// Spacing of the collision surface samples on the cut faces; pieces only need a rough shape to tumble on.
	constexpr double CollisionSampleSpacing = 60.0;
	constexpr double VoronoiBoundsPadding = 10.0;
	// Texture repeats on the torn faces: once per two metres.
	constexpr float InteriorUVScale = 0.005f;
	// Pieces that came to rest shrink away between these times, so a wreck does not litter the map for good.
	const FVector2D SleepSecondsBeforeRemoval(8.f, 14.f);
	const FVector2D RemovalSeconds(2.f, 4.f);

	/** One mesh of an aircraft with where it sits on the aircraft. */
	struct FAircraftCrashMesh
	{
		const UStaticMesh* StaticMesh = nullptr;
		TArray<UMaterialInterface*> StaticMaterials;
		const USkeletalMesh* SkeletalMesh = nullptr;
		const USkeletalMeshComponent* SkeletalComponent = nullptr;
		FTransform RelativeTransform = FTransform::Identity;
	};

	/** The meshes an aircraft Blueprint draws, read from an instance spawned in the editor world. */
	void GatherAircraftMeshes(const AActor& Aircraft, TArray<FAircraftCrashMesh>& OutMeshes,
	                          TArray<FString>& OutStaticNames, TArray<FString>& OutSkeletalNames)
	{
		const FTransform ActorTransform = Aircraft.GetActorTransform();
		TArray<UStaticMeshComponent*> StaticComponents;
		Aircraft.GetComponents<UStaticMeshComponent>(StaticComponents);
		for (const UStaticMeshComponent* Component : StaticComponents)
		{
			if (not IsValid(Component) || not Component->IsVisible() || not IsValid(Component->GetStaticMesh()))
			{
				continue;
			}
			FAircraftCrashMesh& Mesh = OutMeshes.AddDefaulted_GetRef();
			Mesh.StaticMesh = Component->GetStaticMesh();
			Mesh.StaticMaterials = Component->GetMaterials();
			Mesh.RelativeTransform = Component->GetComponentTransform().GetRelativeTransform(ActorTransform);
			OutStaticNames.Add(Mesh.StaticMesh->GetName());
		}
		TArray<USkeletalMeshComponent*> SkeletalComponents;
		Aircraft.GetComponents<USkeletalMeshComponent>(SkeletalComponents);
		for (const USkeletalMeshComponent* Component : SkeletalComponents)
		{
			if (not IsValid(Component) || not IsValid(Component->GetSkeletalMeshAsset()))
			{
				continue;
			}
			FAircraftCrashMesh& Mesh = OutMeshes.AddDefaulted_GetRef();
			Mesh.SkeletalMesh = Component->GetSkeletalMeshAsset();
			Mesh.SkeletalComponent = Component;
			Mesh.RelativeTransform = Component->GetComponentTransform().GetRelativeTransform(ActorTransform);
			OutSkeletalNames.Add(Mesh.SkeletalMesh->GetName());
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
			UE_LOG(LogRTSAircraftCrashCollection, Display, TEXT("Material %s now supports geometry collections (saved=%d)."),
			       *Material->GetPathName(), bSaved);
		}
	}

	/** @return Voronoi sites on the aircraft's own surface, so every cell holds a piece of it and thin wings break too. */
	TArray<FVector> MakeFractureSites(const FGeometryCollection& Collection, const int32 SiteCount, FBox& OutBounds)
	{
		TArray<FVector> GlobalVertices;
		GlobalVertices.Reserve(Collection.Vertex.Num());
		TArray<FTransform> GlobalTransforms;
		GlobalTransforms.SetNum(Collection.Transform.Num());
		for (int32 TransformIndex = 0; TransformIndex < Collection.Transform.Num(); ++TransformIndex)
		{
			GlobalTransforms[TransformIndex] = GeometryCollectionAlgo::GlobalMatrix(Collection.Transform,
			                                                                        Collection.Parent, TransformIndex);
		}
		OutBounds = FBox(ForceInit);
		for (int32 VertexIndex = 0; VertexIndex < Collection.Vertex.Num(); ++VertexIndex)
		{
			const FVector Global = GlobalTransforms[Collection.BoneMap[VertexIndex]].TransformPosition(
				FVector(Collection.Vertex[VertexIndex]));
			GlobalVertices.Add(Global);
			OutBounds += Global;
		}
		TArray<FVector> Sites;
		if (GlobalVertices.IsEmpty())
		{
			return Sites;
		}
		const FRandomStream Stream(FractureSeed);
		for (int32 SiteIndex = 0; SiteIndex < SiteCount; ++SiteIndex)
		{
			Sites.Add(GlobalVertices[Stream.RandRange(0, GlobalVertices.Num() - 1)]);
		}
		return Sites;
	}

	int32 GetPieceTarget(const FBox& Bounds)
	{
		const FVector Size = Bounds.GetSize();
		const int32 Target = FMath::RoundToInt((Size.X + Size.Y) / CentimetresPerPiece);
		return FMath::Clamp(Target, MinimumPieces, MaximumPieces);
	}

	TArray<int32> GetTransformsWithGeometry(const FGeometryCollection& Collection)
	{
		TArray<int32> TransformsWithGeometry;
		for (int32 TransformIndex = 0; TransformIndex < Collection.Transform.Num(); ++TransformIndex)
		{
			if (Collection.TransformToGeometryIndex[TransformIndex] != INDEX_NONE)
			{
				TransformsWithGeometry.Add(TransformIndex);
			}
		}
		return TransformsWithGeometry;
	}

	/**
	 * @brief Moves the one merged geometry into collection space under an identity root and drops every other
	 * transform. Skeletal aircraft arrive with scaled bone transforms; Chaos ignores their scale, so the pieces would
	 * simulate at a hundredth of their size, far from the aircraft.
	 * @return Whether the collection holds exactly one geometry at its root afterwards.
	 */
	bool BakeIntoSingleRootGeometry(FGeometryCollection& Collection)
	{
		const TArray<int32> TransformsWithGeometry = GetTransformsWithGeometry(Collection);
		if (TransformsWithGeometry.Num() != 1)
		{
			return false;
		}
		const int32 Node = TransformsWithGeometry[0];
		const FTransform Global = GeometryCollectionAlgo::GlobalMatrix(Collection.Transform, Collection.Parent, Node);
		const int32 GeometryIndex = Collection.TransformToGeometryIndex[Node];
		const int32 FirstVertex = Collection.VertexStart[GeometryIndex];
		const int32 EndVertex = FirstVertex + Collection.VertexCount[GeometryIndex];
		for (int32 VertexIndex = FirstVertex; VertexIndex < EndVertex; ++VertexIndex)
		{
			Collection.Vertex[VertexIndex] = FVector3f(Global.TransformPosition(FVector(Collection.Vertex[VertexIndex])));
			Collection.Normal[VertexIndex] = FVector3f(
				Global.TransformVectorNoScale(FVector(Collection.Normal[VertexIndex])).GetSafeNormal());
			Collection.TangentU[VertexIndex] = FVector3f(
				Global.TransformVectorNoScale(FVector(Collection.TangentU[VertexIndex])).GetSafeNormal());
			Collection.TangentV[VertexIndex] = FVector3f(
				Global.TransformVectorNoScale(FVector(Collection.TangentV[VertexIndex])).GetSafeNormal());
		}
		Collection.Transform[Node] = FTransform3f::Identity;
		const int32 Parent = Collection.Parent[Node];
		if (Parent != INDEX_NONE)
		{
			Collection.Children[Parent].Remove(Node);
			Collection.Parent[Node] = INDEX_NONE;
		}
		TArray<int32> TransformsToRemove;
		for (int32 TransformIndex = 0; TransformIndex < Collection.Transform.Num(); ++TransformIndex)
		{
			if (TransformIndex != Node)
			{
				TransformsToRemove.Add(TransformIndex);
			}
		}
		if (not TransformsToRemove.IsEmpty())
		{
			Collection.RemoveElements(FGeometryCollection::TransformGroup, TransformsToRemove);
		}
		Collection.UpdateBoundingBox();
		return GetTransformsWithGeometry(Collection).Num() == 1;
	}

	/** @return Whether the collection was cut into pieces. */
	bool FractureCollection(FGeometryCollection& Collection)
	{
		// A skeletal mesh arrives as one geometry per bone; cut separately every cell would hold a piece of each.
		TArray<int32> TransformsToCut = GetTransformsWithGeometry(Collection);
		const int32 GeometryBeforeMerge = TransformsToCut.Num();
		if (TransformsToCut.Num() > 1)
		{
			// Skeletal mesh parts arrive as embedded geometry, which the merge leaves out.
			for (const int32 TransformIndex : TransformsToCut)
			{
				Collection.SimulationType[TransformIndex] = FGeometryCollection::ESimulationTypes::FST_Rigid;
			}
			MergeAllSelectedBones(Collection, TransformsToCut, false);
			TransformsToCut = GetTransformsWithGeometry(Collection);
		}
		if (not BakeIntoSingleRootGeometry(Collection))
		{
			UE_LOG(LogRTSAircraftCrashCollection, Error, TEXT("Could not bake the merged aircraft into one root geometry."));
			return false;
		}
		TransformsToCut = GetTransformsWithGeometry(Collection);
		UE_LOG(LogRTSAircraftCrashCollection, Display, TEXT("Geometry before merge=%d after=%d transforms=%d"),
		       GeometryBeforeMerge, TransformsToCut.Num(), Collection.Transform.Num());
		FBox Bounds;
		TArray<FVector> Sites = MakeFractureSites(Collection, MaximumPieces, Bounds);
		if (Sites.IsEmpty() || TransformsToCut.IsEmpty())
		{
			return false;
		}
		Sites.SetNum(GetPieceTarget(Bounds));
		FVoronoiDiagram Voronoi(Sites, Bounds, VoronoiBoundsPadding);
		FPlanarCells Cells(Sites, Voronoi);
		// The torn faces show the aircraft's own material; the engine's UV scale guess fails on some aircraft meshes.
		Cells.InternalSurfaceMaterials.GlobalMaterialID = 0;
		Cells.InternalSurfaceMaterials.GlobalUVScale = InteriorUVScale;
		// Islands stay with their cell: the loose panels and rivets of a cell come off as one piece.
		constexpr bool bIncludeOutsideCells = true;
		constexpr bool bSetInternalMaterialsFromCollection = false;
		constexpr bool bSplitIslands = false;
		const int32 FirstNewGeometry = CutMultipleWithPlanarCells(Cells, Collection, TransformsToCut, 0.0,
		                                                          CollisionSampleSpacing, FractureSeed,
		                                                          TOptional<FTransform>(), bIncludeOutsideCells,
		                                                          bSetInternalMaterialsFromCollection, nullptr,
		                                                          FVector::ZeroVector, bSplitIslands);
		// The uncut aircraft stays behind as the hidden parent of its pieces and still casts its shadow: drop it.
		const TArray<int32> CutTransforms = TransformsToCut;
		TArray<int32> ParentGeometry;
		for (const int32 TransformIndex : CutTransforms)
		{
			const int32 GeometryIndex = Collection.TransformToGeometryIndex[TransformIndex];
			if (GeometryIndex != INDEX_NONE && not Collection.Children[TransformIndex].IsEmpty())
			{
				ParentGeometry.Add(GeometryIndex);
			}
		}
		ParentGeometry.Sort();
		if (not ParentGeometry.IsEmpty())
		{
			Collection.RemoveElements(FGeometryCollection::GeometryGroup, ParentGeometry);
		}
		UE_LOG(LogRTSAircraftCrashCollection, Display, TEXT("Cut with %d cells: geometry now %d"), Sites.Num(),
		       GetTransformsWithGeometry(Collection).Num());
		return FirstNewGeometry != INDEX_NONE;
	}

	void ApplyCrashSettings(UGeometryCollection& CollectionAsset)
	{
		CollectionAsset.EnableNanite = false;
		CollectionAsset.bRemoveOnMaxSleep = true;
		CollectionAsset.MaximumSleepTime = SleepSecondsBeforeRemoval;
		CollectionAsset.RemovalDuration = RemovalSeconds;
		CollectionAsset.bScaleOnRemoval = true;
	}

	/** @return Whether the collection was built and saved. */
	bool BuildCrashCollection(const FString& AssetName, const TArray<FAircraftCrashMesh>& Meshes)
	{
		const FString PackageName = FString(FAircraftDeathMath::CrashCollectionFolder) / AssetName;
		// An existing package must be loaded in full, else saving over it is refused.
		UPackage* Package = FPackageName::DoesPackageExist(PackageName)
			                    ? LoadPackage(nullptr, *PackageName, LOAD_None)
			                    : CreatePackage(*PackageName);
		if (not IsValid(Package))
		{
			UE_LOG(LogRTSAircraftCrashCollection, Error, TEXT("Could not open package %s."), *PackageName);
			return false;
		}
		Package->FullyLoad();
		if (UGeometryCollection* Existing = FindObject<UGeometryCollection>(Package, *AssetName))
		{
			// Rebuilt in place, so references to it stay valid.
			Existing->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
		}
		UGeometryCollection* CollectionAsset = NewObject<UGeometryCollection>(
			Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
		for (const FAircraftCrashMesh& Mesh : Meshes)
		{
			if (Mesh.StaticMesh != nullptr)
			{
				FGeometryCollectionEngineConversion::AppendStaticMesh(Mesh.StaticMesh, Mesh.StaticMaterials,
				                                                      Mesh.RelativeTransform, CollectionAsset, false);
				continue;
			}
			FGeometryCollectionEngineConversion::AppendSkeletalMesh(Mesh.SkeletalMesh, Mesh.SkeletalComponent,
			                                                        Mesh.RelativeTransform, CollectionAsset, false);
		}
		FGeometryCollection& Collection = *CollectionAsset->GetGeometryCollection();
		Collection.ReindexMaterials();
		CollectionAsset->InitializeMaterials();
		if (not FractureCollection(Collection))
		{
			UE_LOG(LogRTSAircraftCrashCollection, Error, TEXT("%s: could not fracture the aircraft meshes."), *AssetName);
			return false;
		}
		GeometryCollectionAlgo::PrepareForSimulation(&Collection);
		ApplyCrashSettings(*CollectionAsset);
		CollectionAsset->UpdateGeometryDependentProperties();
		CollectionAsset->InvalidateCollection();
		CollectionAsset->RebuildRenderData();
		EnsureMaterialsSupportGeometryCollections(*CollectionAsset);
		CollectionAsset->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(CollectionAsset);

		int32 PieceCount = 0;
		for (int32 TransformIndex = 0; TransformIndex < Collection.Children.Num(); ++TransformIndex)
		{
			PieceCount += Collection.Children[TransformIndex].IsEmpty()
				&& Collection.TransformToGeometryIndex[TransformIndex] != INDEX_NONE ? 1 : 0;
		}
		const FString FileName = FPackageName::LongPackageNameToFilename(PackageName,
		                                                                 FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		const bool bSaved = UPackage::SavePackage(Package, CollectionAsset, *FileName, SaveArgs);
		UE_LOG(LogRTSAircraftCrashCollection, Display,
		       TEXT("RTS_AIRCRAFT_CRASH_COLLECTION built=%s pieces=%d transforms=%d saved=%d"), *PackageName,
		       PieceCount, Collection.Transform.Num(), bSaved);
		return bSaved;
	}

	TArray<UClass*> FindAircraftClasses()
	{
		TArray<UClass*> AircraftClasses;
		const IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		TArray<FAssetData> Assets;
		AssetRegistry.GetAssetsByPath(FName(AircraftBlueprintFolder), Assets, true);
		for (const FAssetData& Asset : Assets)
		{
			if (Asset.AssetClassPath != UBlueprint::StaticClass()->GetClassPathName())
			{
				continue;
			}
			const UBlueprint* Blueprint = Cast<UBlueprint>(Asset.GetAsset());
			UClass* GeneratedClass = IsValid(Blueprint) ? Blueprint->GeneratedClass.Get() : nullptr;
			if (IsValid(GeneratedClass) && GeneratedClass->IsChildOf(AAircraftMaster::StaticClass()))
			{
				AircraftClasses.Add(GeneratedClass);
			}
		}
		return AircraftClasses;
	}

	void BuildAircraftCrashCollections(const TArray<FString>& Arguments)
	{
		UWorld* EditorWorld = GEditor != nullptr ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (not IsValid(EditorWorld))
		{
			UE_LOG(LogRTSAircraftCrashCollection, Error, TEXT("No editor world to spawn the aircraft in."));
			return;
		}
		const bool bOverwrite = Arguments.Contains(OverwriteToken);
		FString NameFilter;
		for (const FString& Argument : Arguments)
		{
			NameFilter = Argument != OverwriteToken ? Argument : NameFilter;
		}
		TSet<FString> BuiltNames;
		int32 FailedCount = 0;
		for (UClass* AircraftClass : FindAircraftClasses())
		{
			if (not NameFilter.IsEmpty() && not AircraftClass->GetName().Contains(NameFilter))
			{
				continue;
			}
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.ObjectFlags = RF_Transient;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AActor* Aircraft = EditorWorld->SpawnActor<AActor>(AircraftClass, FTransform::Identity, SpawnParameters);
			if (not IsValid(Aircraft))
			{
				continue;
			}
			TArray<FAircraftCrashMesh> Meshes;
			TArray<FString> StaticNames;
			TArray<FString> SkeletalNames;
			GatherAircraftMeshes(*Aircraft, Meshes, StaticNames, SkeletalNames);
			const FString AssetName = FAircraftDeathMath::GetCrashCollectionAssetName(StaticNames, SkeletalNames);
			const FString PackageName = FString(FAircraftDeathMath::CrashCollectionFolder) / AssetName;
			const bool bAlreadyBuilt = BuiltNames.Contains(AssetName)
				|| (not bOverwrite && FPackageName::DoesPackageExist(PackageName));
			if (not AssetName.IsEmpty() && not bAlreadyBuilt)
			{
				BuiltNames.Add(AssetName);
				FailedCount += BuildCrashCollection(AssetName, Meshes) ? 0 : 1;
			}
			UE_LOG(LogRTSAircraftCrashCollection, Display, TEXT("RTS_AIRCRAFT_CRASH_COLLECTION aircraft=%s collection=%s"),
			       *AircraftClass->GetName(), *AssetName);
			Aircraft->Destroy();
		}
		UE_LOG(LogRTSAircraftCrashCollection, Display, TEXT("RTS_AIRCRAFT_CRASH_COLLECTION done built=%d failed=%d"),
		       BuiltNames.Num(), FailedCount);
	}

	FAutoConsoleCommand BuildAircraftCrashCollectionsCommand(
		TEXT("RTS.Aircraft.BuildCrashCollections"),
		TEXT("Builds the geometry collections shot-down aircraft break into, one per set of aircraft meshes, in ")
		TEXT("/Game/RTS_Survival/Blueprints/Aircraft/CrashCollections. Args: [AircraftClassNameFilter] [Overwrite]"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&BuildAircraftCrashCollections));
}

#endif // WITH_EDITOR
