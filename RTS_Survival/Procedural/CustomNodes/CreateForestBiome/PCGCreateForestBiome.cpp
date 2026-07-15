// Copyright (C) CoreMinimal Software, Bas Blokzijl - All rights reserved.
#include "PCGCreateForestBiome.h"

#include "ForestBiomeGenerator.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGManagedResource.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"
#include "Data/PCGPolyLineData.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"

#include "RTS_Survival/RTSCollisionTraceChannels.h"

#include "CollisionQueryParams.h"
#include "Components/DecalComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"

#define LOCTEXT_NAMESPACE "PCGCreateForestBiome"

namespace ForestBiomePCGConstants
{
	const FName ExclusionPinLabel = TEXT("Exclusion");
	const FName FoliagePinLabel = TEXT("Foliage");
	const FName TreesPinLabel = TEXT("Trees");
	const FName FoliageMeshAttributeName = TEXT("Mesh");

	// Actors are inspected far below the world so their real bounds and meshes can be measured
	// without disturbing the level.
	constexpr double MeasureSpawnDepth = -100000.0;

	// Ground alignment trace range; generous so tall landscapes still hit.
	constexpr double GroundTraceUp = 10000.0;
	constexpr double GroundTraceDown = 30000.0;

	// Fallback / floor clearance radii (cm) when bounds cannot be measured.
	constexpr double DefaultPropRadius = 200.0;
	constexpr double DefaultAuxiliaryRadius = 150.0;
	constexpr double MinPropRadius = 10.0;
	constexpr double MinFoliageRadius = 5.0;
	constexpr double MinDecalDimension = 1.0;

	// Half-size of the probe used when sampling the exclusion / area spatial data.
	constexpr double AreaSampleHalfExtent = 50.0;

	// Step (cm) at which input spline loops are sampled into a polygon for the area test.
	constexpr double SplineSampleStep = 500.0;
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

#if WITH_EDITOR
FName UPCGCreateForestBiomeSettings::GetDefaultNodeName() const
{
	return FName(TEXT("CreateForestBiome"));
}

FText UPCGCreateForestBiomeSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Create Forest Biome");
}

FText UPCGCreateForestBiomeSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Scatters a forest inside the input spawn volume: large trees, regular trees, bushes, foliage, "
		"ground decals and auxiliary props (rocks etc.), each with its own count per 1000x1000 unit tile. Trees spawn "
		"as standalone Blueprint actors; bushes and instanced auxiliaries are batched into Hierarchical "
		"ISM actors. Trees outputs the spawned tree locations with measured bounds; foliage is emitted on "
		"the Foliage output pin for a GPU Static Mesh Spawner (mesh selection 'By Attribute', attribute "
		"'Mesh'). Categories are placed largest-first into a shared occupancy grid so nothing overlaps; "
		"larger auxiliaries end up more isolated than smaller ones. Nothing is ever placed inside the "
		"Exclusion input. Deterministic from RandomSeed.");
}
#endif

FPCGElementPtr UPCGCreateForestBiomeSettings::CreateElement() const
{
	return MakeShared<FPCGCreateForestBiomeElement>();
}

TArray<FPCGPinProperties> UPCGCreateForestBiomeSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	// The spawn volume (a PCG volume, surface or closed spline loop); only the first data is used.
	Pins.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Spatial).SetRequiredPin();
	// Optional excluded area: nothing is placed where a volume/surface samples > 0, nor inside the
	// footprint of any exclusion point's bounds (e.g. the Radixite node's flat "Field Bounds" output).
	Pins.Emplace(ForestBiomePCGConstants::ExclusionPinLabel, EPCGDataType::Spatial);
	return Pins;
}

TArray<FPCGPinProperties> UPCGCreateForestBiomeSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Pins;
	// Tree points retain each spawned tree's transform and measured local bounds for downstream consumers.
	Pins.Emplace(ForestBiomePCGConstants::TreesPinLabel, EPCGDataType::Point);
	// Foliage points carry a "Mesh" attribute; wire into a Static Mesh Spawner (By Attribute).
	Pins.Emplace(ForestBiomePCGConstants::FoliagePinLabel, EPCGDataType::Point);
	return Pins;
}

// ---------------------------------------------------------------------------
// Asset resolving
// ---------------------------------------------------------------------------

namespace
{
	/** @brief One static mesh of a Blueprint, relative to the actor's pivot. */
	struct FForestMeshInstance
	{
		UStaticMesh* Mesh = nullptr;
		FTransform RelativeTransform = FTransform::Identity;
	};

	/** @brief Runtime-resolved assets, parallel to the generator's resolved parameter arrays. */
	struct FForestBiomeAssets
	{
		// Parallel to FForestBiomeGenParams::LargeTrees / RegularTrees.
		TArray<UClass*> LargeTreeClasses;
		TArray<UClass*> RegularTreeClasses;
		TArray<FBox> LargeTreeLocalBounds;
		TArray<FBox> RegularTreeLocalBounds;

		// Parallel to FForestBiomeGenParams::Bushes. Classes are only used as a fallback when a
		// bush Blueprint has no static meshes to instance.
		TArray<UClass*> BushClasses;
		TArray<TArray<FForestMeshInstance>> BushMeshInstances;

		// Parallel to FForestBiomeGenParams::Foliage.
		TArray<FSoftObjectPath> FoliageMeshPaths;

		// Parallel to FForestBiomeGenParams::Decals.
		TArray<UMaterialInterface*> DecalMaterials;

		// Parallel to FForestBiomeGenParams::Auxiliaries.
		TArray<UClass*> AuxiliaryClasses;
		TArray<EForestAuxiliarySpawnType> AuxiliarySpawnTypes;
		TArray<TArray<FForestMeshInstance>> AuxiliaryMeshInstances;
	};

	/**
	 * @brief Spawns a transient instance of a class far below the world so its real bounds and static
	 * mesh composition can be inspected; exact for Blueprints incl. construction scripts.
	 * The caller must Destroy() the returned actor.
	 */
	AActor* SpawnTransientInspectionActor(UWorld& World, UClass& ActorClass)
	{
		const FVector SpawnLocation(0.0, 0.0, ForestBiomePCGConstants::MeasureSpawnDepth);

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.ObjectFlags |= RF_Transient;

		return World.SpawnActor<AActor>(&ActorClass, FTransform(SpawnLocation), SpawnParams);
	}

	FBox ComputeInspectionActorLocalBounds(const AActor& InspectionActor)
	{
		const FBox WorldBounds = InspectionActor.GetComponentsBoundingBox(true, true);
		if (not WorldBounds.IsValid)
		{
			return FBox(EForceInit::ForceInit);
		}
		return WorldBounds.ShiftBy(-InspectionActor.GetActorLocation());
	}

