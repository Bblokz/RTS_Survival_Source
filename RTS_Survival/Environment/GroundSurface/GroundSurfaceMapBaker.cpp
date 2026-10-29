// Copyright (C) Bas Blokzijl - All rights reserved.

// Editor-only: the console command RTS.GroundSurface.BakeSurfaceMap bakes which ground is where on the open map into
// <SurfaceMapFolder>/SurfaceMap_<MapName> (UGroundSurfaceMap), read by UGroundSurfaceSubsystem at runtime.
// Landscape paint-layer weights only exist in the editor and the layers carry no physical materials, so a runtime
// trace cannot tell grass from sand. Roads and paths are meshes on top of the landscape: components matched by the
// mesh rules or a GroundSurface_<Type> actor tag override the landscape where they lie. Bake again after repainting
// a map or moving its roads.

#if WITH_EDITOR

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CoreMinimal.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Landscape.h"
#include "LandscapeComponent.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "LandscapeLayerInfoObject.h"
#include "Engine/StaticMesh.h"
#include "Misc/FileHelper.h"
#include "NiagaraSystem.h"
#include "NiagaraTypes.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Environment/GroundSurface/GroundSurfaceMap.h"
#include "RTS_Survival/Environment/GroundSurface/RTSGroundSurfaceSettings.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSSurfaceMapBake, Log, All);

namespace GroundSurfaceMapBaker
{
	// About 2000 x 2000 cells; a bigger landscape gets coarser cells instead of a huge map.
	constexpr int32 MaxCells = 4 * 1024 * 1024;
	constexpr float TraceUp = 50000.f;
	constexpr float TraceDown = 100000.f;
	constexpr int32 SurfaceCount = static_cast<int32>(EGroundSurfaceType::Num);
	// Size of the buckets the surface meshes are sorted into, so a sample only tests the meshes near it.
	constexpr float MeshBucketSize = 5000.f;
	const FString SurfaceTagPrefix = TEXT("GroundSurface_");
	// Samples per cell: the centre and four points a quarter cell away from it.
	const FVector2D SampleOffsets[] = {
		FVector2D(0.0, 0.0), FVector2D(-0.25, -0.25), FVector2D(0.25, -0.25), FVector2D(-0.25, 0.25), FVector2D(0.25, 0.25)
	};

	using FSurfaceWeights = TStaticArray<float, SurfaceCount>;

	// Decoded weightmaps per component and layer: GetLayerWeightAtLocation decodes the whole weightmap otherwise.
	using FWeightCache = TMap<TPair<const ULandscapeComponent*, const ULandscapeLayerInfoObject*>, TArray<uint8>>;

	struct FResolvedMeshRule
	{
		const UClass* ActorClass = nullptr;
		const UStaticMesh* Mesh = nullptr;
		EGroundSurfaceType SurfaceType = EGroundSurfaceType::None;
	};

	// A mesh component that decides the surface where it lies; its collision is made queryable for the bake only.
	struct FSurfaceMeshCandidate
	{
		TWeakObjectPtr<UStaticMeshComponent> Component;
		EGroundSurfaceType SurfaceType = EGroundSurfaceType::None;
		FBox2D Bounds = FBox2D(ForceInit);
		ECollisionEnabled::Type OriginalCollision = ECollisionEnabled::NoCollision;
		bool bChangedCollision = false;
	};

	struct FBakeContext
	{
		UWorld* World = nullptr;
		const URTSGroundSurfaceSettings* Settings = nullptr;
		float TopZ = 0.f;
		FWeightCache WeightCache;
		TMap<const ULandscapeLayerInfoObject*, EGroundSurfaceType> LayerSurfaces;
		TSet<FName> UnlistedLayers;
		TArray<FSurfaceMeshCandidate> MeshCandidates;
		TMap<FIntPoint, TArray<int32>> MeshBuckets;
	};

	const TCHAR* GetSurfaceName(const EGroundSurfaceType SurfaceType)
	{
		switch (SurfaceType)
		{
		case EGroundSurfaceType::Dirt: return TEXT("Dirt");
		case EGroundSurfaceType::Grass: return TEXT("Grass");
		case EGroundSurfaceType::Sand: return TEXT("Sand");
		case EGroundSurfaceType::Rock: return TEXT("Rock");
		case EGroundSurfaceType::Concrete: return TEXT("Concrete");
		default: return TEXT("None");
		}
	}