	/** @return XY clearance radius (cm) of a local bounds box, floored to a small minimum. */
	double RadiusFromBounds(const FBox& LocalBounds)
	{
		const FVector Extent = LocalBounds.GetExtent();
		return FMath::Max(ForestBiomePCGConstants::MinPropRadius, FMath::Max(Extent.X, Extent.Y));
	}

	/**
	 * @brief Collects every static mesh of the actor (plain components and ISM/HISM instances) with
	 * its transform relative to the actor pivot, so bushes and instanced auxiliaries can be re-rendered
	 * as shared Hierarchical ISM instances instead of per-prop actors.
	 */
	void ExtractStaticMeshInstances(const AActor& InspectionActor, TArray<FForestMeshInstance>& OutInstances)
	{
		const FTransform WorldToActor = InspectionActor.GetActorTransform().Inverse();

		TArray<UStaticMeshComponent*> MeshComponents;
		InspectionActor.GetComponents<UStaticMeshComponent>(MeshComponents);

		for (const UStaticMeshComponent* MeshComponent : MeshComponents)
		{
			UStaticMesh* Mesh = MeshComponent->GetStaticMesh();
			if (Mesh == nullptr)
			{
				continue;
			}

			if (const UInstancedStaticMeshComponent* Instanced = Cast<UInstancedStaticMeshComponent>(MeshComponent))
			{
				for (int32 InstanceIndex = 0; InstanceIndex < Instanced->GetInstanceCount(); ++InstanceIndex)
				{
					FTransform InstanceTransform;
					if (Instanced->GetInstanceTransform(InstanceIndex, InstanceTransform, true))
					{
						OutInstances.Add({Mesh, InstanceTransform * WorldToActor});
					}
				}
				continue;
			}

			OutInstances.Add({Mesh, MeshComponent->GetComponentTransform() * WorldToActor});
		}
	}

	/**
	 * @brief Resolves one Blueprint prop list (large trees, regular trees or bushes) into generator
	 * entries with measured clearance radii.
	 * @param OutMeshInstances When non-null, each entry's static meshes are extracted for instancing
	 * (bushes); pass null for tree lists that spawn as actors.
	 */
	void ResolveProps(
		FPCGContext* Context,
		UWorld& World,
		const TArray<FForestBlueprintEntry>& Entries,
		TArray<FForestResolvedProp>& OutResolved,
		TArray<UClass*>& OutClasses,
		TArray<TArray<FForestMeshInstance>>* OutMeshInstances,
		TArray<FBox>* OutLocalBounds)
	{
		const bool bExtractMeshes = OutMeshInstances != nullptr;

		for (int32 SettingsIndex = 0; SettingsIndex < Entries.Num(); ++SettingsIndex)
		{
			const FForestBlueprintEntry& Entry = Entries[SettingsIndex];
			UClass* PropClass = Entry.BlueprintClass.LoadSynchronous();
			if (PropClass == nullptr)
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context,
					LOCTEXT("NullPropClass", "CreateForestBiome: a tree/bush entry has no valid class and was skipped."));
				continue;
			}

			FForestResolvedProp Resolved;
			Resolved.SettingsIndex = SettingsIndex;
			Resolved.Weight = Entry.Weight;
			Resolved.bOverrideScale = Entry.bOverrideScale;
			Resolved.MinScale = Entry.MinScale;
			Resolved.MaxScale = FMath::Max(Entry.MinScale, Entry.MaxScale);

			TArray<FForestMeshInstance> MeshInstances;
			double MeasuredRadius = ForestBiomePCGConstants::DefaultPropRadius;
			FBox LocalBounds(EForceInit::ForceInit);
			const bool bNeedsInspection = bExtractMeshes || OutLocalBounds != nullptr || Entry.RadiusOverride <= 0.0f;
			AActor* InspectionActor = bNeedsInspection ? SpawnTransientInspectionActor(World, *PropClass) : nullptr;
			if (IsValid(InspectionActor))
			{
				LocalBounds = ComputeInspectionActorLocalBounds(*InspectionActor);
				if (LocalBounds.IsValid)
				{
					MeasuredRadius = RadiusFromBounds(LocalBounds);
				}
				if (bExtractMeshes)
				{
					ExtractStaticMeshInstances(*InspectionActor, MeshInstances);
				}
				InspectionActor->Destroy();
			}
			Resolved.Radius = Entry.RadiusOverride > 0.0f ? Entry.RadiusOverride : MeasuredRadius;