	EGroundSurfaceType GetLayerSurface(FBakeContext& Context, const ULandscapeLayerInfoObject* LayerInfo)
	{
		if (const EGroundSurfaceType* Known = Context.LayerSurfaces.Find(LayerInfo))
		{
			return *Known;
		}
		EGroundSurfaceType LayerSurface = Context.Settings->UnlistedLayerSurface;
		const FGroundSurfaceDefinition* ListingDefinition = Context.Settings->Surfaces.FindByPredicate(
			[LayerInfo](const FGroundSurfaceDefinition& Definition)
			{
				return Definition.LayerNames.Contains(LayerInfo->LayerName);
			});
		if (ListingDefinition != nullptr)
		{
			LayerSurface = ListingDefinition->SurfaceType;
		}
		else
		{
			Context.UnlistedLayers.Add(LayerInfo->LayerName);
		}
		Context.LayerSurfaces.Add(LayerInfo, LayerSurface);
		return LayerSurface;
	}

	/**
	 * @brief Traces onto the landscape and adds the weights of its paint layers to their surfaces; the part no layer
	 * covers counts as the unlisted-layer surface.
	 * @param Context Bake state and caches.
	 * @param Point World XY to sample.
	 * @param InOutWeights Receives the layer weights, 0 to 1 in total.
	 * @param OutLandscapeZ Height of the landscape at the point, when there is landscape.
	 * @return Whether there is landscape under the point.
	 */
	bool AddLandscapeWeights(FBakeContext& Context, const FVector2D& Point, FSurfaceWeights& InOutWeights,
	                         double& OutLandscapeZ)
	{
		FHitResult Hit;
		const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GroundSurfaceMapBake), false);
		const FVector Start(Point, Context.TopZ + TraceUp);
		const FVector End(Point, Context.TopZ - TraceDown);
		if (not Context.World->LineTraceSingleByChannel(Hit, Start, End, COLLISION_TRACE_LANDSCAPE, QueryParams))
		{
			return false;
		}
		const ULandscapeHeightfieldCollisionComponent* Collision =
			Cast<ULandscapeHeightfieldCollisionComponent>(Hit.GetComponent());
		ULandscapeComponent* Landscape = Collision != nullptr ? Collision->GetRenderComponent() : nullptr;
		if (Landscape == nullptr)
		{
			return false;
		}
		OutLandscapeZ = Hit.ImpactPoint.Z;
		float TotalWeight = 0.f;
		for (const FWeightmapLayerAllocationInfo& Allocation : Landscape->GetWeightmapLayerAllocations())
		{
			ULandscapeLayerInfoObject* LayerInfo = Allocation.LayerInfo;
			if (LayerInfo == nullptr)
			{
				continue;
			}
			TArray<uint8>& LayerCache = Context.WeightCache.FindOrAdd(
				TPair<const ULandscapeComponent*, const ULandscapeLayerInfoObject*>(Landscape, LayerInfo));
			const float LayerWeight = Landscape->GetLayerWeightAtLocation(Hit.ImpactPoint, LayerInfo, &LayerCache);
			InOutWeights[static_cast<int32>(GetLayerSurface(Context, LayerInfo))] += LayerWeight;
			TotalWeight += LayerWeight;
		}
		InOutWeights[static_cast<int32>(Context.Settings->UnlistedLayerSurface)] += FMath::Max(0.f, 1.f - TotalWeight);
		return true;
	}

	TArray<FResolvedMeshRule> ResolveMeshRules(const URTSGroundSurfaceSettings& Settings)
	{
		TArray<FResolvedMeshRule> Rules;
		for (const FGroundSurfaceMeshRule& Rule : Settings.MeshRules)
		{
			FResolvedMeshRule Resolved;
			Resolved.ActorClass = Rule.ActorClass.IsNull() ? nullptr : Rule.ActorClass.LoadSynchronous();
			Resolved.Mesh = Rule.Mesh.IsNull() ? nullptr : Rule.Mesh.LoadSynchronous();
			Resolved.SurfaceType = Rule.SurfaceType;
			const bool bFailedToLoad = (not Rule.ActorClass.IsNull() && Resolved.ActorClass == nullptr)
				|| (not Rule.Mesh.IsNull() && Resolved.Mesh == nullptr);
			if (bFailedToLoad || (Resolved.ActorClass == nullptr && Resolved.Mesh == nullptr))
			{
				UE_LOG(LogRTSSurfaceMapBake, Warning, TEXT("Skipping mesh rule %s / %s: nothing to match."),
				       *Rule.ActorClass.ToString(), *Rule.Mesh.ToString());
				continue;
			}
			Rules.Add(Resolved);
		}
		return Rules;
	}

	bool TryGetTaggedSurface(const AActor& Actor, EGroundSurfaceType& OutSurfaceType)
	{
		const UEnum* SurfaceEnum = StaticEnum<EGroundSurfaceType>();
		for (const FName& Tag : Actor.Tags)
		{
			const FString TagString = Tag.ToString();
			if (not TagString.StartsWith(SurfaceTagPrefix))
			{
				continue;
			}
			const int64 SurfaceValue = SurfaceEnum->GetValueByNameString(TagString.RightChop(SurfaceTagPrefix.Len()));
			if (SurfaceValue == INDEX_NONE || SurfaceValue >= SurfaceCount)
			{
				UE_LOG(LogRTSSurfaceMapBake, Warning, TEXT("Actor %s has unknown surface tag %s."), *Actor.GetActorLabel(),
				       *TagString);
				continue;
			}
			OutSurfaceType = static_cast<EGroundSurfaceType>(SurfaceValue);
			return true;
		}
		return false;
	}

	EGroundSurfaceType GetRuleSurface(const AActor& Actor, const UStaticMeshComponent& Component,
	                                  const TArray<FResolvedMeshRule>& Rules)
	{
		for (const FResolvedMeshRule& Rule : Rules)
		{
			const bool bActorMatches = Rule.ActorClass == nullptr || Actor.IsA(Rule.ActorClass);
			const bool bMeshMatches = Rule.Mesh == nullptr || Component.GetStaticMesh() == Rule.Mesh;
			if (bActorMatches && bMeshMatches)
			{
				return Rule.SurfaceType;
			}
		}
		return EGroundSurfaceType::None;
	}

	void AddMeshCandidate(FBakeContext& Context, UStaticMeshComponent& Component, const EGroundSurfaceType SurfaceType)
	{
		if (SurfaceType == EGroundSurfaceType::None || not Component.IsRegistered() || Component.bHiddenInGame
			|| Component.GetStaticMesh() == nullptr)
		{
			return;
		}
		FSurfaceMeshCandidate Candidate;
		Candidate.Component = &Component;
		Candidate.SurfaceType = SurfaceType;
		const FBox WorldBounds = Component.Bounds.GetBox();
		Candidate.Bounds = FBox2D(FVector2D(WorldBounds.Min), FVector2D(WorldBounds.Max));
		Candidate.OriginalCollision = Component.GetCollisionEnabled();
		const int32 CandidateIndex = Context.MeshCandidates.Add(Candidate);

		const FIntPoint MinBucket(FMath::FloorToInt32(Candidate.Bounds.Min.X / MeshBucketSize),
		                          FMath::FloorToInt32(Candidate.Bounds.Min.Y / MeshBucketSize));
		const FIntPoint MaxBucket(FMath::FloorToInt32(Candidate.Bounds.Max.X / MeshBucketSize),
		                          FMath::FloorToInt32(Candidate.Bounds.Max.Y / MeshBucketSize));
		for (int32 BucketY = MinBucket.Y; BucketY <= MaxBucket.Y; ++BucketY)
		{
			for (int32 BucketX = MinBucket.X; BucketX <= MaxBucket.X; ++BucketX)
			{
				Context.MeshBuckets.FindOrAdd(FIntPoint(BucketX, BucketY)).Add(CandidateIndex);
			}
		}
	}

	void CollectSurfaceMeshes(FBakeContext& Context)
	{
		const TArray<FResolvedMeshRule> Rules = ResolveMeshRules(*Context.Settings);
		for (TActorIterator<AActor> It(Context.World); It; ++It)
		{
			const AActor* Actor = *It;
			if (Actor->IsA<ALandscapeProxy>())
			{
				continue;
			}
			EGroundSurfaceType TaggedSurface = EGroundSurfaceType::None;
			const bool bIsTagged = TryGetTaggedSurface(*Actor, TaggedSurface);
			TInlineComponentArray<UStaticMeshComponent*> MeshComponents(Actor);
			for (UStaticMeshComponent* MeshComponent : MeshComponents)
			{
				const EGroundSurfaceType SurfaceType = bIsTagged ? TaggedSurface : GetRuleSurface(*Actor, *MeshComponent, Rules);
				AddMeshCandidate(Context, *MeshComponent, SurfaceType);
			}
		}
	}

	// Surface meshes often have no collision (they only draw on the landscape); the bake traces them one by one.
	void MakeSurfaceMeshesQueryable(FBakeContext& Context)
	{
		for (FSurfaceMeshCandidate& Candidate : Context.MeshCandidates)
		{
			UStaticMeshComponent* Component = Candidate.Component.Get();
			if (Component == nullptr || CollisionEnabledHasQuery(Candidate.OriginalCollision))
			{
				continue;
			}
			Component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			if (USplineMeshComponent* SplineMesh = Cast<USplineMeshComponent>(Component))
			{
				SplineMesh->RecreateCollision();
			}
			Component->RecreatePhysicsState();
			Candidate.bChangedCollision = true;
		}
	}

	void RestoreSurfaceMeshCollision(FBakeContext& Context)
	{
		for (const FSurfaceMeshCandidate& Candidate : Context.MeshCandidates)
		{
			UStaticMeshComponent* Component = Candidate.Component.Get();
			if (Component == nullptr || not Candidate.bChangedCollision)
			{
				continue;
			}
			Component->SetCollisionEnabled(Candidate.OriginalCollision);
			Component->RecreatePhysicsState();
		}
	}

	/**
	 * @brief Traces the surface meshes near a point; the topmost one hit decides the surface there.
	 * @param Context Bake state with the sorted surface meshes.
	 * @param Point World XY to sample.
	 * @param MaxHitZ Hits above this are overhead (wires, gantries) and ignored.
	 * @return The surface of the topmost mesh, or None where no surface mesh lies.
	 */
	EGroundSurfaceType TraceSurfaceMeshes(const FBakeContext& Context, const FVector2D& Point, const double MaxHitZ)
	{
		const FIntPoint Bucket(FMath::FloorToInt32(Point.X / MeshBucketSize), FMath::FloorToInt32(Point.Y / MeshBucketSize));
		const TArray<int32>* CandidateIndices = Context.MeshBuckets.Find(Bucket);
		if (CandidateIndices == nullptr)
		{
			return EGroundSurfaceType::None;
		}
		const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GroundSurfaceMapBakeMesh), true);
		const FVector Start(Point, Context.TopZ + TraceUp);
		const FVector End(Point, Context.TopZ - TraceDown);
		EGroundSurfaceType TopSurface = EGroundSurfaceType::None;
		double TopHitZ = -UE_BIG_NUMBER;
		for (const int32 CandidateIndex : *CandidateIndices)
		{
			const FSurfaceMeshCandidate& Candidate = Context.MeshCandidates[CandidateIndex];
			UStaticMeshComponent* Component = Candidate.Component.Get();
			if (Component == nullptr || not Candidate.Bounds.IsInside(Point))
			{
				continue;
			}
			FHitResult Hit;
			if (Component->LineTraceComponent(Hit, Start, End, QueryParams) && Hit.ImpactPoint.Z > TopHitZ
				&& Hit.ImpactPoint.Z <= MaxHitZ)
			{
				TopHitZ = Hit.ImpactPoint.Z;
				TopSurface = Candidate.SurfaceType;
			}
		}
		return TopSurface;
	}

	// The surface that covers most of the cell; None where neither landscape nor a surface mesh lies.
	EGroundSurfaceType BakeCell(FBakeContext& Context, const FVector2D& CellCentre, const float CellSize)
	{
		FSurfaceWeights Weights(InPlace, 0.f);
		bool bHasGround = false;
		for (const FVector2D& Offset : SampleOffsets)
		{
			const FVector2D SamplePoint = CellCentre + Offset * CellSize;
			FSurfaceWeights LandscapeWeights(InPlace, 0.f);
			double LandscapeZ = 0.0;
			const bool bHitLandscape = AddLandscapeWeights(Context, SamplePoint, LandscapeWeights, LandscapeZ);
			const double MaxMeshZ = bHitLandscape
				                        ? LandscapeZ + Context.Settings->MaxSurfaceMeshHeightAboveLandscape
				                        : UE_BIG_NUMBER;
			const EGroundSurfaceType MeshSurface = TraceSurfaceMeshes(Context, SamplePoint, MaxMeshZ);
			if (MeshSurface != EGroundSurfaceType::None)
			{
				Weights[static_cast<int32>(MeshSurface)] += 1.f;
				bHasGround = true;
				continue;
			}
			for (int32 SurfaceIndex = 0; SurfaceIndex < SurfaceCount; ++SurfaceIndex)
			{
				Weights[SurfaceIndex] += LandscapeWeights[SurfaceIndex];
			}
			bHasGround |= bHitLandscape;
		}
		if (not bHasGround)
		{
			return EGroundSurfaceType::None;
		}
		int32 BestSurface = static_cast<int32>(EGroundSurfaceType::None);
		for (int32 SurfaceIndex = 1; SurfaceIndex < SurfaceCount; ++SurfaceIndex)
		{
			if (Weights[SurfaceIndex] > Weights[BestSurface])
			{
				BestSurface = SurfaceIndex;
			}
		}
		return static_cast<EGroundSurfaceType>(BestSurface);
	}

	UGroundSurfaceMap* FindOrCreateSurfaceMapAsset(const FSoftObjectPath& MapPath)
	{
		if (UGroundSurfaceMap* Existing = Cast<UGroundSurfaceMap>(MapPath.TryLoad()))
		{
			return Existing;
		}
		UPackage* Package = CreatePackage(*MapPath.GetLongPackageName());
		UGroundSurfaceMap* SurfaceMap = NewObject<UGroundSurfaceMap>(
			Package, FName(*MapPath.GetAssetName()), RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(SurfaceMap);
		return SurfaceMap;
	}

	FColor GetPreviewColor(const EGroundSurfaceType SurfaceType)
	{
		switch (SurfaceType)
		{
		case EGroundSurfaceType::Dirt: return FColor(120, 85, 55);
		case EGroundSurfaceType::Grass: return FColor(70, 130, 50);
		case EGroundSurfaceType::Sand: return FColor(235, 170, 80);
		case EGroundSurfaceType::Rock: return FColor(110, 110, 120);
		case EGroundSurfaceType::Concrete: return FColor(220, 220, 220);
		default: return FColor::Black;
		}
	}

	// A picture of the surface map in Saved/GroundSurface, +Y up as in the top-down editor view.
	void WritePreview(const UGroundSurfaceMap& SurfaceMap, const FString& MapName)
	{
		TArray<FColor> Pixels;
		Pixels.SetNum(SurfaceMap.SurfaceCells.Num());
		for (int32 CellY = 0; CellY < SurfaceMap.CellCount.Y; ++CellY)
		{
			for (int32 CellX = 0; CellX < SurfaceMap.CellCount.X; ++CellX)
			{
				const uint8 SurfaceValue = SurfaceMap.SurfaceCells[CellY * SurfaceMap.CellCount.X + CellX];
				Pixels[(SurfaceMap.CellCount.Y - 1 - CellY) * SurfaceMap.CellCount.X + CellX] =
					GetPreviewColor(static_cast<EGroundSurfaceType>(SurfaceValue));
			}
		}
		const FString PreviewPath = FPaths::ProjectSavedDir() / TEXT("GroundSurface") / (TEXT("SurfaceMap_") + MapName + TEXT(".bmp"));
		FFileHelper::CreateBitmap(*PreviewPath, SurfaceMap.CellCount.X, SurfaceMap.CellCount.Y, Pixels.GetData());
	}

	void LogBakeResult(const FBakeContext& Context, const UGroundSurfaceMap& SurfaceMap, const FString& MapName,
	                   const bool bIsSaved, const FSoftObjectPath& MapPath)
	{
		TStaticArray<int32, SurfaceCount> CellsPerSurface(InPlace, 0);
		for (const uint8 SurfaceValue : SurfaceMap.SurfaceCells)
		{
			++CellsPerSurface[FMath::Min<int32>(SurfaceValue, SurfaceCount - 1)];
		}
		TStaticArray<int32, SurfaceCount> MeshesPerSurface(InPlace, 0);
		for (const FSurfaceMeshCandidate& Candidate : Context.MeshCandidates)
		{
			++MeshesPerSurface[static_cast<int32>(Candidate.SurfaceType)];
		}
		FString Counts;
		for (int32 SurfaceIndex = 0; SurfaceIndex < SurfaceCount; ++SurfaceIndex)
		{
			Counts += FString::Printf(TEXT("%s=%d(meshes %d) "), GetSurfaceName(static_cast<EGroundSurfaceType>(SurfaceIndex)),
			                          CellsPerSurface[SurfaceIndex], MeshesPerSurface[SurfaceIndex]);
		}
		// Which actor classes and meshes became surface meshes, to check the rules against the map.
		TMap<FString, int32> MeshKinds;
		for (const FSurfaceMeshCandidate& Candidate : Context.MeshCandidates)
		{
			const UStaticMeshComponent* Component = Candidate.Component.Get();
			if (Component == nullptr)
			{
				continue;
			}
			const FString Kind = FString::Printf(TEXT("%s:%s:%s"), GetSurfaceName(Candidate.SurfaceType),
			                                     *Component->GetOwner()->GetClass()->GetName(),
			                                     *GetNameSafe(Component->GetStaticMesh()));
			++MeshKinds.FindOrAdd(Kind);
		}
		for (const TPair<FString, int32>& MeshKind : MeshKinds)
		{
			UE_LOG(LogRTSSurfaceMapBake, Display, TEXT("RTS_SURFACE_MAP MESH %s x%d"), *MeshKind.Key, MeshKind.Value);
		}
		FString Unlisted;
		for (const FName& LayerName : Context.UnlistedLayers)
		{
			Unlisted += LayerName.ToString() + TEXT(" ");
		}
		UE_LOG(LogRTSSurfaceMapBake, Display,
		       TEXT("RTS_SURFACE_MAP RESULT DONE map=%s grid=%dx%d cell=%.0f %s unlisted_layers=[%s] saved=%d asset=%s"),
		       *MapName, SurfaceMap.CellCount.X, SurfaceMap.CellCount.Y, SurfaceMap.CellSize, *Counts.TrimEnd(),
		       *Unlisted.TrimEnd(), bIsSaved ? 1 : 0, *MapPath.ToString());
	}

	void BakeSurfaceMap()
	{
		FBakeContext Context;
		Context.World = GEditor != nullptr ? GEditor->GetEditorWorldContext().World() : nullptr;
		Context.Settings = URTSGroundSurfaceSettings::Get();
		if (Context.World == nullptr || Context.Settings == nullptr)
		{
			UE_LOG(LogRTSSurfaceMapBake, Error, TEXT("RTS_SURFACE_MAP RESULT FAIL no editor world"));
			return;
		}
		FBox LandscapeBounds(ForceInit);
		for (TActorIterator<ALandscapeProxy> It(Context.World); It; ++It)
		{
			LandscapeBounds += It->GetComponentsBoundingBox(true);
		}
		const FString MapName = FPackageName::GetShortName(Context.World->GetOutermost());
		if (not LandscapeBounds.IsValid)
		{
			UE_LOG(LogRTSSurfaceMapBake, Error, TEXT("RTS_SURFACE_MAP RESULT FAIL map %s has no landscape"), *MapName);
			return;
		}
		Context.TopZ = LandscapeBounds.Max.Z;
		const FVector2D BoundsSize(LandscapeBounds.GetSize());
		const float CellSize = FMath::Max(Context.Settings->CellSize, FMath::Sqrt(BoundsSize.X * BoundsSize.Y / MaxCells));
		const FIntPoint CellCount(
			FMath::Max(1, FMath::CeilToInt(BoundsSize.X / CellSize)),
			FMath::Max(1, FMath::CeilToInt(BoundsSize.Y / CellSize)));
		const FVector2D Origin(LandscapeBounds.Min);

		CollectSurfaceMeshes(Context);
		MakeSurfaceMeshesQueryable(Context);
		TArray<uint8> SurfaceCells;
		SurfaceCells.SetNumZeroed(CellCount.X * CellCount.Y);
		for (int32 CellY = 0; CellY < CellCount.Y; ++CellY)
		{
			for (int32 CellX = 0; CellX < CellCount.X; ++CellX)
			{
				const FVector2D CellCentre = Origin + (FVector2D(CellX, CellY) + 0.5) * CellSize;
				SurfaceCells[CellY * CellCount.X + CellX] = static_cast<uint8>(BakeCell(Context, CellCentre, CellSize));
			}
		}
		RestoreSurfaceMeshCollision(Context);

		const FSoftObjectPath MapPath = Context.Settings->GetSurfaceMapPath(MapName);
		UGroundSurfaceMap* SurfaceMap = FindOrCreateSurfaceMapAsset(MapPath);
		SurfaceMap->Modify();
		SurfaceMap->Origin = Origin;
		SurfaceMap->CellCount = CellCount;
		SurfaceMap->CellSize = CellSize;
		SurfaceMap->SurfaceCells = MoveTemp(SurfaceCells);
		SurfaceMap->MarkPackageDirty();
		WritePreview(*SurfaceMap, MapName);
		const bool bIsSaved = UEditorLoadingAndSavingUtils::SavePackages({SurfaceMap->GetOutermost()}, false);
		LogBakeResult(Context, *SurfaceMap, MapName, bIsSaved, MapPath);
	}

	// The track smoke system's default look must be the old tan: vehicles whose code never writes it (or an editor on
	// an older DLL) would otherwise draw black or invisible smoke.
	void LogTrackSmokeDefaultLook()
	{
		const TCHAR* const SystemPath =
			TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/TrackSmoke/NS_Track_Smokes.NS_Track_Smokes");
		const UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, SystemPath);
		if (System == nullptr)
		{
			UE_LOG(LogRTSSurfaceMapBake, Error, TEXT("RTS_SMOKE_DEFAULT RESULT FAIL cannot load %s"), SystemPath);
			return;
		}
		const FNiagaraVariable LookVariable(FNiagaraTypeDefinition::GetVec4Def(), TEXT("User.SmokeSurfaceLook"));
		const int32 Offset = System->GetExposedParameters().IndexOf(LookVariable);
		if (Offset == INDEX_NONE)
		{
			UE_LOG(LogRTSSurfaceMapBake, Error, TEXT("RTS_SMOKE_DEFAULT RESULT FAIL no Vector4 User.SmokeSurfaceLook"));
			return;
		}
		const FVector4f DefaultLook = System->GetExposedParameters().GetParameterValue<FVector4f>(LookVariable);
		UE_LOG(LogRTSSurfaceMapBake, Display, TEXT("RTS_SMOKE_DEFAULT RESULT DONE User.SmokeSurfaceLook=(%.3f %.3f %.3f %.3f)"),
		       DefaultLook.X, DefaultLook.Y, DefaultLook.Z, DefaultLook.W);
	}

	FAutoConsoleCommand LogTrackSmokeDefaultCommand(
		TEXT("RTS.GroundSurface.LogSmokeDefault"),
		TEXT("Logs the default User.SmokeSurfaceLook of NS_Track_Smokes (should be the old tan, alpha 1)."),
		FConsoleCommandDelegate::CreateStatic(&LogTrackSmokeDefaultLook));

	FAutoConsoleCommand BakeSurfaceMapCommand(
		TEXT("RTS.GroundSurface.BakeSurfaceMap"),
		TEXT("Bakes which ground is where on the open map (landscape paint layers and surface meshes in the RTS Ground Surfaces settings) into SurfaceMap_<MapName>."),
		FConsoleCommandDelegate::CreateStatic(&BakeSurfaceMap));
}

#endif // WITH_EDITOR