			if (bExtractMeshes && MeshInstances.IsEmpty())
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context,
					LOCTEXT("BushNoMeshes", "CreateForestBiome: a bush Blueprint has no static meshes to instance; spawning it as an actor instead."));
			}

			OutResolved.Add(Resolved);
			OutClasses.Add(PropClass);
			if (OutLocalBounds != nullptr)
			{
				if (not LocalBounds.IsValid)
				{
					const FVector FallbackExtent(Resolved.Radius);
					LocalBounds = FBox(-FallbackExtent, FallbackExtent);
				}
				OutLocalBounds->Add(LocalBounds);
			}
			if (bExtractMeshes)
			{
				OutMeshInstances->Add(MoveTemp(MeshInstances));
			}
		}
	}

	void ResolveFoliage(
		FPCGContext* Context,
		const TArray<FForestFoliageEntry>& Entries,
		TArray<FForestResolvedFoliage>& OutResolved,
		TArray<FSoftObjectPath>& OutMeshPaths)
	{
		for (int32 SettingsIndex = 0; SettingsIndex < Entries.Num(); ++SettingsIndex)
		{
			const FForestFoliageEntry& Entry = Entries[SettingsIndex];
			const UStaticMesh* Mesh = Entry.Mesh.LoadSynchronous();
			if (Mesh == nullptr)
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context,
					LOCTEXT("NullFoliageMesh", "CreateForestBiome: a foliage entry has no valid mesh and was skipped."));
				continue;
			}

			FForestResolvedFoliage Resolved;
			Resolved.SettingsIndex = SettingsIndex;
			Resolved.Weight = Entry.Weight;
			Resolved.MinScale = Entry.MinScale;
			Resolved.MaxScale = FMath::Max(Entry.MinScale, Entry.MaxScale);

			const FVector Extent = Mesh->GetBoundingBox().GetExtent();
			Resolved.Radius = FMath::Max(ForestBiomePCGConstants::MinFoliageRadius, FMath::Max(Extent.X, Extent.Y));

			OutResolved.Add(Resolved);
			OutMeshPaths.Add(Entry.Mesh.ToSoftObjectPath());
		}
	}

	void ResolveDecals(
		FPCGContext* Context,
		const TArray<FForestDecalEntry>& Entries,
		TArray<FForestResolvedDecal>& OutResolved,
		TArray<UMaterialInterface*>& OutMaterials)
	{
		for (int32 SettingsIndex = 0; SettingsIndex < Entries.Num(); ++SettingsIndex)
		{
			const FForestDecalEntry& Entry = Entries[SettingsIndex];
			UMaterialInterface* Material = Entry.Material.LoadSynchronous();
			if (Material == nullptr)
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context,
					LOCTEXT("NullDecalMaterial", "CreateForestBiome: a decal entry has no valid material and was skipped."));
				continue;
			}

			FForestResolvedDecal& Resolved = OutResolved.Emplace_GetRef();
			Resolved.SettingsIndex = SettingsIndex;
			Resolved.Weight = Entry.Weight;
			Resolved.DecalSize = FVector(
				FMath::Max(ForestBiomePCGConstants::MinDecalDimension, Entry.DecalSize.X),
				FMath::Max(ForestBiomePCGConstants::MinDecalDimension, Entry.DecalSize.Y),
				FMath::Max(ForestBiomePCGConstants::MinDecalDimension, Entry.DecalSize.Z));
			OutMaterials.Add(Material);
		}
	}

	float IsolationMultiplierForSize(const UPCGCreateForestBiomeSettings* Settings, const EForestAuxiliarySize Size)
	{
		switch (Size)
		{
			case EForestAuxiliarySize::Small:
				return Settings->SmallAuxiliaryIsolationMultiplier;
			case EForestAuxiliarySize::Medium:
				return Settings->MediumAuxiliaryIsolationMultiplier;
			case EForestAuxiliarySize::Large:
				return Settings->LargeAuxiliaryIsolationMultiplier;
			default:
				return 1.0f;
		}
	}

	void ResolveAuxiliaries(
		FPCGContext* Context,
		const UPCGCreateForestBiomeSettings* Settings,
		UWorld& World,
		TArray<FForestResolvedAuxiliary>& OutResolved,
		FForestBiomeAssets& Assets)
	{
		for (int32 SettingsIndex = 0; SettingsIndex < Settings->Auxiliaries.Num(); ++SettingsIndex)
		{
			const FForestAuxiliaryEntry& Entry = Settings->Auxiliaries[SettingsIndex];
			UClass* AuxiliaryClass = Entry.AuxiliaryClass.LoadSynchronous();
			if (AuxiliaryClass == nullptr)
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context,
					LOCTEXT("NullAuxClass", "CreateForestBiome: an auxiliary entry has no valid class and was skipped."));
				continue;
			}

			FForestResolvedAuxiliary Resolved;
			Resolved.SettingsIndex = SettingsIndex;
			Resolved.Weight = Entry.Weight;
			Resolved.Size = Entry.Size;
			Resolved.bOverrideScale = Entry.bOverrideScale;
			Resolved.MinScale = Entry.MinScale;
			Resolved.MaxScale = FMath::Max(Entry.MinScale, Entry.MaxScale);
			Resolved.IsolationDistance =
				Entry.PreferredDistanceFromOtherAuxiliaries * IsolationMultiplierForSize(Settings, Entry.Size);

			const bool bInstanced = Entry.SpawnType == EForestAuxiliarySpawnType::Instanced;
			TArray<FForestMeshInstance> MeshInstances;
			double MeasuredRadius = ForestBiomePCGConstants::DefaultAuxiliaryRadius;
			const bool bNeedsInspection = bInstanced || Entry.RadiusOverride <= 0.0f;
			AActor* InspectionActor = bNeedsInspection ? SpawnTransientInspectionActor(World, *AuxiliaryClass) : nullptr;
			if (IsValid(InspectionActor))
			{
				const FBox LocalBounds = ComputeInspectionActorLocalBounds(*InspectionActor);
				if (LocalBounds.IsValid)
				{
					MeasuredRadius = RadiusFromBounds(LocalBounds);
				}
				if (bInstanced)
				{
					ExtractStaticMeshInstances(*InspectionActor, MeshInstances);
				}
				InspectionActor->Destroy();
			}
			Resolved.Radius = Entry.RadiusOverride > 0.0f ? Entry.RadiusOverride : MeasuredRadius;

			if (bInstanced && MeshInstances.IsEmpty())
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context,
					LOCTEXT("AuxNoMeshes", "CreateForestBiome: an instanced auxiliary has no static meshes; spawning it as an actor instead."));
			}

			OutResolved.Add(Resolved);
			Assets.AuxiliaryClasses.Add(AuxiliaryClass);
			Assets.AuxiliarySpawnTypes.Add(Entry.SpawnType);
			Assets.AuxiliaryMeshInstances.Add(MoveTemp(MeshInstances));
		}
	}

	// --- Area & exclusion tests (biome-local space) ---

	bool IsPointInPolygon(const TArray<FVector2D>& Polygon, const FVector2D& Position);

	/**
	 * @brief The world-space XY footprint of a single flat exclusion point. Point-data exclusions - most
	 * importantly the Radixite node's "Field Bounds" output - describe their excluded area as ground-level
	 * point bounds with no vertical thickness. UPCGSpatialData::SamplePoint cannot detect those (the biome
	 * samples at its own Z, which never intersects a zero-height bounds), so their footprints are tested
	 * directly in 2D instead. Mirrors the point-bounds handling the Radixite node already does on its own
	 * Excluded Bounds input.
	 */
	struct FForestExclusionArea
	{
		// World-space rectangle corners: the point's local XY bounds transformed to world.
		TArray<FVector2D> Polygon;
		// World-space AABB of Polygon; a cheap broad-phase reject before the point-in-polygon test.
		FBox2D Bounds = FBox2D(ForceInit);
	};

	/** @brief Builds the world-space XY footprint of one exclusion point from its (possibly flat) bounds. */
	FForestExclusionArea MakeExclusionAreaFromPoint(const FPCGPoint& Point)
	{
		// A point that is degenerate on an axis (zero-size bounds) still excludes a small area around it.
		const bool bDegenerateX = FMath::IsNearlyEqual(Point.BoundsMin.X, Point.BoundsMax.X);
		const bool bDegenerateY = FMath::IsNearlyEqual(Point.BoundsMin.Y, Point.BoundsMax.Y);
		const double MinX = bDegenerateX ? Point.BoundsMin.X - ForestBiomePCGConstants::AreaSampleHalfExtent : Point.BoundsMin.X;
		const double MaxX = bDegenerateX ? Point.BoundsMax.X + ForestBiomePCGConstants::AreaSampleHalfExtent : Point.BoundsMax.X;
		const double MinY = bDegenerateY ? Point.BoundsMin.Y - ForestBiomePCGConstants::AreaSampleHalfExtent : Point.BoundsMin.Y;
		const double MaxY = bDegenerateY ? Point.BoundsMax.Y + ForestBiomePCGConstants::AreaSampleHalfExtent : Point.BoundsMax.Y;
		const FVector LocalCorners[] =
		{
			FVector(MinX, MinY, 0.0),
			FVector(MaxX, MinY, 0.0),
			FVector(MaxX, MaxY, 0.0),
			FVector(MinX, MaxY, 0.0)
		};

		FForestExclusionArea Area;
		Area.Polygon.Reserve(UE_ARRAY_COUNT(LocalCorners));
		for (const FVector& LocalCorner : LocalCorners)
		{
			const FVector2D WorldCorner(Point.Transform.TransformPosition(LocalCorner));
			Area.Polygon.Add(WorldCorner);
			Area.Bounds += WorldCorner;
		}
		return Area;
	}

	TFunction<bool(const FVector2D&)> BuildExclusionFunction(FPCGContext* Context, const FTransform& BiomeToWorld)
	{
		// Volumetric / surface / spline exclusions are density-sampled as before; flat point-bounds
		// exclusions (which SamplePoint cannot detect) are matched against their 2D footprint instead.
		TArray<const UPCGSpatialData*> ExclusionSpatialData;
		TArray<FForestExclusionArea> ExclusionAreas;
		const TArray<FPCGTaggedData> Inputs =
			Context->InputData.GetInputsByPin(ForestBiomePCGConstants::ExclusionPinLabel);
		for (const FPCGTaggedData& Input : Inputs)
		{
			const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data);
			if (SpatialData == nullptr)
			{
				continue;
			}
			if (const UPCGPointData* PointData = Cast<UPCGPointData>(SpatialData))
			{
				for (const FPCGPoint& Point : PointData->GetPoints())
				{
					ExclusionAreas.Add(MakeExclusionAreaFromPoint(Point));
				}
			}
			else
			{
				ExclusionSpatialData.Add(SpatialData);
			}
		}

		if (ExclusionSpatialData.IsEmpty() && ExclusionAreas.IsEmpty())
		{
			return nullptr;
		}

		return [ExclusionSpatialData = MoveTemp(ExclusionSpatialData), ExclusionAreas = MoveTemp(ExclusionAreas), BiomeToWorld](const FVector2D& LocalPosition) -> bool
		{
			const FVector WorldPosition = BiomeToWorld.TransformPosition(FVector(LocalPosition, 0.0));

			// Flat point-bounds exclusions (e.g. radixite field bounds): 2D footprint test, Z-independent.
			const FVector2D WorldXY(WorldPosition);
			for (const FForestExclusionArea& Area : ExclusionAreas)
			{
				if (Area.Bounds.bIsValid && Area.Bounds.Min.X <= WorldXY.X && WorldXY.X <= Area.Bounds.Max.X
					&& Area.Bounds.Min.Y <= WorldXY.Y && WorldXY.Y <= Area.Bounds.Max.Y
					&& IsPointInPolygon(Area.Polygon, WorldXY))
				{
					return true;
				}
			}
			const FBox SampleBounds(
				FVector(-ForestBiomePCGConstants::AreaSampleHalfExtent),
				FVector(ForestBiomePCGConstants::AreaSampleHalfExtent));
			for (const UPCGSpatialData* SpatialData : ExclusionSpatialData)
			{
				FPCGPoint SampledPoint;
				if (SpatialData->SamplePoint(FTransform(WorldPosition), SampleBounds, SampledPoint, nullptr)
					&& SampledPoint.Density > 0.0f)
				{
					return true;
				}
			}
			return false;
		};
	}

	/** @brief Even-odd ray-crossing test; the polygon is implicitly closed. */
	bool IsPointInPolygon(const TArray<FVector2D>& Polygon, const FVector2D& Position)
	{
		bool bInside = false;
		for (int32 CurrentIndex = 0, PreviousIndex = Polygon.Num() - 1;
			CurrentIndex < Polygon.Num();
			PreviousIndex = CurrentIndex++)
		{
			const FVector2D& VertexA = Polygon[CurrentIndex];
			const FVector2D& VertexB = Polygon[PreviousIndex];

			const bool bCrossesRay = (VertexA.Y > Position.Y) != (VertexB.Y > Position.Y);
			if (bCrossesRay
				&& Position.X < (VertexB.X - VertexA.X) * (Position.Y - VertexA.Y) / (VertexB.Y - VertexA.Y) + VertexA.X)
			{
				bInside = not bInside;
			}
		}
		return bInside;
	}

	/**
	 * @brief Builds the biome-local area test. Spline loops are sampled into a 2D polygon for fast
	 * point-in-polygon checks; other spatial data falls back to density sampling.
	 */
	TFunction<bool(const FVector2D&)> BuildAreaFunction(const UPCGSpatialData* AreaData, const FTransform& BiomeToWorld)
	{
		if (const UPCGPolyLineData* PolyLine = Cast<UPCGPolyLineData>(AreaData))
		{
			TArray<FVector2D> Polygon;
			for (int32 SegmentIndex = 0; SegmentIndex < PolyLine->GetNumSegments(); ++SegmentIndex)
			{
				const double SegmentLength = PolyLine->GetSegmentLength(SegmentIndex);
				const int32 NumSamples =
					FMath::Clamp(FMath::CeilToInt32(SegmentLength / ForestBiomePCGConstants::SplineSampleStep), 2, 128);
				for (int32 SampleIndex = 0; SampleIndex < NumSamples; ++SampleIndex)
				{
					const double Distance = SegmentLength * SampleIndex / NumSamples;
					const FTransform SampleTransform = PolyLine->GetTransformAtDistance(SegmentIndex, Distance, true);
					const FVector LocalPosition = BiomeToWorld.InverseTransformPosition(SampleTransform.GetLocation());
					Polygon.Add(FVector2D(LocalPosition.X, LocalPosition.Y));
				}
			}

			if (Polygon.Num() >= 3)
			{
				return [Polygon = MoveTemp(Polygon)](const FVector2D& LocalPosition) -> bool
				{
					return IsPointInPolygon(Polygon, LocalPosition);
				};
			}
		}

		// Generic spatial data (volumes, surfaces, unions...): sample the density field.
		return [AreaData, BiomeToWorld](const FVector2D& LocalPosition) -> bool
		{
			const FVector WorldPosition = BiomeToWorld.TransformPosition(FVector(LocalPosition, 0.0));
			const FBox SampleBounds(
				FVector(-ForestBiomePCGConstants::AreaSampleHalfExtent),
				FVector(ForestBiomePCGConstants::AreaSampleHalfExtent));
			FPCGPoint SampledPoint;
			return AreaData->SamplePoint(FTransform(WorldPosition), SampleBounds, SampledPoint, nullptr)
				&& SampledPoint.Density > 0.0f;
		};
	}
}

// ---------------------------------------------------------------------------
// Spawning
// ---------------------------------------------------------------------------

namespace
{
	/** @brief World position and surface normal of a ground projection. */
	struct FForestGroundHit
	{
		FVector Position = FVector::ZeroVector;
		FVector Normal = FVector::UpVector;
	};

	AActor* SpawnManagedEmptyActor(UWorld& World, const FTransform& Transform, const FString& Label)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AActor* Actor = World.SpawnActor<AActor>(AActor::StaticClass(), Transform, SpawnParams);
		if (not IsValid(Actor))
		{
			return nullptr;
		}

		Actor->Tags.Add(PCGHelpers::DefaultPCGActorTag);
#if WITH_EDITOR
		Actor->SetActorLabel(Label);
#endif
		return Actor;
	}

	FCollisionQueryParams MakeGroundTraceParams(const TArray<AActor*>& ActorsToIgnore)
	{
		FCollisionQueryParams TraceParams;
		TraceParams.AddIgnoredActors(ActorsToIgnore);
		return TraceParams;
	}

	FForestGroundHit ProjectToGround(UWorld& World, const FVector& WorldPosition, const FCollisionQueryParams& TraceParams)
	{
		FHitResult GroundHit;
		const FVector TraceStart = WorldPosition + FVector::UpVector * ForestBiomePCGConstants::GroundTraceUp;
		const FVector TraceEnd = WorldPosition - FVector::UpVector * ForestBiomePCGConstants::GroundTraceDown;
		// Trace the landscape-only channel, not ECC_WorldStatic: props (including trees) ignore it, so
		// a placement can only ever land on the terrain. This is what keeps regeneration correct -
		// on a force rebuild the previous generation's tree actors are still collision-active when the
		// new traces run, and tracing WorldStatic would land new trees on top of those leftover canopies.
		if (World.LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, COLLISION_TRACE_LANDSCAPE, TraceParams))
		{
			return FForestGroundHit{GroundHit.ImpactPoint, GroundHit.ImpactNormal};
		}
		return FForestGroundHit{WorldPosition, FVector::UpVector};
	}

	/** @brief Yaw rotation, optionally tilted to sit flush on the ground surface normal. */
	FQuat MakeSpawnRotation(const double YawRadians, const FVector& GroundNormal, const bool bAlignToNormal)
	{
		FQuat Rotation(FVector::UpVector, YawRadians);
		if (bAlignToNormal)
		{
			Rotation = FQuat::FindBetweenNormals(FVector::UpVector, GroundNormal) * Rotation;
		}
		return Rotation;
	}

	AActor* SpawnManagedActor(UWorld& World, UClass* ActorClass, const FTransform& Transform, TArray<AActor*>& OutSpawnedActors)
	{
		if (ActorClass == nullptr)
		{
			return nullptr;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AActor* Actor = World.SpawnActor<AActor>(ActorClass, Transform, SpawnParams);
		if (not IsValid(Actor))
		{
			return nullptr;
		}
		Actor->Tags.Add(PCGHelpers::DefaultPCGActorTag);
		OutSpawnedActors.Add(Actor);
		return Actor;
	}

	/** @return The spawned Blueprint actor, or nullptr when the class was invalid or spawning failed. */
	AActor* SpawnGroundActor(
		UWorld& World,
		UClass* ActorClass,
		const FForestPlacement& Placement,
		const FTransform& BiomeToWorld,
		const bool bAlignToNormal,
		TArray<AActor*>& OutSpawnedActors)
	{
		if (ActorClass == nullptr)
		{
			return nullptr;
		}

		const FCollisionQueryParams TraceParams = MakeGroundTraceParams(OutSpawnedActors);
		const FVector WorldPosition = BiomeToWorld.TransformPosition(FVector(Placement.Position, 0.0));
		const FForestGroundHit Ground = ProjectToGround(World, WorldPosition, TraceParams);
		const FQuat Rotation = MakeSpawnRotation(Placement.YawRadians, Ground.Normal, bAlignToNormal);

		return SpawnManagedActor(
			World, ActorClass, FTransform(Rotation, Ground.Position, FVector(Placement.UniformScale)), OutSpawnedActors);
	}

	/** @brief Tree output data captured from a successfully spawned tree actor. */
	struct FForestSpawnedTree
	{
		FTransform Transform;
		FBox LocalBounds = FBox(EForceInit::ForceInit);
	};

	/** @brief Spawns a solid-prop category (large or regular trees) as upright standalone actors. */
	void SpawnTreeActors(
		UWorld& World,
		const TArray<FForestPlacement>& Placements,
		const TArray<UClass*>& Classes,
		const TArray<FBox>& LocalBounds,
		const FTransform& BiomeToWorld,
		TArray<AActor*>& OutSpawnedActors,
		TArray<FForestSpawnedTree>& OutSpawnedTrees)
	{
		for (const FForestPlacement& Placement : Placements)
		{
			if (not Classes.IsValidIndex(Placement.EntryIndex) || not LocalBounds.IsValidIndex(Placement.EntryIndex))
			{
				continue;
			}
			// Trees stay vertical; only ground-projected on Z.
			AActor* SpawnedTree = SpawnGroundActor(
				World, Classes[Placement.EntryIndex], Placement, BiomeToWorld, false, OutSpawnedActors);
			if (not IsValid(SpawnedTree))
			{
				continue;
			}

			FForestSpawnedTree& TreeOutput = OutSpawnedTrees.Emplace_GetRef();
			TreeOutput.Transform = SpawnedTree->GetActorTransform();
			TreeOutput.LocalBounds = LocalBounds[Placement.EntryIndex];
		}
	}

	/**
	 * @brief Ground-projects each instanced placement, batches its extracted meshes per unique mesh
	 * (world transforms), and routes placements whose Blueprint had no meshes to a fallback actor list.
	 */
	void GatherInstancedTransforms(
		UWorld& World,
		const TArray<FForestPlacement>& Placements,
		const TArray<TArray<FForestMeshInstance>>& MeshInstancesPerEntry,
		const TArray<UClass*>& FallbackClasses,
		const FTransform& BiomeToWorld,
		const FCollisionQueryParams& TraceParams,
		TMap<UStaticMesh*, TArray<FTransform>>& OutMeshToTransforms,
		TArray<TPair<UClass*, FTransform>>& OutFallbackActors)
	{
		for (const FForestPlacement& Placement : Placements)
		{
			if (not MeshInstancesPerEntry.IsValidIndex(Placement.EntryIndex))
			{
				continue;
			}

			const FVector WorldPosition = BiomeToWorld.TransformPosition(FVector(Placement.Position, 0.0));
			const FForestGroundHit Ground = ProjectToGround(World, WorldPosition, TraceParams);
			const FQuat Rotation = MakeSpawnRotation(Placement.YawRadians, Ground.Normal, true);
			const FTransform PivotTransform(Rotation, Ground.Position, FVector(Placement.UniformScale));

			const TArray<FForestMeshInstance>& MeshInstances = MeshInstancesPerEntry[Placement.EntryIndex];
			if (MeshInstances.IsEmpty())
			{
				if (FallbackClasses.IsValidIndex(Placement.EntryIndex) && FallbackClasses[Placement.EntryIndex] != nullptr)
				{
					OutFallbackActors.Emplace(FallbackClasses[Placement.EntryIndex], PivotTransform);
				}
				continue;
			}

			for (const FForestMeshInstance& MeshInstance : MeshInstances)
			{
				OutMeshToTransforms.FindOrAdd(MeshInstance.Mesh).Add(MeshInstance.RelativeTransform * PivotTransform);
			}
		}
	}

	/** @brief One anchor actor holding a Hierarchical ISM component per unique mesh (bulk world instances). */
	void BuildInstancedAnchor(
		UWorld& World,
		TMap<UStaticMesh*, TArray<FTransform>>& MeshToTransforms,
		const FVector& AnchorLocation,
		const FString& Label,
		TArray<AActor*>& OutSpawnedActors)
	{
		if (MeshToTransforms.IsEmpty())
		{
			return;
		}

		AActor* Anchor = SpawnManagedEmptyActor(World, FTransform(AnchorLocation), Label);
		if (Anchor == nullptr)
		{
			return;
		}
		OutSpawnedActors.Add(Anchor);

		USceneComponent* Root = NewObject<USceneComponent>(Anchor);
		Anchor->SetRootComponent(Root);
		Anchor->AddInstanceComponent(Root);
		Root->RegisterComponent();

		for (TPair<UStaticMesh*, TArray<FTransform>>& MeshEntry : MeshToTransforms)
		{
			UHierarchicalInstancedStaticMeshComponent* InstancedMeshes =
				NewObject<UHierarchicalInstancedStaticMeshComponent>(Anchor);
			InstancedMeshes->SetMobility(EComponentMobility::Static);
			InstancedMeshes->SetStaticMesh(MeshEntry.Key);
			InstancedMeshes->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
			Anchor->AddInstanceComponent(InstancedMeshes);
			InstancedMeshes->RegisterComponent();
			// One bulk add per mesh (world space) so the HISM cluster tree builds once.
			InstancedMeshes->AddInstances(MeshEntry.Value, false, true);
		}
	}

	/**
	 * @brief Spawns an instanced category (bushes or instanced auxiliaries): meshes are batched into
	 * a shared HISM anchor, and any placement whose Blueprint had no meshes spawns as its actor.
	 */
	void SpawnInstancedPlacements(
		UWorld& World,
		const TArray<FForestPlacement>& Placements,
		const TArray<TArray<FForestMeshInstance>>& MeshInstancesPerEntry,
		const TArray<UClass*>& FallbackClasses,
		const FTransform& BiomeToWorld,
		const FString& AnchorLabel,
		TArray<AActor*>& OutSpawnedActors)
	{
		if (Placements.IsEmpty())
		{
			return;
		}

		const FCollisionQueryParams TraceParams = MakeGroundTraceParams(OutSpawnedActors);
		TMap<UStaticMesh*, TArray<FTransform>> MeshToTransforms;
		TArray<TPair<UClass*, FTransform>> FallbackActors;
		GatherInstancedTransforms(
			World, Placements, MeshInstancesPerEntry, FallbackClasses, BiomeToWorld, TraceParams,
			MeshToTransforms, FallbackActors);

		BuildInstancedAnchor(World, MeshToTransforms, BiomeToWorld.GetLocation(), AnchorLabel, OutSpawnedActors);

		for (const TPair<UClass*, FTransform>& Fallback : FallbackActors)
		{
			SpawnManagedActor(World, Fallback.Key, Fallback.Value, OutSpawnedActors);
		}
	}

	/** @brief Splits auxiliaries into instanced and Blueprint-actor placements and spawns each set. */
	void SpawnAuxiliaries(
		UWorld& World,
		const TArray<FForestPlacement>& Placements,
		const FForestBiomeAssets& Assets,
		const FTransform& BiomeToWorld,
		TArray<AActor*>& OutSpawnedActors)
	{
		TArray<FForestPlacement> InstancedPlacements;
		TArray<FForestPlacement> BlueprintPlacements;
		for (const FForestPlacement& Placement : Placements)
		{
			if (not Assets.AuxiliarySpawnTypes.IsValidIndex(Placement.EntryIndex))
			{
				continue;
			}
			if (Assets.AuxiliarySpawnTypes[Placement.EntryIndex] == EForestAuxiliarySpawnType::Instanced)
			{
				InstancedPlacements.Add(Placement);
			}
			else
			{
				BlueprintPlacements.Add(Placement);
			}
		}

		SpawnInstancedPlacements(
			World, InstancedPlacements, Assets.AuxiliaryMeshInstances, Assets.AuxiliaryClasses, BiomeToWorld,
			TEXT("PCG_ForestAuxiliaries"), OutSpawnedActors);

		for (const FForestPlacement& Placement : BlueprintPlacements)
		{
			if (not Assets.AuxiliaryClasses.IsValidIndex(Placement.EntryIndex))
			{
				continue;
			}
			// Auxiliaries sit flush on slopes.
			SpawnGroundActor(World, Assets.AuxiliaryClasses[Placement.EntryIndex], Placement, BiomeToWorld, true, OutSpawnedActors);
		}
	}

	/** @brief Projects forest decals straight down onto the landscape and owns them on one PCG-managed actor. */
	void SpawnDecals(
		UWorld& World,
		const TArray<FForestDecalPlacement>& Placements,
		const TArray<UMaterialInterface*>& Materials,
		const FTransform& BiomeToWorld,
		TArray<AActor*>& OutSpawnedActors)
	{
		if (Placements.IsEmpty())
		{
			return;
		}

		AActor* Anchor = SpawnManagedEmptyActor(
			World, FTransform(BiomeToWorld.GetLocation()), TEXT("PCG_ForestDecals"));
		if (Anchor == nullptr)
		{
			return;
		}
		OutSpawnedActors.Add(Anchor);

		USceneComponent* Root = NewObject<USceneComponent>(Anchor);
		Anchor->SetRootComponent(Root);
		Anchor->AddInstanceComponent(Root);
		Root->RegisterComponent();

		const FCollisionQueryParams TraceParams = MakeGroundTraceParams(OutSpawnedActors);
		for (const FForestDecalPlacement& Placement : Placements)
		{
			if (not Materials.IsValidIndex(Placement.EntryIndex))
			{
				continue;
			}

			UMaterialInterface* Material = Materials[Placement.EntryIndex];
			if (Material == nullptr)
			{
				continue;
			}

			const FVector WorldPosition = BiomeToWorld.TransformPosition(FVector(Placement.Position, 0.0));
			const FForestGroundHit Ground = ProjectToGround(World, WorldPosition, TraceParams);
			const FRotator Rotation(-90.0, FMath::RadiansToDegrees(Placement.YawRadians), 0.0);

			UDecalComponent* DecalComponent = NewObject<UDecalComponent>(Anchor);
			DecalComponent->SetMobility(EComponentMobility::Static);
			DecalComponent->SetDecalMaterial(Material);
			DecalComponent->DecalSize = Placement.DecalSize;
			DecalComponent->SetWorldTransform(FTransform(Rotation, Ground.Position));
			DecalComponent->AttachToComponent(Root, FAttachmentTransformRules::KeepWorldTransform);
			Anchor->AddInstanceComponent(DecalComponent);
			DecalComponent->RegisterComponent();
		}
	}
}

// ---------------------------------------------------------------------------
// Point-data output
// ---------------------------------------------------------------------------

namespace
{
	UPCGPointData* CreateOutputPointData(FPCGContext* Context, const FName PinLabel)
	{
		UPCGPointData* PointData = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);
		PointData->InitializeFromData(nullptr);

		FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
		Output.Data = PointData;
		Output.Pin = PinLabel;
		return PointData;
	}

	/** @brief Emits foliage points (with a 'Mesh' attribute), ground-projected and slope-aligned. */
	void EmitFoliageOutput(
		FPCGContext* Context,
		UWorld& World,
		const FForestBiomeGenResult& Result,
		const FForestBiomeAssets& Assets,
		const FTransform& BiomeToWorld,
		const int32 RandomSeed,
		const TArray<AActor*>& ActorsToIgnoreInTraces)
	{
		using namespace ForestBiomePCGConstants;

		UPCGPointData* FoliageData = CreateOutputPointData(Context, FoliagePinLabel);
		FPCGMetadataAttribute<FSoftObjectPath>* MeshAttribute =
			FoliageData->Metadata->CreateAttribute<FSoftObjectPath>(FoliageMeshAttributeName, FSoftObjectPath(), false, false);

		const FCollisionQueryParams TraceParams = MakeGroundTraceParams(ActorsToIgnoreInTraces);
		TArray<FPCGPoint>& Points = FoliageData->GetMutablePoints();
		Points.Reserve(Result.Foliage.Num());

		for (int32 FoliageIndex = 0; FoliageIndex < Result.Foliage.Num(); ++FoliageIndex)
		{
			const FForestPlacement& Placement = Result.Foliage[FoliageIndex];
			if (not Assets.FoliageMeshPaths.IsValidIndex(Placement.EntryIndex))
			{
				continue;
			}

			const FVector WorldPosition = BiomeToWorld.TransformPosition(FVector(Placement.Position, 0.0));
			const FForestGroundHit Ground = ProjectToGround(World, WorldPosition, TraceParams);
			const FQuat Rotation = MakeSpawnRotation(Placement.YawRadians, Ground.Normal, true);

			FPCGPoint& Point = Points.Emplace_GetRef();
			Point.Transform = FTransform(Rotation, Ground.Position, FVector(Placement.UniformScale));
			Point.Density = 1.0f;
			Point.Seed = PCGHelpers::ComputeSeed(RandomSeed, FoliageIndex);

			FoliageData->Metadata->InitializeOnSet(Point.MetadataEntry);
			MeshAttribute->SetValue(Point.MetadataEntry, Assets.FoliageMeshPaths[Placement.EntryIndex]);
		}
	}

	/** @brief Emits one point per spawned tree, preserving its actual transform and measured bounds. */
	void EmitTreesOutput(
		FPCGContext* Context,
		const TArray<FForestSpawnedTree>& SpawnedTrees,
		const int32 RandomSeed)
	{
		using namespace ForestBiomePCGConstants;

		UPCGPointData* TreesData = CreateOutputPointData(Context, TreesPinLabel);
		TArray<FPCGPoint>& Points = TreesData->GetMutablePoints();
		Points.Reserve(SpawnedTrees.Num());

		for (int32 TreeIndex = 0; TreeIndex < SpawnedTrees.Num(); ++TreeIndex)
		{
			const FForestSpawnedTree& SpawnedTree = SpawnedTrees[TreeIndex];

			FPCGPoint& Point = Points.Emplace_GetRef();
			Point.Transform = SpawnedTree.Transform;
			Point.BoundsMin = SpawnedTree.LocalBounds.Min;
			Point.BoundsMax = SpawnedTree.LocalBounds.Max;
			Point.Density = 1.0f;
			Point.Seed = PCGHelpers::ComputeSeed(RandomSeed, TreeIndex);
		}
	}
}

// ---------------------------------------------------------------------------
// Element
// ---------------------------------------------------------------------------

namespace
{
	/**
	 * @brief Extracts the biome frame from the first spatial input: the area's bounds center becomes
	 * the biome origin and the bounds size becomes the biome rectangle.
	 */
	bool TryGetBiomeArea(
		FPCGContext* Context,
		FTransform& OutBiomeToWorld,
		FVector2D& OutBiomeLengths,
		const UPCGSpatialData*& OutAreaData)
	{
		const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
		for (const FPCGTaggedData& Input : Inputs)
		{
			const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data);
			if (SpatialData == nullptr)
			{
				continue;
			}

			const FBox Bounds = SpatialData->GetBounds();
			if (not Bounds.IsValid)
			{
				continue;
			}

			OutBiomeToWorld = FTransform(Bounds.GetCenter());
			OutBiomeLengths = FVector2D(Bounds.GetSize().X, Bounds.GetSize().Y);
			OutAreaData = SpatialData;

			if (Inputs.Num() > 1)
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context,
					LOCTEXT("MultipleAreas", "CreateForestBiome: multiple input data; only the first spatial data is used as the spawn volume."));
			}
			return true;
		}
		return false;
	}

	void FillScalarParams(const UPCGCreateForestBiomeSettings* Settings, FForestBiomeGenParams& Params)
	{
		Params.RandomSeed = Settings->RandomSeed;
		Params.LargeTreesPer1000 = Settings->LargeTreesPer1000Units;
		Params.RegularTreesPer1000 = Settings->RegularTreesPer1000Units;
		Params.BushesPer1000 = Settings->BushesPer1000Units;
		Params.FoliagePer1000 = Settings->FoliagePer1000Units;
		Params.DecalsPer1000 = Settings->DecalsPer1000Units;
		Params.AuxiliariesPer1000 = Settings->AuxiliariesPer1000Units;
		Params.GlobalDensityMultiplier = Settings->GlobalDensityMultiplier;
		Params.PropSpacing = Settings->PropSpacing;
		Params.MinGlobalDecalSizeMultiplier = Settings->MinGlobalDecalSizeMultiplier;
		Params.MaxGlobalDecalSizeMultiplier = Settings->MaxGlobalDecalSizeMultiplier;
	}

	void ResolveAllAssets(
		FPCGContext* Context,
		const UPCGCreateForestBiomeSettings* Settings,
		UWorld& World,
		FForestBiomeGenParams& Params,
		FForestBiomeAssets& Assets)
	{
		ResolveProps(
			Context, World, Settings->LargeTrees, Params.LargeTrees, Assets.LargeTreeClasses, nullptr,
			&Assets.LargeTreeLocalBounds);
		ResolveProps(
			Context, World, Settings->RegularTrees, Params.RegularTrees, Assets.RegularTreeClasses, nullptr,
			&Assets.RegularTreeLocalBounds);
		ResolveProps(Context, World, Settings->Bushes, Params.Bushes, Assets.BushClasses, &Assets.BushMeshInstances, nullptr);
		ResolveFoliage(Context, Settings->Foliage, Params.Foliage, Assets.FoliageMeshPaths);
		ResolveDecals(Context, Settings->Decals, Params.Decals, Assets.DecalMaterials);
		ResolveAuxiliaries(Context, Settings, World, Params.Auxiliaries, Assets);
	}

	void SpawnAllCategories(
		UWorld& World,
		const FForestBiomeGenResult& Result,
		const FForestBiomeAssets& Assets,
		const FTransform& BiomeToWorld,
		TArray<AActor*>& OutSpawnedActors,
		TArray<FForestSpawnedTree>& OutSpawnedTrees)
	{
		SpawnTreeActors(
			World, Result.LargeTrees, Assets.LargeTreeClasses, Assets.LargeTreeLocalBounds, BiomeToWorld,
			OutSpawnedActors, OutSpawnedTrees);
		SpawnTreeActors(
			World, Result.RegularTrees, Assets.RegularTreeClasses, Assets.RegularTreeLocalBounds, BiomeToWorld,
			OutSpawnedActors, OutSpawnedTrees);
		SpawnInstancedPlacements(
			World, Result.Bushes, Assets.BushMeshInstances, Assets.BushClasses, BiomeToWorld,
			TEXT("PCG_ForestBushes"), OutSpawnedActors);
		SpawnDecals(World, Result.Decals, Assets.DecalMaterials, BiomeToWorld, OutSpawnedActors);
		SpawnAuxiliaries(World, Result.Auxiliaries, Assets, BiomeToWorld, OutSpawnedActors);
	}

	void RegisterManagedActors(UPCGComponent* SourceComponent, const TArray<AActor*>& SpawnedActors)
	{
		if (SpawnedActors.IsEmpty())
		{
			return;
		}

		UPCGManagedActors* ManagedActors = NewObject<UPCGManagedActors>(SourceComponent);
		for (AActor* SpawnedActor : SpawnedActors)
		{
			ManagedActors->GeneratedActors.Add(SpawnedActor);
		}
		SourceComponent->AddToManagedResources(ManagedActors);
	}
}

bool FPCGCreateForestBiomeElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);

	const UPCGCreateForestBiomeSettings* Settings = Context->GetInputSettings<UPCGCreateForestBiomeSettings>();
	UPCGComponent* SourceComponent = Context->SourceComponent.Get();
	if (Settings == nullptr || not IsValid(SourceComponent) || not IsValid(SourceComponent->GetWorld()))
	{
		return true;
	}
	UWorld& World = *SourceComponent->GetWorld();

	FTransform BiomeToWorld;
	FVector2D BiomeLengths = FVector2D::ZeroVector;
	const UPCGSpatialData* AreaData = nullptr;
	if (not TryGetBiomeArea(Context, BiomeToWorld, BiomeLengths, AreaData))
	{
		PCGE_LOG(Warning, GraphAndLog, LOCTEXT("NoArea",
			"CreateForestBiome: no valid spatial input (expected a spawn volume, surface or closed spline loop)."));
		return true;
	}

	FForestBiomeGenParams Params;
	FForestBiomeAssets Assets;
	FillScalarParams(Settings, Params);
	Params.BiomeLengthX = BiomeLengths.X;
	Params.BiomeLengthY = BiomeLengths.Y;
	ResolveAllAssets(Context, Settings, World, Params, Assets);
	Params.IsExcluded = BuildExclusionFunction(Context, BiomeToWorld);
	Params.IsInsideArea = BuildAreaFunction(AreaData, BiomeToWorld);

	FForestBiomeGenResult Result;
	FForestBiomeGenerator Generator(Params);
	Generator.Generate(Result);

	TArray<AActor*> SpawnedActors;
	TArray<FForestSpawnedTree> SpawnedTrees;
	SpawnAllCategories(World, Result, Assets, BiomeToWorld, SpawnedActors, SpawnedTrees);
	RegisterManagedActors(SourceComponent, SpawnedActors);

	EmitTreesOutput(Context, SpawnedTrees, Settings->RandomSeed);
	EmitFoliageOutput(Context, World, Result, Assets, BiomeToWorld, Settings->RandomSeed, SpawnedActors);
	return true;
}

#undef LOCTEXT_NAMESPACE
